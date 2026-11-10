# Volatile camera + A53 Linux boot. No Flash, SD or UserSet writes.
set root [file normalize [file join [file dirname [info script]] ..]]
set out [file join $root build ai-linux]
if {[info exists ::env(MAX_CAMERA_AI_LINUX_DIR)]} {
    set out [file normalize $::env(MAX_CAMERA_AI_LINUX_DIR)]
}
set bit [file join $root diagnostics fpga-ppc4-v13-20261102 logic-ppc4-v13.bit]
set xsa [file join $root diagnostics fpga-ppc4-v13-20261102 logic-ppc4-v13.xsa]
set r5 [file join $root build ai-r5 MaxCameraApp.elf]
set fsbl [file normalize [file join $root ../platform/export/platform/sw/boot/fsbl.elf]]
set pmufw [file normalize [file join $root ../platform/export/platform/sw/boot/pmufw.elf]]
set bl31 [file join $out tfa-uart1 zynqmp release bl31 bl31.elf]
set image /path/to/petalinux-project/images/linux/Image
# Environment overrides make the source export usable with externally built artifacts.
foreach {name variable} {MAX_CAMERA_AI_BIT bit MAX_CAMERA_AI_XSA xsa MAX_CAMERA_AI_R5 r5 MAX_CAMERA_AI_FSBL fsbl MAX_CAMERA_AI_PMUFW pmufw MAX_CAMERA_AI_BL31 bl31 MAX_CAMERA_LINUX_IMAGE image} {
    if {[info exists ::env($name)]} {set $variable [file normalize $::env($name)]}
}
foreach p [list $bit $xsa $r5 $fsbl $pmufw $bl31 $image [file join $out system.dtb] [file join $out rootfs.cpio.gz] [file join $out linux-entry.elf]] {
    if {![file isfile $p]} {error "Missing input: $p"}
}
connect -url tcp:127.0.0.1:3121
jtag targets 2
jtag frequency 30000000
if {![info exists ::env(VITIS_ROOT)]} {error "Set VITIS_ROOT to the Vitis2024.2 installation"}
source [file join $::env(VITIS_ROOT) scripts vitis util zynqmp_utils.tcl]
# Force both TAPs to enumerate before selecting reset CPU targets.
puts [targets -target-properties]
targets -set -nocase -filter {name =~ "APU*"}
rst -system
after 3000
targets -set -nocase -filter {name =~ "RPU*"}
enable_split_mode
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-HS1 210512180081" && level==0 && name == "PS TAP"}
fpga -file $bit
targets -set -nocase -filter {name =~ "PSU"}
loadhw -hw $xsa -mem-ranges [list {0x80000000 0xbfffffff} {0x400000000 0x5ffffffff} {0x1000000000 0x7fffffffff}]
configparams force-mem-access 1
disable_pmu_gate
targets -set -nocase -filter {name =~ "MicroBlaze PMU*"}
dow $pmufw
con
after 1000
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow $fsbl
set bp [bpadd -addr &XFsbl_Exit]
con -block -timeout 40
bpremove $bp
puts "AI_FSBL_COMPLETE"
targets -set -nocase -filter {name =~ "*R5*#0"}
rst -processor
dow $r5
con
puts "AI_R5_RUNNING"
# Digilent HS1 supports30MHz; validate the chain before the bulk transfer.
jtag targets 2
jtag frequency 30000000
puts "AI_JTAG_FREQUENCY=[jtag frequency]"
targets -set -nocase -filter {name =~ "PSU"}
dow -data $image 0x20200000
puts "AI_LINUX_IMAGE_LOADED"
dow -data [file join $out system.dtb] 0x22000000
dow -data [file join $out rootfs.cpio.gz] 0x26000000
puts "AI_ROOTFS_LOADED"
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow [file join $out linux-entry.elf]
dow $bl31
configparams force-mem-access 0
con
puts "AI_LINUX_STARTED"
after 1000
puts [targets]
