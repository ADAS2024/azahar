// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace DefaultINI {

const char* sdl2_config_file = R"(
[Controls]
# The input devices and parameters for each 3DS native input
# It should be in the format of "engine:[engine_name],[param1]:[value1],[param2]:[value2]..."
# Escape characters $0 (for ':'), $1 (for ',') and $2 (for '$') can be used in values

# for button input, the following devices are available:
#  - "keyboard" (default) for keyboard input. Required parameters:
#      - "code": the code of the key to bind
#  - "sdl" for joystick input using SDL. Required parameters:
#      - "joystick": the index of the joystick to bind
#      - "button"(optional): the index of the button to bind
#      - "hat"(optional): the index of the hat to bind as direction buttons
#      - "axis"(optional): the index of the axis to bind
#      - "direction"(only used for hat): the direction name of the hat to bind. Can be "up", "down", "left" or "right"
#      - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
#          triggered if the axis value crosses
#      - "direction"(only used for axis): "+" means the button is triggered when the axis value
#          is greater than the threshold; "-" means the button is triggered when the axis value
#          is smaller than the threshold
button_a=
button_b=
button_x=
button_y=
button_up=
button_down=
button_left=
button_right=
button_l=
button_r=
button_start=
button_select=
button_debug=
button_gpio14=
button_zl=
button_zr=
button_home=

# for analog input, the following devices are available:
#  - "analog_from_button" (default) for emulating analog input from direction buttons. Required parameters:
#      - "up", "down", "left", "right": sub-devices for each direction.
#          Should be in the format as a button input devices using escape characters, for example, "engine$0keyboard$1code$00"
#      - "modifier": sub-devices as a modifier.
#      - "modifier_scale": a float number representing the applied modifier scale to the analog input.
#          Must be in range of 0.0-1.0. Defaults to 0.5
#  - "sdl" for joystick input using SDL. Required parameters:
#      - "joystick": the index of the joystick to bind
#      - "axis_x": the index of the axis to bind as x-axis (default to 0)
#      - "axis_y": the index of the axis to bind as y-axis (default to 1)
circle_pad=
c_stick=

# for motion input, the following devices are available:
#  - "motion_emu" (default) for emulating motion input from mouse input. Required parameters:
#      - "update_period": update period in milliseconds (default to 100)
#      - "sensitivity": the coefficient converting mouse movement to tilting angle (default to 0.01)
#      - "tilt_clamp": the max value of the tilt angle in degrees (default to 90)
#  - "cemuhookudp" reads motion input from a udp server that uses cemuhook's udp protocol
motion_device=

# for touch input, the following devices are available:
#  - "emu_window" (default) for emulating touch input from mouse input to the emulation window. No parameters required
#  - "cemuhookudp" reads touch input from a udp server that uses cemuhook's udp protocol
#      - "min_x", "min_y", "max_x", "max_y": defines the udp device's touch screen coordinate system
touch_device= engine:emu_window

# Most desktop operating systems do not expose a way to poll the motion state of the controllers
# so as a way around it, cemuhook created a udp client/server protocol to broadcast the data directly
# from a controller device to the client program. Citra has a client that can connect and read
# from any cemuhook compatible motion program.

# IPv4 address of the udp input server (Default "127.0.0.1")
udp_input_address=

# Port of the udp input server. (Default 26760)
udp_input_port=

# The pad to request data on. Should be between 0 (Pad 1) and 3 (Pad 4). (Default 0)
udp_pad_index=

# Use Artic Controller when connected to Artic Base Server. (Default 0)
use_artic_base_controller=

[Core]
# Whether to use the Just-In-Time (JIT) compiler for CPU emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
use_cpu_jit =

# Change the Clock Frequency of the emulated 3DS CPU.
# Underclocking can increase the performance of the game at the risk of freezing.
# Overclocking may fix lag that happens on console, but also comes with the risk of freezing.
# Range is any positive integer (but we suspect 25 - 400 is a good idea) Default is 100
cpu_clock_percentage =

[Renderer]
# Whether to render using OpenGL
# 1: OpenGL ES (default), 2: Vulkan
graphics_api =

