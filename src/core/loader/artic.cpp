// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/certificate.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/otp.h"
#include "core/file_sys/romfs_reader.h"
#include "core/file_sys/secure_value_backend_artic.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/hid/hid_user.h"
#include "core/hw/unique_data.h"
#include "core/loader/artic.h"
#include "core/loader/smdh.h"
#include "core/memory.h"
#include "core/system_titles.h"
#include "network/network.h"

namespace Loader {

using namespace Common::Literals;

Apploader_Artic::Apploader_Artic(Core::System& system_, const std::string& server_addr,
                                 u16 server_port, ArticInitMode init_mode)
    : AppLoader(system_, FileUtil::IOFile()) {
    client = std::make_shared<Network::ArticBase::Client>(server_addr, server_port);
    client->SetCommunicationErrorCallback([&system_](const std::string& msg) {
        system_.SetStatus(Core::System::ResultStatus::ErrorArticDisconnected,
                          msg.empty() ? nullptr : msg.c_str());
    });
    client->SetArticReportTrafficCallback(
        [&system_](u32 bytes) { system_.ReportArticTraffic(bytes); });
    client->SetReportArticEventCallback([&system_](u64 event) {
        Core::PerfStats::PerfArticEventBits ev =
            static_cast<Core::PerfStats::PerfArticEventBits>(event & 0xFFFFFFFF);
        bool set = (event > 32) != 0;
        system_.ReportPerfArticEvent(ev, set);
    });
    is_initial_setup = init_mode != ArticInitMode::NONE;
    artic_init_mode = init_mode;
}

Apploader_Artic::~Apploader_Artic() {
    // TODO(PabloMK7) Find memory leak that prevents the romfs readers being destroyed
    // when emulation stops. Looks like the mem leak comes from IVFCFile objects
    // not being destroyed...
    if (main_romfs_reader) {
        static_cast<FileSys::ArticRomFSReader*>(main_romfs_reader.get())->ClearCache();
        static_cast<FileSys::ArticRomFSReader*>(main_romfs_reader.get())->CloseFile();
        main_romfs_reader.reset();
    }
    if (update_romfs_reader) {
        static_cast<FileSys::ArticRomFSReader*>(update_romfs_reader.get())->ClearCache();
        static_cast<FileSys::ArticRomFSReader*>(update_romfs_reader.get())->CloseFile();
        update_romfs_reader.reset();
    }
    client->Stop();
}

FileType Apploader_Artic::IdentifyType(FileUtil::IOFile* file) {
    return FileType::ARTIC;
}

std::pair<std::optional<u32>, ResultStatus> Apploader_Artic::LoadCoreVersion() {
    if (!is_loaded) {
        bool success = LoadExheader();
        if (!success) {
            return std::make_pair(std::nullopt, ResultStatus::ErrorArtic);
        }
    }

    // Provide the core version from the exheader.
    auto& ncch_caps = program_exheader.arm11_system_local_caps;
    return std::make_pair(ncch_caps.core_version, ResultStatus::Success);
}

std::pair<std::optional<Kernel::MemoryMode>, ResultStatus> Apploader_Artic::LoadKernelMemoryMode() {
    if (!is_loaded) {
        bool success = LoadExheader();
        if (!success) {
            return std::make_pair(std::nullopt, ResultStatus::ErrorArtic);
        }
    }

    if (memory_mode_override.has_value()) {
        return std::make_pair(memory_mode_override, ResultStatus::Success);
    }

    // Provide the memory mode from the exheader.
    auto& ncch_caps = program_exheader.arm11_system_local_caps;
    auto mode = static_cast<Kernel::MemoryMode>(ncch_caps.system_mode.Value());
    return std::make_pair(mode, ResultStatus::Success);
}

std::pair<std::optional<Kernel::New3dsHwCapabilities>, ResultStatus>
Apploader_Artic::LoadNew3dsHwCapabilities() {
    if (!is_loaded) {
        bool success = LoadExheader();
        if (!success) {
            return std::make_pair(std::nullopt, ResultStatus::ErrorArtic);
        }
    }

    // Provide the capabilities from the exheader.
    auto& ncch_caps = program_exheader.arm11_system_local_caps;
    auto caps = Kernel::New3dsHwCapabilities{
        ncch_caps.enable_l2_cache != 0,
        ncch_caps.enable_804MHz_cpu != 0,
        static_cast<Kernel::New3dsMemoryMode>(ncch_caps.n3ds_mode),
    };
    return std::make_pair(std::move(caps), ResultStatus::Success);
}

ResultStatus Apploader_Artic::LoadExec(std::shared_ptr<Kernel::Process>& process) {

    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;

    std::vector<u8> code;
    u64_le program_id;
    if (ResultStatus::Success == ReadCode(code) &&
        ResultStatus::Success == ReadProgramId(program_id)) {
        return LoadExecImpl(process, program_id, program_exheader, code);
    }
    return ResultStatus::ErrorArtic;
}

ResultStatus Apploader_Artic::LoadExecImpl(std::shared_ptr<Kernel::Process>& process,
                                           u64_le program_id, const ExHeader_Header& exheader,
                                           std::vector<u8>& code) {
    using Kernel::CodeSet;

    std::string process_name =
        Common::StringFromFixedZeroTerminatedBuffer((const char*)exheader.codeset_info.name, 8);

    std::shared_ptr<CodeSet> codeset = system.Kernel().CreateCodeSet(process_name, program_id);

    codeset->CodeSegment().offset = 0;
    codeset->CodeSegment().addr = exheader.codeset_info.text.address;
    codeset->CodeSegment().size =
        exheader.codeset_info.text.num_max_pages * Memory::CITRA_PAGE_SIZE;

    codeset->RODataSegment().offset = codeset->CodeSegment().offset + codeset->CodeSegment().size;
    codeset->RODataSegment().addr = exheader.codeset_info.ro.address;
    codeset->RODataSegment().size =
        exheader.codeset_info.ro.num_max_pages * Memory::CITRA_PAGE_SIZE;

    // TODO(yuriks): Not sure if the bss size is added to the page-aligned .data size or just
    //               to the regular size. Playing it safe for now.
    u32 bss_page_size = (exheader.codeset_info.bss_size + 0xFFF) & ~0xFFF;
    code.resize(code.size() + bss_page_size, 0);

    codeset->DataSegment().offset = codeset->RODataSegment().offset + codeset->RODataSegment().size;
    codeset->DataSegment().addr = exheader.codeset_info.data.address;
    codeset->DataSegment().size =
        exheader.codeset_info.data.num_max_pages * Memory::CITRA_PAGE_SIZE + bss_page_size;

    // Apply patches now that the entire codeset (including .bss) has been allocated
    // const ResultStatus patch_result = overlay_ncch->ApplyCodePatch(code);
    // if (patch_result != ResultStatus::Success && patch_result != ResultStatus::ErrorNotUsed)
    //    return patch_result;

    codeset->entrypoint = codeset->CodeSegment().addr;
    codeset->memory = std::move(code);

    process = system.Kernel().CreateProcess(std::move(codeset));

    // Attach a resource limit to the process based on the resource limit category
    const auto category = static_cast<Kernel::ResourceLimitCategory>(
        exheader.arm11_system_local_caps.resource_limit_category);
    process->resource_limit = system.Kernel().ResourceLimit().GetForCategory(category);

    // When running N3DS-unaware titles pm will lie about the amount of memory available.
    // This means RESLIMIT_COMMIT = APPMEMALLOC doesn't correspond to the actual size of
    // APPLICATION. See:
    // https://github.com/LumaTeam/Luma3DS/blob/e2778a45/sysmodules/pm/source/launch.c#L237
    auto& ncch_caps = exheader.arm11_system_local_caps;
    const auto o3ds_mode = *LoadKernelMemoryMode().first;
    const auto n3ds_mode = static_cast<Kernel::New3dsMemoryMode>(ncch_caps.n3ds_mode);
    const bool is_new_3ds = Settings::values.is_new_3ds.GetValue();
    if (is_new_3ds && n3ds_mode == Kernel::New3dsMemoryMode::Legacy &&
        category == Kernel::ResourceLimitCategory::Application) {
        u64 new_limit = 0;
        switch (o3ds_mode) {
        case Kernel::MemoryMode::Prod:
            new_limit = 64_MiB;
            break;
        case Kernel::MemoryMode::Dev1:
            new_limit = 96_MiB;
            break;
        case Kernel::MemoryMode::Dev2:
            new_limit = 80_MiB;
            break;
        default:
            break;
        }
        process->resource_limit->SetLimitValue(Kernel::ResourceLimitType::Commit,
                                               static_cast<s32>(new_limit));
    }

    // Set the default CPU core for this process
    process->ideal_processor = exheader.arm11_system_local_caps.ideal_processor;

    // Copy data while converting endianness
    using KernelCaps = std::array<u32, ExHeader_ARM11_KernelCaps::NUM_DESCRIPTORS>;
    KernelCaps kernel_caps;
    std::copy_n(exheader.arm11_kernel_caps.descriptors, kernel_caps.size(), begin(kernel_caps));
    process->ParseKernelCaps(kernel_caps.data(), kernel_caps.size());

    s32 priority = exheader.arm11_system_local_caps.priority;
    u32 stack_size = exheader.codeset_info.stack_size;

    // On real HW this is done with FS:Reg, but we can be lazy
    auto fs_user = system.ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
    fs_user->RegisterProgramInfo(process->process_id, process->codeset->program_id, "articbase://");

    Service::FS::FS_USER::ProductInfo product_info{};
    if (LoadProductInfo(product_info) != ResultStatus::Success) {
        return ResultStatus::ErrorArtic;
    }
    fs_user->RegisterProductInfo(process->process_id, product_info);

    process->Run(priority, stack_size);
    return ResultStatus::Success;
}

void Apploader_Artic::ParseRegionLockoutInfo(u64 program_id) {
    if (Settings::values.region_value.GetValue() != Settings::REGION_VALUE_AUTO_SELECT) {
        return;
    }

    preferred_regions.clear();

    std::vector<u8> smdh_buffer;
    if (ReadIcon(smdh_buffer) == ResultStatus::Success && smdh_buffer.size() >= sizeof(SMDH)) {
        SMDH smdh;
        std::memcpy(&smdh, smdh_buffer.data(), sizeof(SMDH));
        u32 region_lockout = smdh.region_lockout;
        constexpr u32 REGION_COUNT = 7;
        for (u32 region = 0; region < REGION_COUNT; ++region) {
            if (region_lockout & 1) {
                preferred_regions.push_back(region);
            }
            region_lockout >>= 1;
        }
    } else {
        const auto region = Core::GetSystemTitleRegion(program_id);
        if (region.has_value()) {
            preferred_regions.push_back(region.value());
        }
    }
}

bool Apploader_Artic::LoadExheader() {
    if (program_exheader_loaded)
        return true;

    EnsureClientConnected();
    if (!client_connected)
        return false;

    auto req = client->NewRequest("Process_GetExheader");
    auto resp = client->Send(req);
    if (!resp.has_value())
        return false;

    auto exheader_buf = resp->GetResponseBuffer(0);
    if (!exheader_buf.has_value())
        return false;

    if (exheader_buf->second != sizeof(ExHeader_Header) - sizeof(ExHeader_Header::access_desc))
        return false;

    u8* prg_exh = reinterpret_cast<u8*>(&program_exheader);
    memcpy(prg_exh, exheader_buf->first,
           sizeof(ExHeader_Header) - sizeof(ExHeader_Header::access_desc));
    memcpy(prg_exh + offsetof(ExHeader_Header, access_desc.arm11_system_local_caps),
           reinterpret_cast<u8*>(exheader_buf->first) +
               offsetof(ExHeader_Header, arm11_system_local_caps),
           offsetof(ExHeader_Header, access_desc) -
               offsetof(ExHeader_Header, arm11_system_local_caps));
    program_exheader_loaded = true;
    return true;
}

ResultStatus Apploader_Artic::LoadProductInfo(Service::FS::FS_USER::ProductInfo& out_product_info) {
    if (cached_product_info.has_value()) {
        out_product_info = *cached_product_info;
        return ResultStatus::Success;
    }

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    auto req = client->NewRequest("Process_GetProductInfo");
    auto resp = client->Send(req);
    if (!resp.has_value())
        return ResultStatus::ErrorArtic;

    auto pinfo_buf = resp->GetResponseBuffer(0);
    if (!pinfo_buf.has_value() || pinfo_buf->second != sizeof(Service::FS::FS_USER::ProductInfo))
        return ResultStatus::ErrorArtic;

    out_product_info = *reinterpret_cast<Service::FS::FS_USER::ProductInfo*>(pinfo_buf->first);
    cached_product_info = out_product_info;

    return ResultStatus::Success;
}

void Apploader_Artic::EnsureClientConnected() {
    if (client_connected) {
        return;
    }
    client_connected = client->Connect();
    if (!client_connected) {
        return;
    }

    if (is_initial_setup) {
        // Ensure we are running the initial setup app in the correct version
        auto req = client->NewRequest("System_ArticSetupVersion");
        req.AddParameterU32(SETUP_TOOL_VERSION);
        auto resp = client->Send(req);
        if (!resp.has_value()) {
            client_connected = false;
            return;
        }

        auto ret_buf = resp->GetResponseBuffer(0);
        if (!ret_buf.has_value() || ret_buf->second != sizeof(u32)) {
            client_connected = false;
            return;
        }

        if (*reinterpret_cast<u32*>(ret_buf->first) != SETUP_TOOL_VERSION) {
            system.SetStatus(Core::System::ResultStatus::ErrorArticDisconnected,
                             "\nIncompatible Artic Setup Tool version.\nCheck for Artic Setup Tool "
                             "or Azahar updates.");
            client_connected = false;
            client->Stop();
        } else {
            client_connected = true;
        }
    }
}

ResultStatus Apploader_Artic::Load(std::shared_ptr<Kernel::Process>& process) {
    u64_le ncch_program_id;

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    ResultStatus result = ReadProgramId(ncch_program_id);
    if (result != ResultStatus::Success) {
        return result;
    }

    std::string program_id{fmt::format("{:016X}", ncch_program_id)};

    LOG_INFO(Loader, "Program ID: {}", program_id);

    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info;
        ReadTitle(game_info.name);
        game_info.id = ncch_program_id;
        room_member->SendGameInfo(game_info);
    }

