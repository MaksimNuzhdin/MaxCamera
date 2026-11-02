# Source-only HLS recreation. Run with Vitis HLS 2024.2 from any directory.
set root [file dirname [file normalize [info script]]]
if {![info exists ::env(VITIS_VISION_INCLUDE)]} {error "Set VITIS_VISION_INCLUDE to Vitis_Libraries 2024.2_rel/vision/L1/include"}
set vision $::env(VITIS_VISION_INCLUDE)
set tops {camera_raw_correct_v5 camera_color_pipeline_v9 camera_scene_metrics_v8 camera_rgb_output camera_local_contrast camera_adaptive_tone integer_remap}
foreach top $tops {
 open_project -reset [file join $root build hls $top]
 set_top $top
 set flags "-std=c++14 -I$vision -I$root/hls/camera_isp -I$root/experiments/remap_integer"
 if {[info exists ::env(OPENCV_INCLUDE)]} {append flags " -I$::env(OPENCV_INCLUDE)"}
 switch $top {
  camera_raw_correct_v5 {set src {camera_isp.cpp color_adjust.cpp raw_lsc.cpp};set test tests/camera_isp/test_raw_v5.cpp}
  camera_color_pipeline_v9 {set src {camera_isp.cpp color_adjust.cpp rgb_detail.cpp raw_lsc.cpp};set test tests/camera_isp/test_color_v9.cpp}
  camera_scene_metrics_v8 {set src {scene_metrics_v8.cpp};set test tests/camera_isp/test_scene_metrics_v8.cpp}
  camera_rgb_output {set src {rgb_output.cpp};set test tests/camera_isp/test_rgb_output.cpp}
  camera_local_contrast {set src {local_contrast.cpp};set test tests/camera_isp/test_local_contrast.cpp}
  camera_adaptive_tone {set src {adaptive_tone.cpp};set test tests/adaptive_tone/test.cpp}
  integer_remap {set src {};set test experiments/remap_integer/test.cpp;add_files [file join $root experiments remap_integer integer_remap.cpp] -cflags $flags}
 }
 foreach file $src {add_files [file join $root hls camera_isp $file] -cflags $flags}
 add_files -tb [file join $root $test] -cflags $flags
 open_solution -reset solution -flow_target vivado
 set_part xczu2cg-sfvc784-1-e
 create_clock -period 5 -name default
 csim_design
 csynth_design
 export_design -format ip_catalog -rtl verilog
 close_project
}
exit