# Whether to compile shaders on multiple worker threads (Vulkan only)
# 0: Off, 1: On (default)
async_shader_compilation =

# Whether to emit PICA fragment shader using SPIRV or GLSL (Vulkan only)
# 0: GLSL, 1: SPIR-V (default)
spirv_shader_gen =

# Whether to disable the SPIRV optimizer. Disabling it reduces stutter, but may slightly worsen performance
# 0: Enabled, 1: Disabled (default)
disable_spirv_optimizer =

# Whether to use hardware shaders to emulate 3DS shaders
# 0: Software, 1 (default): Hardware
use_hw_shader =

# Whether to use accurate multiplication in hardware shaders
# 0: Off (Default. Faster, but causes issues in some games) 1: On (Slower, but correct)
shaders_accurate_mul =

# Whether to use the Just-In-Time (JIT) compiler for shader emulation
# 0: Interpreter (slow), 1 (default): JIT (fast)
use_shader_jit =

# Overrides the sampling filter used by games. This can be useful in certain
# cases with poorly behaved games when upscaling.
# 0 (default): Game Controlled, 1: Nearest Neighbor, 2: Linear
texture_sampling =

# Forces VSync on the display thread. Usually doesn't impact performance, but on some drivers it can
# so only turn this off if you notice a speed difference.
# 0: Off, 1 (default): On
use_vsync_new =

# Reduce stuttering by storing and loading generated shaders to disk
# 0: Off, 1 (default. On)
use_disk_shader_cache =

# Resolution scale factor
# 0: Auto (scales resolution to window size), 1: Native 3DS screen resolution, Otherwise a scale
# factor for the 3DS resolution
resolution_factor =

# Whether to enable V-Sync (caps the framerate at 60FPS) or not.
# 0 (default): Off, 1: On
vsync_enabled =

# Turns on the frame limiter, which will limit frames output to the target game speed
# 0: Off, 1: On (default)
use_frame_limit =

# Limits the speed of the game to run no faster than this value as a percentage of target speed
# 1 - 9999: Speed limit as a percentage of target game speed. 100 (default)
frame_limit =

# Alternative frame limit which can be triggered by the user
# 1 - 9999: Speed limit as a percentage of target game speed. 100 (default)
turbo_limit =

# The clear color for the renderer. What shows up on the sides of the bottom screen.
# Must be in range of 0.0-1.0. Defaults to 0.0 for all.
bg_red =
bg_blue =
bg_green =

# Whether and how Stereoscopic 3D should be rendered
# 0 (default): Off, 1: Side by Side, 2: Reverse Side by Side, 3: Anaglyph, 4: Interlaced, 5: Reverse Interlaced, 6: Cardboard VR
render_3d =

# Change 3D Intensity
# 0 - 255: Intensity. 0 (default)
factor_3d =

# The name of the post processing shader to apply.
# Loaded from shaders if render_3d is off or side by side.
pp_shader_name =

# The name of the shader to apply when render_3d is anaglyph.
# Loaded from shaders/anaglyph
anaglyph_shader_name =

# Whether to enable linear filtering or not
# This is required for some shaders to work correctly
# 0: Nearest, 1 (default): Linear
filter_mode =

# Delays the game render thread by the specified amount of microseconds
# Set to 0 for no delay, only useful in dynamic-fps games to simulate GPU delay.
delay_game_render_thread_us =

# Disables rendering the right eye image.
# Greatly improves performance in some games, but can cause flickering in others.
# 0 (default): Enable right eye rendering, 1: Disable right eye rendering
disable_right_eye_render =

[Layout]
# Layout for the screen inside the render window, landscape mode
# 0: Original (screens vertically aligned)
# 1: Single Screen Only,
# 2: Large Screen (Default on android)
# 3: Side by Side
# 4: Hybrid
# 5: Custom Layout
layout_option =

[Storage]
# Whether to compress the installed CIA contents
# 0 (default): Do not compress, 1: Compress
compress_cia_installs =

# Position of the performance overlay
# 0: Top Left
# 1: Center Top
# 2: Top Right
# 3: Bottom Left
# 4: Center Bottom
# 5: Bottom Right
performance_overlay_position =