    is_loaded = true; // Set state to loaded

    if (is_initial_setup) {

        // If there is already a console linked, check it's the same device.
        // Otherwise it could cause weird issues with account save data.
        if (HW::UniqueData::IsFullConsoleLinked()) {
            auto req = client->NewRequest("System_ReportDeviceID");
            req.AddParameterU32(HW::UniqueData::GetOTP().GetDeviceID());

            auto resp = client->Send(req);
            if (!resp.has_value() || !resp->Succeeded())
                return ResultStatus::ErrorArtic;

            if (resp->GetMethodResult() != 0)
                return ResultStatus::ErrorArtic;
        }

        auto cfg = system.ServiceManager().GetService<Service::CFG::CFG_U>("cfg:u");
        // Request console unique data
        for (int i = 0; i < 6; i++) {
            std::string path;
            std::size_t expected_size;

            switch (i) {
            case 0:
                path = HW::UniqueData::GetSecureInfoAPath();
                expected_size = sizeof(HW::UniqueData::SecureInfoA);
                break;
            case 1:
                path = HW::UniqueData::GetLocalFriendCodeSeedBPath();
                expected_size = sizeof(HW::UniqueData::LocalFriendCodeSeedB);
                break;
            case 2:
                path = HW::UniqueData::GetMovablePath();
                expected_size = sizeof(HW::UniqueData::MovableSedFull);
                break;
            case 3:
                path = HW::UniqueData::GetOTPPath();
                expected_size = sizeof(FileSys::OTP::OTPBin);
                break;
            case 4:
                expected_size = sizeof(u64) + sizeof(u32);
                break;
            case 5:
                expected_size = sizeof(u8) * 6;
                break;
            }

            auto req = client->NewRequest("System_GetSystemFile");
            req.AddParameterU8(static_cast<u8>(i));
            auto resp = client->Send(req);
            if (!resp.has_value() || !resp->Succeeded())
                return ResultStatus::ErrorArtic;

            if (resp->GetMethodResult() != 0)
                return ResultStatus::ErrorArtic;

            auto resp_buff = resp->GetResponseBuffer(0);
            if (!resp_buff.has_value() || resp_buff->second != expected_size) {
                if (resp_buff.has_value() && i == 2 &&
                    resp_buff->second == sizeof(HW::UniqueData::MovableSed)) {
                    // Account for uninitialized movable files
                    expected_size = sizeof(HW::UniqueData::MovableSed);
                } else {
                    return ResultStatus::ErrorArtic;
                }
            }

            if (i < 4) {
                FileUtil::CreateFullPath(path);
                FileUtil::IOFile out_file(path, "wb");
                if (!out_file.IsOpen()) {
                    return ResultStatus::ErrorArtic;
                }
                out_file.WriteBytes(reinterpret_cast<u8*>(resp_buff->first), resp_buff->second);
            } else if (i == 4) {
                u64 console_id;
                u32 random_id;
                memcpy(&console_id, resp_buff->first, sizeof(u64));
                memcpy(&random_id, reinterpret_cast<u8*>(resp_buff->first) + sizeof(u64),
                       sizeof(u32));
                if (cfg.get()) {
                    auto cfg_module = cfg->GetModule();
                    cfg_module->SetConsoleUniqueId(random_id, console_id);
                    cfg_module->UpdateConfigNANDSavegame();
                }
            } else if (i == 5) {
                std::array<u8, 6> mac;
                memcpy(mac.data(), resp_buff->first, mac.size());
                if (cfg.get()) {
                    auto cfg_module = cfg->GetModule();
                    cfg_module->GetMacAddress() = Service::CFG::MacToString(mac);
                    cfg_module->SaveMacAddress();
                }
                system.Kernel().GetSharedPageHandler().SetMacAddress(mac);
            }
        }

        HW::UniqueData::InvalidateSecureData();
        if (!HW::UniqueData::GetCTCert().IsValid() || !HW::UniqueData::GetMovableSed().IsValid() ||
            !HW::UniqueData::GetSecureInfoA().IsValid() ||
            !HW::UniqueData::GetLocalFriendCodeSeedB().IsValid()) {
            client->LogOnServer(Network::ArticBaseCommon::LogOnServerType::LOG_ERROR,
                                "Some console unique data is invalid.\n    Aborting...");
            return ResultStatus::ErrorArtic;
        }

        if (cfg.get()) {
            auto cfg_module = cfg->GetModule();
            if (!Service::CFG::Module::IsValidRegionCountry(cfg_module->GetRegionValue(true),
                                                            cfg_module->GetCountryCode())) {
                // Report mismatch to server.
                client->LogOnServer(
                    Network::ArticBaseCommon::LogOnServerType::LOG_ERROR,
                    "The country configuration does not match\n    the console region. "
                    "Please select a valid\n    country from the emulation settings.");
                return ResultStatus::ErrorArtic;
            }
            cfg_module->SetSystemSetupNeeded(false);
        }

        // Set deliver arg so that System Settings goes to the update screen directly
        auto apt = Service::APT::GetModule(system);
        Service::APT::DeliverArg arg;
        arg.param.push_back(0x7a);
        apt->GetAppletManager()->SetDeliverArg(arg);

        // Load NIM
        auto req = client->NewRequest("System_GetNIM");
        auto resp = client->Send(req);
        if (!resp.has_value() || !resp->Succeeded())
            return ResultStatus::ErrorArtic;

        if (resp->GetMethodResult() != 0)
            return ResultStatus::ErrorArtic;

        auto resp_buff = resp->GetResponseBuffer(0);
        if (!resp_buff.has_value() || resp_buff->second != sizeof(ExHeader_Header))
            return ResultStatus::ErrorArtic;

        ExHeader_Header nim_exheader;
        memcpy(&nim_exheader, resp_buff->first, resp_buff->second);

        resp_buff = resp->GetResponseBuffer(1);
        if (!resp_buff.has_value())
            return ResultStatus::ErrorArtic;

        std::vector<u8> code(resp_buff->second);
        memcpy(code.data(), resp_buff->first, resp_buff->second);

        std::shared_ptr<Kernel::Process> nim_process;
        result = LoadExecImpl(nim_process, 0x0004013000002C02, nim_exheader, code);
        if (ResultStatus::Success != result)
            return result;

        // Force O3DS mode so that NIM fetches O3DS titles
        auto am = Service::AM::GetModule(system);
        if (am) {
            if (artic_init_mode == ArticInitMode::O3DS) {
                am->ForceO3DSDeviceID();
            } else if (artic_init_mode == ArticInitMode::N3DS) {
                am->ForceN3DSDeviceID();
            }
        }
    }

