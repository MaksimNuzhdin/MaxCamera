# Recreate accepted v13 topology from source. No board access or programming.
set root [file dirname [file normalize [info script]]]
set output [file join $root build vivado]
if {[info exists ::env(MAXCAMERA_HW_BUILD)]} {set output [file normalize $::env(MAXCAMERA_HW_BUILD)]}
set iproot [file join $root build hls]
if {[info exists ::env(MAXCAMERA_HLS_IP_ROOT)]} {set iproot [file normalize $::env(MAXCAMERA_HLS_IP_ROOT)]}
create_project camera_v13 $output -part xczu2cg-sfvc784-1-e
set_property target_language Verilog [current_project]
set repos [list [file join $root ip MAX_FAN_CONTROLLER_1_0]]
foreach top {camera_raw_correct_v5 camera_color_pipeline_v9 camera_scene_metrics_v8 camera_rgb_output camera_local_contrast camera_adaptive_tone integer_remap} {
 set repo [file join $iproot $top solution impl ip]
 if {![file exists [file join $repo component.xml]]} {error "Build missing HLS IP first: $top"}
 lappend repos $repo
}
set_property ip_repo_paths $repos [current_project]
update_ip_catalog
foreach file {axis_raw10_repack.v axis_raw10_isp_bridge.v axis_rgb_byte_pack_v7.v axis_camera_isp_bridge_v9.v axis_rgb_polyphase_route.v axis_byte_to_writer_ppc4.v logic_single_camera.v fan_controller.v} {
 add_files [file join $root rtl $file]
}
add_files -fileset constrs_1 [file join $root constraints single-camera.xdc]
source [file join $root design design_1.tcl]
set_property -dict [list CONFIG.HP_IO_BANK_SELECTION {64} CONFIG.CLK_LANE_IO_LOC {AC9} CONFIG.DATA_LANE0_IO_LOC {AE9} CONFIG.DATA_LANE1_IO_LOC {AB8}] [get_bd_cells mipi_csi2_rx_subsyst_1]
set_property CONFIG.XBAR_DATA_WIDTH 32 [get_bd_cells ps8_0_axi_periph]
validate_bd_design
save_bd_design
set wrappers [make_wrapper -files [get_files design_1.bd] -top]
add_files $wrappers
set_property top logic [current_fileset]
update_compile_order -fileset sources_1
write_bd_tcl -force [file join $output recreated-bd.tcl]
if {[info exists ::env(MAXCAMERA_HW_BUILD_BITSTREAM)] && $::env(MAXCAMERA_HW_BUILD_BITSTREAM) eq "1"} {
 generate_target all [get_files design_1.bd]
 launch_runs synth_1 -jobs 2
 wait_on_run synth_1
 if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {error "Synthesis failed"}
 launch_runs impl_1 -to_step write_bitstream -jobs 2
 wait_on_run impl_1
 if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {error "Implementation failed"}
 open_run impl_1
 report_utilization -file [file join $output utilization.rpt]
 report_timing_summary -report_unconstrained -file [file join $output timing.rpt]
 report_drc -file [file join $output drc.rpt]
 report_route_status -file [file join $output route-status.rpt]
 report_bus_skew -file [file join $output bus-skew.rpt]
 exec python3 [file join $root tools check_build_reports.py] $output --output [file join $output report-check.json]
 write_hw_platform -fixed -include_bit -force -file [file join $output camera_v13.xsa]
}
puts "MAXCAMERA_SOURCE_RECREATION_COMPLETE"
close_project
exit