# Screen Gap - adds a gap between screens in all two-screen modes
# Measured in pixels relative to the 240px default height of the screens
# Scales with the larger screen (so 24 is 10% of the larger screen height)
# Default value is 0.0
screen_gap =

# Large Screen Proportion - Relative size of large:small in large screen mode
# Default value is 2.25
large_screen_proportion =

# Small Screen Position - where is the small screen relative to the large
# Default value is 0
# 0: Top Right    1: Middle Right    2: Bottom Right
# 3: Top Left     4: Middle left     5: Bottom Left
# 6: Above the large screen          7: Below the large screen
small_screen_position =


# Screen placement when using Custom layout option
# 0x, 0y is the top left corner of the render window.
# suggested aspect ratio for top screen is 5:3
# suggested aspect ratio for bottom screen is 4:3
custom_top_x =
custom_top_y =
custom_top_width =
custom_top_height =
custom_bottom_x =
custom_bottom_y =
custom_bottom_width =
custom_bottom_height =

# Orientation option for the emulator
# 2 (default): Automatic
# 0: Landscape
# 8: Landscape (Flipped)
# 1: Portrait
# 9: Portrait (Flipped)
screen_orientation =

# Layout for the portrait mode
# 0 (default): Top and bottom screens at top, full width
# 1: Custom Layout
portrait_layout_option =

# Screen placement when using Portrait Custom layout option
# 0x, 0y is the top left corner of the render window.
custom_portrait_top_x =
custom_portrait_top_y =
custom_portrait_top_width =
custom_portrait_top_height =
custom_portrait_bottom_x =
custom_portrait_bottom_y =
custom_portrait_bottom_width =
custom_portrait_bottom_height =

# Swaps the prominent screen with the other screen.
# For example, if Single Screen is chosen, setting this to 1 will display the bottom screen instead of the top screen.
# 0 (default): Top Screen is prominent, 1: Bottom Screen is prominent
swap_screen =

# Expands the display area to include the cutout (or notch) area
# 0 (default): Off, 1: On
expand_to_cutout_area =

# Screen placement settings when using Cardboard VR (render3d = 4)
# 30 - 100: Screen size as a percentage of the viewport. 85 (default)
cardboard_screen_size =
# -100 - 100: Screen X-Coordinate shift as a percentage of empty space. 0 (default)
cardboard_x_shift =
# -100 - 100: Screen Y-Coordinate shift as a percentage of empty space. 0 (default)
cardboard_y_shift =

[Utility]
# Dumps textures as PNG to dump/textures/[Title ID]/.
# 0 (default): Off, 1: On
dump_textures =

# Reads PNG files from load/textures/[Title ID]/ and replaces textures.
# 0 (default): Off, 1: On
custom_textures =

# Loads all custom textures into memory before booting.
# 0 (default): Off, 1: On
preload_textures =

# Loads custom textures asynchronously with background threads.
# 0: Off, 1 (default): On
async_custom_loading =

[Audio]
# Whether or not to enable DSP LLE
# 0 (default): No, 1: Yes
enable_dsp_lle =

# Whether or not to run DSP LLE on a different thread
# 0 (default): No, 1: Yes
enable_dsp_lle_thread =

# Whether or not to enable the audio-stretching post-processing effect.
# This effect adjusts audio speed to match emulation speed and helps prevent audio stutter,
# at the cost of increasing audio latency.
# 0: No, 1 (default): Yes
enable_audio_stretching =

# Scales audio playback speed to account for drops in emulation framerate
# 0 (default): No, 1: Yes
enable_realtime_audio =

# Output volume.
# 1.0 (default): 100%, 0.0; mute
volume =

# Which audio output type to use.
# 0 (default): Auto-select, 1: No audio output, 2: Cubeb (if available), 3: OpenAL (if available), 4: SDL2 (if available)
output_type =

# Which audio output device to use.
# auto (default): Auto-select
output_device =

# Which audio input type to use.
# 0 (default): Auto-select, 1: No audio input, 2: Static noise, 3: Cubeb (if available), 4: OpenAL (if available)
input_type =