    result = LoadExec(process); // Load the executable into memory for booting
    if (ResultStatus::Success != result)
        return result;

    system.ArchiveManager().RegisterSelfNCCH(*this);
    if (!is_initial_setup) {
        system.ArchiveManager().RegisterArticSaveDataSource(client);
        system.ArchiveManager().RegisterArticExtData(client);
        system.ArchiveManager().RegisterArticNCCH(client);
        system.ArchiveManager().RegisterArticSystemSaveData(client);

        auto fs_user = system.ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
        if (fs_user.get()) {
            fs_user->RegisterSecureValueBackend(
                std::make_shared<FileSys::ArticSecureValueBackend>(client));
        }

        auto cfg = system.ServiceManager().GetService<Service::CFG::CFG_U>("cfg:u");
        if (cfg.get()) {
            cfg->UseArticClient(client);
        }

        auto amnet = system.ServiceManager().GetService<Service::AM::AM_NET>("am:net");
        if (amnet.get()) {
            amnet->UseArticClient(client);
        }

        auto amapp = system.ServiceManager().GetService<Service::AM::AM_APP>("am:app");
        if (amapp.get()) {
            amapp->UseArticClient(client);
        }

        if (Settings::values.use_artic_base_controller.GetValue()) {
            auto hid_user = system.ServiceManager().GetService<Service::HID::User>("hid:USER");
            if (hid_user.get()) {
                hid_user->GetModule()->UseArticClient(client);
            }
        }
    }