# Which audio input device to use.
# auto (default): Auto-select
input_device =

[Data Storage]
# Whether to create a virtual SD card.
# 1 (default): Yes, 0: No
use_virtual_sd =

[System]
# The system model that Citra will try to emulate
# 0: Old 3DS (default), 1: New 3DS
is_new_3ds =

# Whether to use LLE system applets, if installed
# 0: No, 1 (default): Yes
lle_applets =

# Whether to enable LLE modules for online play
# 0 (default): No, 1: Yes
enable_required_online_lle_modules =

# The system region that Citra will use during emulation
# -1: Auto-select (default), 0: Japan, 1: USA, 2: Europe, 3: Australia, 4: China, 5: Korea, 6: Taiwan
region_value =

# The system language that Citra will use during emulation
# 0: Japanese, 1: English (default), 2: French, 3: German, 4: Italian, 5: Spanish,
# 6: Simplified Chinese, 7: Korean, 8: Dutch, 9: Portuguese, 10: Russian, 11: Traditional Chinese
language =

# The clock to use when citra starts
# 0: System clock (default), 1: fixed time
init_clock =

# Time used when init_clock is set to fixed_time in the format %Y-%m-%d %H:%M:%S
# set to fixed time. Default 2000-01-01 00:00:01
# Note: 3DS can only handle times later then Jan 1 2000
init_time =

# The system ticks count to use when citra starts
# 0: Random (default), 1: Fixed
init_ticks_type =

# Tick count to use when init_ticks_type is set to Fixed.
# Defaults to 0.
init_ticks_override =

# Number of steps per hour reported by the pedometer. Range from 0 to 65,535.
# Defaults to 0.
steps_per_hour =

# Plugin loader state, if enabled plugins will be loaded from the SD card.
# You can also set if homebrew apps are allowed to enable the plugin loader
plugin_loader =
allow_plugin_loader =

[Camera]
# Which camera engine to use for the right outer camera
# blank: a dummy camera that always returns black image
# image: loads a still image from the storage. When the camera is started, you will be prompted
#        to select an image.
# ndk (Default): uses the device camera. You can specify the camera ID to use in the config field.
#                If you don't specify an ID, the default setting will be used. For outer cameras,
#                the back-facing camera will be used. For the inner camera, the front-facing
#                camera will be used. Please note that 'Legacy' cameras are not supported.
camera_outer_right_name =

# A config string for the right outer camera. Its meaning is defined by the camera engine
camera_outer_right_config =

# The image flip to apply
# 0: None (default), 1: Horizontal, 2: Vertical, 3: Reverse
camera_outer_right_flip =

# ... for the left outer camera
camera_outer_left_name =
camera_outer_left_config =
camera_outer_left_flip =

# ... for the inner camera
camera_inner_name =
camera_inner_config =
camera_inner_flip =

[Miscellaneous]
# A filter which removes logs below a certain logging level.
# Examples: *:Debug Kernel.SVC:Trace Service.*:Critical
log_filter = *:Info

[Debugging]
# Record frame time data, can be found in the log directory. Boolean value
record_frame_times =

# Whether to enable additional debugging information during emulation
# 0 (default): Off, 1: On
renderer_debug =

# Port for listening to GDB connections.
use_gdbstub=false
gdbstub_port=24689

# Flush log output on every message
# Immediately commits the debug log to file. Use this if Azahar crashes and the log output is being cut.
instant_debug_log =

# Enable RPC server for scripting purposes. Allows accessing guest memory remotely.
# 0 (default): Off, 1: On
enable_rpc_server =

# Delay the start of apps when LLE modules are enabled
# 0: Off, 1 (default): On
delay_start_for_lle_modules =

# Force deterministic async operations
# Only needed for debugging, makes performance worse if enabled
# 0: Off (default), 1: On
deterministic_async_operations =

# To LLE a service module add "LLE\<module name>=true"

[WebService]
# URL for Web API
web_api_url =
# Username and token for Citra Web Service
citra_username =
citra_token =
)";
}