    ParseRegionLockoutInfo(ncch_program_id);

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::IsExecutable(bool& out_executable) {
    out_executable = true;
    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadCode(std::vector<u8>& buffer) {
    // Code is only read once, there is no need to cache it.

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    size_t code_size = program_exheader.codeset_info.text.num_max_pages * Memory::CITRA_PAGE_SIZE;
    code_size += program_exheader.codeset_info.ro.num_max_pages * Memory::CITRA_PAGE_SIZE;
    code_size += program_exheader.codeset_info.data.num_max_pages * Memory::CITRA_PAGE_SIZE;

    size_t read_amount = 0;
    buffer.clear();

    while (read_amount != code_size) {
        size_t to_read =
            std::min<size_t>(client->GetServerRequestMaxSize() - 0x100, code_size - read_amount);

        auto req = client->NewRequest("Process_ReadCode");
        req.AddParameterS32(static_cast<s32>(read_amount));
        req.AddParameterS32(static_cast<s32>(to_read));
        auto resp = client->Send(req);
        if (!resp.has_value() || !resp->Succeeded() || resp->GetMethodResult() != 0)
            return ResultStatus::ErrorArtic;

        auto code_buff = resp->GetResponseBuffer(0);
        if (!code_buff.has_value() || code_buff->second != to_read)
            return ResultStatus::ErrorArtic;

        buffer.resize(read_amount + to_read);
        memcpy(buffer.data() + read_amount, code_buff->first, to_read);
        read_amount += to_read;
    }

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadIcon(std::vector<u8>& buffer) {
    if (!cached_icon.empty()) {
        buffer = cached_icon;
        return ResultStatus::Success;
    }

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    auto req = client->NewRequest("Process_ReadIcon");
    auto resp = client->Send(req);
    if (!resp.has_value() || !resp->Succeeded() || resp->GetMethodResult() != 0)
        return ResultStatus::ErrorArtic;

    auto icon_buf = resp->GetResponseBuffer(0);
    if (!icon_buf.has_value())
        return ResultStatus::ErrorArtic;

    cached_icon.resize(icon_buf->second);
    memcpy(cached_icon.data(), icon_buf->first, icon_buf->second);
    buffer = cached_icon;

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadBanner(std::vector<u8>& buffer) {
    if (!cached_banner.empty()) {
        buffer = cached_banner;
        return ResultStatus::Success;
    }

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    auto req = client->NewRequest("Process_ReadBanner");
    auto resp = client->Send(req);
    if (!resp.has_value() || !resp->Succeeded() || resp->GetMethodResult() != 0)
        return ResultStatus::ErrorArtic;

    auto banner_buf = resp->GetResponseBuffer(0);
    if (!banner_buf.has_value())
        return ResultStatus::ErrorArtic;

    cached_banner.resize(banner_buf->second);
    memcpy(cached_banner.data(), banner_buf->first, banner_buf->second);
    buffer = cached_banner;

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadLogo(std::vector<u8>& buffer) {
    if (!cached_logo.empty()) {
        buffer = cached_logo;
        return ResultStatus::Success;
    }

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    auto req = client->NewRequest("Process_ReadLogo");
    auto resp = client->Send(req);
    if (!resp.has_value() || !resp->Succeeded() || resp->GetMethodResult() != 0)
        return ResultStatus::ErrorArtic;

    auto logo_buf = resp->GetResponseBuffer(0);
    if (!logo_buf.has_value())
        return ResultStatus::ErrorArtic;

    cached_logo.resize(logo_buf->second);
    memcpy(cached_logo.data(), logo_buf->first, logo_buf->second);
    buffer = cached_logo;

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadProgramId(u64& out_program_id) {
    if (cached_title_id.has_value()) {
        out_program_id = *cached_title_id;
        return ResultStatus::Success;
    }

    EnsureClientConnected();
    if (!client_connected)
        return ResultStatus::ErrorArtic;

    auto req = client->NewRequest("Process_GetTitleID");
    auto resp = client->Send(req);
    if (!resp.has_value())
        return ResultStatus::ErrorArtic;

    auto tid_buf = resp->GetResponseBuffer(0);
    if (!tid_buf.has_value() || tid_buf->second != sizeof(u64))
        return ResultStatus::ErrorArtic;

    out_program_id = *reinterpret_cast<u64*>(tid_buf->first);
    cached_title_id = out_program_id;

    return ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadExtdataId(u64& out_extdata_id) {
    if (program_exheader.arm11_system_local_caps.storage_info.other_attributes >> 1) {
        // Using extended save data access
        // There would be multiple possible extdata IDs in this case. The best we can do for now is
        // guessing that the first one would be the main save.
        const std::array<u64, 6> extdata_ids{{
            program_exheader.arm11_system_local_caps.storage_info.extdata_id0.Value(),
            program_exheader.arm11_system_local_caps.storage_info.extdata_id1.Value(),
            program_exheader.arm11_system_local_caps.storage_info.extdata_id2.Value(),
            program_exheader.arm11_system_local_caps.storage_info.extdata_id3.Value(),
            program_exheader.arm11_system_local_caps.storage_info.extdata_id4.Value(),
            program_exheader.arm11_system_local_caps.storage_info.extdata_id5.Value(),
        }};
        for (u64 id : extdata_ids) {
            if (id) {
                // Found a non-zero ID, use it
                out_extdata_id = id;
                return ResultStatus::Success;
            }
        }

        return ResultStatus::ErrorNotUsed;
    }

    out_extdata_id = program_exheader.arm11_system_local_caps.storage_info.ext_save_data_id;
    return Loader::ResultStatus::Success;
}

ResultStatus Apploader_Artic::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    main_romfs_reader = romfs_file = std::make_shared<FileSys::ArticRomFSReader>(client, false);
    return static_cast<FileSys::ArticRomFSReader*>(romfs_file.get())->OpenStatus();
}

ResultStatus Apploader_Artic::ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    update_romfs_reader = romfs_file = std::make_shared<FileSys::ArticRomFSReader>(client, true);
    return static_cast<FileSys::ArticRomFSReader*>(romfs_file.get())->OpenStatus();
}

ResultStatus Apploader_Artic::DumpRomFS(const std::string& target_path) {
    return ResultStatus::ErrorNotImplemented;
}

ResultStatus Apploader_Artic::DumpUpdateRomFS(const std::string& target_path) {
    return ResultStatus::ErrorNotImplemented;
}

ResultStatus Apploader_Artic::ReadTitle(std::string& title) {
    std::vector<u8> data;
    Loader::SMDH smdh;
    ResultStatus result = ReadIcon(data);
    if (result != ResultStatus::Success) {
        return result;
    }

    if (!Loader::IsValidSMDH(data)) {
        return ResultStatus::ErrorInvalidFormat;
    }

    std::memcpy(&smdh, data.data(), sizeof(Loader::SMDH));

    const auto& short_title = smdh.GetShortTitle(SMDH::TitleLanguage::English);
    auto title_end = std::find(short_title.begin(), short_title.end(), u'\0');
    title = Common::UTF16ToUTF8(std::u16string{short_title.begin(), title_end});

    return ResultStatus::Success;
}

} // namespace Loader
