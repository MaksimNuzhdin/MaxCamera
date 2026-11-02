
################################################################
# This is a generated script based on design: design_1
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2024.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   if { [string compare $scripts_vivado_version $current_vivado_version] > 0 } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2042 -severity "ERROR" " This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Sourcing the script failed since it was created with a future version of Vivado."}

   } else {
     catch {common::send_gid_msg -ssname BD::TCL -id 2041 -severity "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   }

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source design_1_script.tcl


# The design that will be created by this Tcl script contains the following 
# module references:
# axis_raw10_repack, axis_camera_isp_bridge_v9, axis_rgb_polyphase_route, axis_byte_to_writer_ppc4

# Please add the sources of those modules before sourcing this Tcl script.

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xczu2cg-sfvc784-1-e
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name design_1

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_gid_msg -ssname BD::TCL -id 2001 -severity "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_gid_msg -ssname BD::TCL -id 2002 -severity "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_gid_msg -ssname BD::TCL -id 2003 -severity "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_gid_msg -ssname BD::TCL -id 2004 -severity "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_gid_msg -ssname BD::TCL -id 2005 -severity "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_gid_msg -ssname BD::TCL -id 2006 -severity "ERROR" $errMsg}
   return $nRet
}

set bCheckIPsPassed 1
##################################################################
# CHECK IPs
##################################################################
set bCheckIPs 1
if { $bCheckIPs == 1 } {
   set list_check_ips "\ 
xilinx.com:ip:zynq_ultra_ps_e:3.5\
xilinx.com:ip:clk_wiz:6.0\
xilinx.com:ip:proc_sys_reset:5.0\
xilinx.com:ip:mipi_csi2_rx_subsystem:6.0\
xilinx.com:ip:v_frmbuf_wr:3.0\
xilinx.com:ip:xlconcat:2.1\
xilinx.com:ip:axi_gpio:2.0\
xilinx.com:ip:system_management_wiz:1.3\
xilinx.com:ip:util_ds_buf:2.2\
xilinx.com:ip:axi_timer:2.0\
user.org:user:MAX_FAN_CONTROLLER:1.0\
xilinx.com:ip:axis_subset_converter:1.1\
xilinx.com:ip:xlconstant:1.1\
xilinx.com:hls:camera_raw_correct_v5:1.0\
xilinx.com:hls:camera_color_pipeline_v9:1.0\
xilinx.com:hls:camera_scene_metrics_v8:1.0\
xilinx.com:hls:camera_rgb_output:1.0\
xilinx.com:hls:camera_local_contrast:1.0\
xilinx.com:ip:v_proc_ss:2.3\
xilinx.com:ip:xlslice:1.0\
xilinx.com:hls:camera_adaptive_tone:1.0\
xilinx.com:hls:integer_remap:1.0\
"

   set list_ips_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2011 -severity "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2012 -severity "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

}

##################################################################
# CHECK Modules
##################################################################
set bCheckModules 1
if { $bCheckModules == 1 } {
   set list_check_mods "\ 
axis_raw10_repack\
axis_camera_isp_bridge_v9\
axis_rgb_polyphase_route\
axis_byte_to_writer_ppc4\
"

   set list_mods_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2020 -severity "INFO" "Checking if the following modules exist in the project's sources: $list_check_mods ."

   foreach mod_vlnv $list_check_mods {
      if { [can_resolve_reference $mod_vlnv] == 0 } {
         lappend list_mods_missing $mod_vlnv
      }
   }

   if { $list_mods_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2021 -severity "ERROR" "The following module(s) are not found in the project: $list_mods_missing" }
      common::send_gid_msg -ssname BD::TCL -id 2022 -severity "INFO" "Please add source files for the missing module(s) above."
      set bCheckIPsPassed 0
   }
}

if { $bCheckIPsPassed != 1 } {
  common::send_gid_msg -ssname BD::TCL -id 2023 -severity "WARNING" "Will not continue with creation of design due to the error(s) above."
  return 3
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder
  variable design_name

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2090 -severity "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2091 -severity "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set mipi_phy_if_1 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:mipi_phy_rtl:1.0 mipi_phy_if_1 ]

  set camera_sccb [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 camera_sccb ]


  # Create ports
  set input_clock [ create_bd_port -dir I -type clk -freq_hz 25000000 input_clock ]
  set_property -dict [ list \
   CONFIG.ASSOCIATED_RESET {pl_resetn0_0} \
 ] $input_clock
  set leds_ctrl [ create_bd_port -dir O -from 3 -to 0 leds_ctrl ]
  set fan_ctrl [ create_bd_port -dir O -from 0 -to 0 fan_ctrl ]
  set cam0_pwup_control [ create_bd_port -dir O -from 0 -to 0 cam0_pwup_control ]

  # Create instance: zynq_ultra_ps_e_0, and set properties
  set zynq_ultra_ps_e_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.5 zynq_ultra_ps_e_0 ]
  set_property -dict [list \
    CONFIG.PSU_BANK_0_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_BANK_1_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_BANK_2_IO_STANDARD {LVCMOS18} \
    CONFIG.PSU_DDR_RAM_HIGHADDR {0x3FFFFFFF} \
    CONFIG.PSU_DDR_RAM_HIGHADDR_OFFSET {0x00000002} \
    CONFIG.PSU_DDR_RAM_LOWADDR_OFFSET {0x40000000} \
    CONFIG.PSU_MIO_1_DRIVE_STRENGTH {12} \
    CONFIG.PSU_MIO_1_SLEW {fast} \
    CONFIG.PSU_MIO_26_POLARITY {Default} \
    CONFIG.PSU_MIO_31_POLARITY {Default} \
    CONFIG.PSU_MIO_34_POLARITY {Default} \
    CONFIG.PSU_MIO_35_POLARITY {Default} \
    CONFIG.PSU_MIO_36_POLARITY {Default} \
    CONFIG.PSU_MIO_38_POLARITY {Default} \
    CONFIG.PSU_MIO_39_POLARITY {Default} \
    CONFIG.PSU_MIO_40_POLARITY {Default} \
    CONFIG.PSU_MIO_41_POLARITY {Default} \
    CONFIG.PSU_MIO_42_POLARITY {Default} \
    CONFIG.PSU_MIO_43_POLARITY {Default} \
    CONFIG.PSU_MIO_44_POLARITY {Default} \
    CONFIG.PSU_MIO_4_INPUT_TYPE {cmos} \
    CONFIG.PSU_MIO_TREE_PERIPHERALS {Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Quad SPI Flash#Feedback Clk##################UART 1#UART 1#GPIO1 MIO#DPAUX#DPAUX#DPAUX#DPAUX#GPIO1\
MIO#I2C 1#I2C 1#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#PCIE#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#GPIO1 MIO#SD 1#SD 1#SD 1#SD 1#SD 1#SD 1#SD 1#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB 0#USB\
0#USB 0#USB 0#USB 0#USB 0#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#Gem 3#MDIO 3#MDIO 3} \
    CONFIG.PSU_MIO_TREE_SIGNALS {sclk_out#miso_mo1#mo2#mo3#mosi_mi0#n_ss_out#clk_for_lpbk##################txd#rxd#gpio1[26]#dp_aux_data_out#dp_hot_plug_detect#dp_aux_data_oe#dp_aux_data_in#gpio1[31]#scl_out#sda_out#gpio1[34]#gpio1[35]#gpio1[36]#reset_n#gpio1[38]#gpio1[39]#gpio1[40]#gpio1[41]#gpio1[42]#gpio1[43]#gpio1[44]#sdio1_cd_n#sdio1_data_out[0]#sdio1_data_out[1]#sdio1_data_out[2]#sdio1_data_out[3]#sdio1_cmd_out#sdio1_clk_out#ulpi_clk_in#ulpi_dir#ulpi_tx_data[2]#ulpi_nxt#ulpi_tx_data[0]#ulpi_tx_data[1]#ulpi_stp#ulpi_tx_data[3]#ulpi_tx_data[4]#ulpi_tx_data[5]#ulpi_tx_data[6]#ulpi_tx_data[7]#rgmii_tx_clk#rgmii_txd[0]#rgmii_txd[1]#rgmii_txd[2]#rgmii_txd[3]#rgmii_tx_ctl#rgmii_rx_clk#rgmii_rxd[0]#rgmii_rxd[1]#rgmii_rxd[2]#rgmii_rxd[3]#rgmii_rx_ctl#gem3_mdc#gem3_mdio_out}\
\
    CONFIG.PSU_SD1_INTERNAL_BUS_WIDTH {4} \
    CONFIG.PSU_USB3__DUAL_CLOCK_ENABLE {1} \
    CONFIG.PSU__ACT_DDR_FREQ_MHZ {1199.988037} \
    CONFIG.PSU__CRF_APB__ACPU_CTRL__ACT_FREQMHZ {1199.988037} \
    CONFIG.PSU__CRF_APB__DBG_FPD_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRF_APB__DBG_TSTMP_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRF_APB__DDR_CTRL__ACT_FREQMHZ {599.994019} \
    CONFIG.PSU__CRF_APB__DDR_CTRL__FREQMHZ {1200} \
    CONFIG.PSU__CRF_APB__DPDMA_REF_CTRL__ACT_FREQMHZ {599.994019} \
    CONFIG.PSU__CRF_APB__DP_AUDIO_REF_CTRL__ACT_FREQMHZ {24.999750} \
    CONFIG.PSU__CRF_APB__DP_AUDIO__FRAC_ENABLED {0} \
    CONFIG.PSU__CRF_APB__DP_STC_REF_CTRL__ACT_FREQMHZ {26.922808} \
    CONFIG.PSU__CRF_APB__DP_VIDEO_REF_CTRL__ACT_FREQMHZ {299.997009} \
    CONFIG.PSU__CRF_APB__DP_VIDEO__FRAC_ENABLED {0} \
    CONFIG.PSU__CRF_APB__GDMA_REF_CTRL__ACT_FREQMHZ {599.994019} \
    CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__FREQMHZ {250} \
    CONFIG.PSU__CRF_APB__PCIE_REF_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRF_APB__TOPSW_LSBUS_CTRL__ACT_FREQMHZ {99.999001} \
    CONFIG.PSU__CRF_APB__TOPSW_MAIN_CTRL__ACT_FREQMHZ {524.994751} \
    CONFIG.PSU__CRL_APB__ADMA_REF_CTRL__ACT_FREQMHZ {499.994995} \
    CONFIG.PSU__CRL_APB__AMS_REF_CTRL__ACT_FREQMHZ {49.999500} \
    CONFIG.PSU__CRL_APB__CPU_R5_CTRL__ACT_FREQMHZ {499.994995} \
    CONFIG.PSU__CRL_APB__DBG_LPD_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRL_APB__DLL_REF_CTRL__ACT_FREQMHZ {1499.984985} \
    CONFIG.PSU__CRL_APB__GEM3_REF_CTRL__ACT_FREQMHZ {124.998749} \
    CONFIG.PSU__CRL_APB__GEM_TSU_REF_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRL_APB__GEM_TSU_REF_CTRL__SRCSEL {IOPLL} \
    CONFIG.PSU__CRL_APB__I2C1_REF_CTRL__ACT_FREQMHZ {99.999001} \
    CONFIG.PSU__CRL_APB__IOU_SWITCH_CTRL__ACT_FREQMHZ {266.664001} \
    CONFIG.PSU__CRL_APB__LPD_LSBUS_CTRL__ACT_FREQMHZ {99.999001} \
    CONFIG.PSU__CRL_APB__LPD_SWITCH_CTRL__ACT_FREQMHZ {499.994995} \
    CONFIG.PSU__CRL_APB__PCAP_CTRL__ACT_FREQMHZ {187.498123} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__ACT_FREQMHZ {99.999001} \
    CONFIG.PSU__CRL_APB__PL1_REF_CTRL__ACT_FREQMHZ {199.998001} \
    CONFIG.PSU__CRL_APB__QSPI_REF_CTRL__ACT_FREQMHZ {299.997009} \
    CONFIG.PSU__CRL_APB__SDIO1_REF_CTRL__ACT_FREQMHZ {199.998001} \
    CONFIG.PSU__CRL_APB__TIMESTAMP_REF_CTRL__ACT_FREQMHZ {33.333000} \
    CONFIG.PSU__CRL_APB__UART1_REF_CTRL__ACT_FREQMHZ {99.999001} \
    CONFIG.PSU__CRL_APB__USB0_BUS_REF_CTRL__ACT_FREQMHZ {249.997498} \
    CONFIG.PSU__CRL_APB__USB3_DUAL_REF_CTRL__ACT_FREQMHZ {19.999800} \
    CONFIG.PSU__CRL_APB__USB3__ENABLE {1} \
    CONFIG.PSU__DDRC__BG_ADDR_COUNT {1} \
    CONFIG.PSU__DDRC__BUS_WIDTH {32 Bit} \
    CONFIG.PSU__DDRC__CL {16} \
    CONFIG.PSU__DDRC__CWL {12} \
    CONFIG.PSU__DDRC__DEVICE_CAPACITY {4096 MBits} \
    CONFIG.PSU__DDRC__DRAM_WIDTH {16 Bits} \
    CONFIG.PSU__DDRC__ROW_ADDR_COUNT {15} \
    CONFIG.PSU__DDRC__SPEED_BIN {DDR4_2400P} \
    CONFIG.PSU__DDRC__T_FAW {30.0} \
    CONFIG.PSU__DDRC__T_RAS_MIN {32.0} \
    CONFIG.PSU__DDRC__T_RC {44.5} \
    CONFIG.PSU__DDRC__T_RCD {16} \
    CONFIG.PSU__DDRC__T_RP {16} \
    CONFIG.PSU__DDR_HIGH_ADDRESS_GUI_ENABLE {0} \
    CONFIG.PSU__DDR__INTERFACE__FREQMHZ {600.000} \
    CONFIG.PSU__DISPLAYPORT__LANE0__ENABLE {1} \
    CONFIG.PSU__DISPLAYPORT__LANE0__IO {GT Lane3} \
    CONFIG.PSU__DISPLAYPORT__LANE1__ENABLE {1} \
    CONFIG.PSU__DISPLAYPORT__LANE1__IO {GT Lane2} \
    CONFIG.PSU__DISPLAYPORT__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__DLL__ISUSED {1} \
    CONFIG.PSU__DPAUX__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__DPAUX__PERIPHERAL__IO {MIO 27 .. 30} \
    CONFIG.PSU__DP__LANE_SEL {Dual Higher} \
    CONFIG.PSU__DP__REF_CLK_FREQ {27} \
    CONFIG.PSU__DP__REF_CLK_SEL {Ref Clk2} \
    CONFIG.PSU__ENET3__FIFO__ENABLE {0} \
    CONFIG.PSU__ENET3__GRP_MDIO__ENABLE {1} \
    CONFIG.PSU__ENET3__GRP_MDIO__IO {MIO 76 .. 77} \
    CONFIG.PSU__ENET3__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__ENET3__PERIPHERAL__IO {MIO 64 .. 75} \
    CONFIG.PSU__ENET3__PTP__ENABLE {0} \
    CONFIG.PSU__ENET3__TSU__ENABLE {0} \
    CONFIG.PSU__FPDMASTERS_COHERENCY {0} \
    CONFIG.PSU__FPGA_PL0_ENABLE {0} \
    CONFIG.PSU__FPGA_PL1_ENABLE {0} \
    CONFIG.PSU__GEM3_COHERENCY {0} \
    CONFIG.PSU__GEM3_ROUTE_THROUGH_FPD {0} \
    CONFIG.PSU__GEM__TSU__ENABLE {0} \
    CONFIG.PSU__GPIO1_MIO__IO {MIO 26 .. 51} \
    CONFIG.PSU__GPIO1_MIO__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__GPIO_EMIO_WIDTH {95} \
    CONFIG.PSU__GPIO_EMIO__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__GPIO_EMIO__PERIPHERAL__IO {95} \
    CONFIG.PSU__GT__LINK_SPEED {HBR} \
    CONFIG.PSU__GT__PRE_EMPH_LVL_4 {0} \
    CONFIG.PSU__GT__VLT_SWNG_LVL_4 {0} \
    CONFIG.PSU__I2C1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__I2C1__PERIPHERAL__IO {MIO 32 .. 33} \
    CONFIG.PSU__MAXIGP0__DATA_WIDTH {128} \
    CONFIG.PSU__PCIE__BAR0_64BIT {0} \
    CONFIG.PSU__PCIE__BAR0_ENABLE {1} \
    CONFIG.PSU__PCIE__BAR0_PREFETCHABLE {0} \
    CONFIG.PSU__PCIE__BAR0_SCALE {Megabytes} \
    CONFIG.PSU__PCIE__BAR0_SIZE {1} \
    CONFIG.PSU__PCIE__BAR0_TYPE {Memory} \
    CONFIG.PSU__PCIE__BAR0_VAL {0xfff00000} \
    CONFIG.PSU__PCIE__BAR1_ENABLE {0} \
    CONFIG.PSU__PCIE__BAR1_VAL {0x0} \
    CONFIG.PSU__PCIE__BAR2_ENABLE {0} \
    CONFIG.PSU__PCIE__BAR2_VAL {0x0} \
    CONFIG.PSU__PCIE__BAR3_ENABLE {0} \
    CONFIG.PSU__PCIE__BAR3_VAL {0x0} \
    CONFIG.PSU__PCIE__BAR4_ENABLE {0} \
    CONFIG.PSU__PCIE__BAR4_VAL {0x0} \
    CONFIG.PSU__PCIE__BAR5_ENABLE {0} \
    CONFIG.PSU__PCIE__BAR5_VAL {0x0} \
    CONFIG.PSU__PCIE__BRIDGE_BAR_INDICATOR {BAR 0} \
    CONFIG.PSU__PCIE__CLASS_CODE_BASE {0x05} \
    CONFIG.PSU__PCIE__CLASS_CODE_INTERFACE {0x0} \
    CONFIG.PSU__PCIE__CLASS_CODE_SUB {0x80} \
    CONFIG.PSU__PCIE__CLASS_CODE_VALUE {0x58000} \
    CONFIG.PSU__PCIE__DEVICE_ID {0xD011} \
    CONFIG.PSU__PCIE__DEVICE_PORT_TYPE {Endpoint Device} \
    CONFIG.PSU__PCIE__EROM_ENABLE {0} \
    CONFIG.PSU__PCIE__EROM_VAL {0x0} \
    CONFIG.PSU__PCIE__INTX_GENERATION {1} \
    CONFIG.PSU__PCIE__INTX_PIN {INTA} \
    CONFIG.PSU__PCIE__LANE0__ENABLE {1} \
    CONFIG.PSU__PCIE__LANE0__IO {GT Lane0} \
    CONFIG.PSU__PCIE__LANE1__ENABLE {0} \
    CONFIG.PSU__PCIE__LANE2__ENABLE {0} \
    CONFIG.PSU__PCIE__LANE3__ENABLE {0} \
    CONFIG.PSU__PCIE__LINK_SPEED {5.0 Gb/s} \
    CONFIG.PSU__PCIE__MAXIMUM_LINK_WIDTH {x1} \
    CONFIG.PSU__PCIE__MAX_PAYLOAD_SIZE {256 bytes} \
    CONFIG.PSU__PCIE__MSIX_BAR_INDICATOR {BAR 0} \
    CONFIG.PSU__PCIE__MSIX_PBA_BAR_INDICATOR {BAR 0} \
    CONFIG.PSU__PCIE__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__PCIE__PERIPHERAL__ENDPOINT_ENABLE {1} \
    CONFIG.PSU__PCIE__PERIPHERAL__ENDPOINT_IO {MIO 37} \
    CONFIG.PSU__PCIE__PERIPHERAL__ROOTPORT_ENABLE {0} \
    CONFIG.PSU__PCIE__REF_CLK_FREQ {100} \
    CONFIG.PSU__PCIE__REF_CLK_SEL {Ref Clk0} \
    CONFIG.PSU__PCIE__RESET__POLARITY {Active Low} \
    CONFIG.PSU__PCIE__REVISION_ID {0x0} \
    CONFIG.PSU__PCIE__SUBSYSTEM_ID {0x7} \
    CONFIG.PSU__PCIE__SUBSYSTEM_VENDOR_ID {0x10EE} \
    CONFIG.PSU__PCIE__VENDOR_ID {0x10EE} \
    CONFIG.PSU__PROTECTION__MASTERS {USB1:NonSecure;0|USB0:NonSecure;1|S_AXI_LPD:NA;0|S_AXI_HPC1_FPD:NA;0|S_AXI_HPC0_FPD:NA;0|S_AXI_HP3_FPD:NA;0|S_AXI_HP2_FPD:NA;0|S_AXI_HP1_FPD:NA;1|S_AXI_HP0_FPD:NA;0|S_AXI_ACP:NA;0|S_AXI_ACE:NA;0|SD1:NonSecure;1|SD0:NonSecure;0|SATA1:NonSecure;0|SATA0:NonSecure;0|RPU1:Secure;1|RPU0:Secure;1|QSPI:NonSecure;1|PMU:NA;1|PCIe:NonSecure;1|NAND:NonSecure;0|LDMA:NonSecure;1|GPU:NonSecure;1|GEM3:NonSecure;1|GEM2:NonSecure;0|GEM1:NonSecure;0|GEM0:NonSecure;0|FDMA:NonSecure;1|DP:NonSecure;1|DAP:NA;1|Coresight:NA;1|CSU:NA;1|APU:NA;1}\
\
    CONFIG.PSU__PROTECTION__SLAVES {LPD;USB3_1_XHCI;FE300000;FE3FFFFF;0|LPD;USB3_1;FF9E0000;FF9EFFFF;0|LPD;USB3_0_XHCI;FE200000;FE2FFFFF;1|LPD;USB3_0;FF9D0000;FF9DFFFF;1|LPD;UART1;FF010000;FF01FFFF;1|LPD;UART0;FF000000;FF00FFFF;0|LPD;TTC3;FF140000;FF14FFFF;0|LPD;TTC2;FF130000;FF13FFFF;0|LPD;TTC1;FF120000;FF12FFFF;0|LPD;TTC0;FF110000;FF11FFFF;0|FPD;SWDT1;FD4D0000;FD4DFFFF;0|LPD;SWDT0;FF150000;FF15FFFF;0|LPD;SPI1;FF050000;FF05FFFF;0|LPD;SPI0;FF040000;FF04FFFF;0|FPD;SMMU_REG;FD5F0000;FD5FFFFF;1|FPD;SMMU;FD800000;FDFFFFFF;1|FPD;SIOU;FD3D0000;FD3DFFFF;1|FPD;SERDES;FD400000;FD47FFFF;1|LPD;SD1;FF170000;FF17FFFF;1|LPD;SD0;FF160000;FF16FFFF;0|FPD;SATA;FD0C0000;FD0CFFFF;0|LPD;RTC;FFA60000;FFA6FFFF;1|LPD;RSA_CORE;FFCE0000;FFCEFFFF;1|LPD;RPU;FF9A0000;FF9AFFFF;1|LPD;R5_TCM_RAM_GLOBAL;FFE00000;FFE3FFFF;1|LPD;R5_1_Instruction_Cache;FFEC0000;FFECFFFF;1|LPD;R5_1_Data_Cache;FFED0000;FFEDFFFF;1|LPD;R5_1_BTCM_GLOBAL;FFEB0000;FFEBFFFF;1|LPD;R5_1_ATCM_GLOBAL;FFE90000;FFE9FFFF;1|LPD;R5_0_Instruction_Cache;FFE40000;FFE4FFFF;1|LPD;R5_0_Data_Cache;FFE50000;FFE5FFFF;1|LPD;R5_0_BTCM_GLOBAL;FFE20000;FFE2FFFF;1|LPD;R5_0_ATCM_GLOBAL;FFE00000;FFE0FFFF;1|LPD;QSPI_Linear_Address;C0000000;DFFFFFFF;1|LPD;QSPI;FF0F0000;FF0FFFFF;1|LPD;PMU_RAM;FFDC0000;FFDDFFFF;1|LPD;PMU_GLOBAL;FFD80000;FFDBFFFF;1|FPD;PCIE_MAIN;FD0E0000;FD0EFFFF;1|FPD;PCIE_LOW;E0000000;EFFFFFFF;1|FPD;PCIE_HIGH2;8000000000;BFFFFFFFFF;1|FPD;PCIE_HIGH1;600000000;7FFFFFFFF;1|FPD;PCIE_DMA;FD0F0000;FD0FFFFF;1|FPD;PCIE_ATTRIB;FD480000;FD48FFFF;1|LPD;OCM_XMPU_CFG;FFA70000;FFA7FFFF;1|LPD;OCM_SLCR;FF960000;FF96FFFF;1|OCM;OCM;FFFC0000;FFFFFFFF;1|LPD;NAND;FF100000;FF10FFFF;0|LPD;MBISTJTAG;FFCF0000;FFCFFFFF;1|LPD;LPD_XPPU_SINK;FF9C0000;FF9CFFFF;1|LPD;LPD_XPPU;FF980000;FF98FFFF;1|LPD;LPD_SLCR_SECURE;FF4B0000;FF4DFFFF;1|LPD;LPD_SLCR;FF410000;FF4AFFFF;1|LPD;LPD_GPV;FE100000;FE1FFFFF;1|LPD;LPD_DMA_7;FFAF0000;FFAFFFFF;1|LPD;LPD_DMA_6;FFAE0000;FFAEFFFF;1|LPD;LPD_DMA_5;FFAD0000;FFADFFFF;1|LPD;LPD_DMA_4;FFAC0000;FFACFFFF;1|LPD;LPD_DMA_3;FFAB0000;FFABFFFF;1|LPD;LPD_DMA_2;FFAA0000;FFAAFFFF;1|LPD;LPD_DMA_1;FFA90000;FFA9FFFF;1|LPD;LPD_DMA_0;FFA80000;FFA8FFFF;1|LPD;IPI_CTRL;FF380000;FF3FFFFF;1|LPD;IOU_SLCR;FF180000;FF23FFFF;1|LPD;IOU_SECURE_SLCR;FF240000;FF24FFFF;1|LPD;IOU_SCNTRS;FF260000;FF26FFFF;1|LPD;IOU_SCNTR;FF250000;FF25FFFF;1|LPD;IOU_GPV;FE000000;FE0FFFFF;1|LPD;I2C1;FF030000;FF03FFFF;1|LPD;I2C0;FF020000;FF02FFFF;0|FPD;GPU;FD4B0000;FD4BFFFF;0|LPD;GPIO;FF0A0000;FF0AFFFF;1|LPD;GEM3;FF0E0000;FF0EFFFF;1|LPD;GEM2;FF0D0000;FF0DFFFF;0|LPD;GEM1;FF0C0000;FF0CFFFF;0|LPD;GEM0;FF0B0000;FF0BFFFF;0|FPD;FPD_XMPU_SINK;FD4F0000;FD4FFFFF;1|FPD;FPD_XMPU_CFG;FD5D0000;FD5DFFFF;1|FPD;FPD_SLCR_SECURE;FD690000;FD6CFFFF;1|FPD;FPD_SLCR;FD610000;FD68FFFF;1|FPD;FPD_DMA_CH7;FD570000;FD57FFFF;1|FPD;FPD_DMA_CH6;FD560000;FD56FFFF;1|FPD;FPD_DMA_CH5;FD550000;FD55FFFF;1|FPD;FPD_DMA_CH4;FD540000;FD54FFFF;1|FPD;FPD_DMA_CH3;FD530000;FD53FFFF;1|FPD;FPD_DMA_CH2;FD520000;FD52FFFF;1|FPD;FPD_DMA_CH1;FD510000;FD51FFFF;1|FPD;FPD_DMA_CH0;FD500000;FD50FFFF;1|LPD;EFUSE;FFCC0000;FFCCFFFF;1|FPD;Display\
Port;FD4A0000;FD4AFFFF;1|FPD;DPDMA;FD4C0000;FD4CFFFF;1|FPD;DDR_XMPU5_CFG;FD050000;FD05FFFF;1|FPD;DDR_XMPU4_CFG;FD040000;FD04FFFF;1|FPD;DDR_XMPU3_CFG;FD030000;FD03FFFF;1|FPD;DDR_XMPU2_CFG;FD020000;FD02FFFF;1|FPD;DDR_XMPU1_CFG;FD010000;FD01FFFF;1|FPD;DDR_XMPU0_CFG;FD000000;FD00FFFF;1|FPD;DDR_QOS_CTRL;FD090000;FD09FFFF;1|FPD;DDR_PHY;FD080000;FD08FFFF;1|DDR;DDR_LOW;0;3FFFFFFF;1|DDR;DDR_HIGH;800000000;800000000;0|FPD;DDDR_CTRL;FD070000;FD070FFF;1|LPD;Coresight;FE800000;FEFFFFFF;1|LPD;CSU_DMA;FFC80000;FFC9FFFF;1|LPD;CSU;FFCA0000;FFCAFFFF;1|LPD;CRL_APB;FF5E0000;FF85FFFF;1|FPD;CRF_APB;FD1A0000;FD2DFFFF;1|FPD;CCI_REG;FD5E0000;FD5EFFFF;1|LPD;CAN1;FF070000;FF07FFFF;0|LPD;CAN0;FF060000;FF06FFFF;0|FPD;APU;FD5C0000;FD5CFFFF;1|LPD;APM_INTC_IOU;FFA20000;FFA2FFFF;1|LPD;APM_FPD_LPD;FFA30000;FFA3FFFF;1|FPD;APM_5;FD490000;FD49FFFF;1|FPD;APM_0;FD0B0000;FD0BFFFF;1|LPD;APM2;FFA10000;FFA1FFFF;1|LPD;APM1;FFA00000;FFA0FFFF;1|LPD;AMS;FFA50000;FFA5FFFF;1|FPD;AFI_5;FD3B0000;FD3BFFFF;1|FPD;AFI_4;FD3A0000;FD3AFFFF;1|FPD;AFI_3;FD390000;FD39FFFF;1|FPD;AFI_2;FD380000;FD38FFFF;1|FPD;AFI_1;FD370000;FD37FFFF;1|FPD;AFI_0;FD360000;FD36FFFF;1|LPD;AFIFM6;FF9B0000;FF9BFFFF;1|FPD;ACPU_GIC;F9010000;F907FFFF;1}\
\
    CONFIG.PSU__QSPI_COHERENCY {0} \
    CONFIG.PSU__QSPI_ROUTE_THROUGH_FPD {0} \
    CONFIG.PSU__QSPI__GRP_FBCLK__ENABLE {1} \
    CONFIG.PSU__QSPI__GRP_FBCLK__IO {MIO 6} \
    CONFIG.PSU__QSPI__PERIPHERAL__DATA_MODE {x4} \
    CONFIG.PSU__QSPI__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__QSPI__PERIPHERAL__IO {MIO 0 .. 5} \
    CONFIG.PSU__QSPI__PERIPHERAL__MODE {Single} \
    CONFIG.PSU__SAXIGP3__DATA_WIDTH {128} \
    CONFIG.PSU__SD1_COHERENCY {0} \
    CONFIG.PSU__SD1_ROUTE_THROUGH_FPD {0} \
    CONFIG.PSU__SD1__CLK_50_SDR_ITAP_DLY {0x15} \
    CONFIG.PSU__SD1__CLK_50_SDR_OTAP_DLY {0x5} \
    CONFIG.PSU__SD1__DATA_TRANSFER_MODE {4Bit} \
    CONFIG.PSU__SD1__GRP_CD__ENABLE {1} \
    CONFIG.PSU__SD1__GRP_CD__IO {MIO 45} \
    CONFIG.PSU__SD1__GRP_POW__ENABLE {0} \
    CONFIG.PSU__SD1__GRP_WP__ENABLE {0} \
    CONFIG.PSU__SD1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__SD1__PERIPHERAL__IO {MIO 46 .. 51} \
    CONFIG.PSU__SD1__SLOT_TYPE {SD 2.0} \
    CONFIG.PSU__TSU__BUFG_PORT_PAIR {0} \
    CONFIG.PSU__UART1__BAUD_RATE {115200} \
    CONFIG.PSU__UART1__MODEM__ENABLE {0} \
    CONFIG.PSU__UART1__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__UART1__PERIPHERAL__IO {MIO 24 .. 25} \
    CONFIG.PSU__USB0_COHERENCY {0} \
    CONFIG.PSU__USB0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__USB0__PERIPHERAL__IO {MIO 52 .. 63} \
    CONFIG.PSU__USB0__REF_CLK_FREQ {26} \
    CONFIG.PSU__USB0__REF_CLK_SEL {Ref Clk3} \
    CONFIG.PSU__USB2_0__EMIO__ENABLE {0} \
    CONFIG.PSU__USB3_0__EMIO__ENABLE {0} \
    CONFIG.PSU__USB3_0__PERIPHERAL__ENABLE {1} \
    CONFIG.PSU__USB3_0__PERIPHERAL__IO {GT Lane1} \
    CONFIG.PSU__USB__RESET__MODE {Boot Pin} \
    CONFIG.PSU__USB__RESET__POLARITY {Active Low} \
    CONFIG.PSU__USE__IRQ0 {1} \
    CONFIG.PSU__USE__M_AXI_GP0 {1} \
    CONFIG.PSU__USE__M_AXI_GP1 {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} \
    CONFIG.PSU__USE__S_AXI_GP2 {0} \
    CONFIG.PSU__USE__S_AXI_GP3 {1} \
  ] $zynq_ultra_ps_e_0


  # Create instance: clk_wiz_0, and set properties
  set clk_wiz_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:6.0 clk_wiz_0 ]
  set_property -dict [list \
    CONFIG.CLKIN1_JITTER_PS {400.0} \
    CONFIG.CLKIN2_JITTER_PS {100.0} \
    CONFIG.CLKOUT1_JITTER {134.782} \
    CONFIG.CLKOUT1_PHASE_ERROR {228.705} \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {200.000} \
    CONFIG.JITTER_SEL {Min_O_Jitter} \
    CONFIG.MMCM_BANDWIDTH {HIGH} \
    CONFIG.MMCM_CLKFBOUT_MULT_F {63.000} \
    CONFIG.MMCM_CLKIN1_PERIOD {40.000} \
    CONFIG.MMCM_CLKIN2_PERIOD {10.0} \
    CONFIG.MMCM_CLKOUT0_DIVIDE_F {7.875} \
    CONFIG.OPTIMIZE_CLOCKING_STRUCTURE_EN {true} \
    CONFIG.RESET_PORT {reset} \
    CONFIG.RESET_TYPE {ACTIVE_HIGH} \
    CONFIG.SECONDARY_SOURCE {Single_ended_clock_capable_pin} \
    CONFIG.USE_INCLK_SWITCHOVER {false} \
    CONFIG.USE_LOCKED {false} \
    CONFIG.USE_RESET {false} \
  ] $clk_wiz_0


  # Create instance: proc_sys_reset_0, and set properties
  set proc_sys_reset_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0 ]

  # Create instance: mipi_csi2_rx_subsyst_1, and set properties
  set mipi_csi2_rx_subsyst_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:mipi_csi2_rx_subsystem:6.0 mipi_csi2_rx_subsyst_1 ]
  set_property -dict [list \
    CONFIG.AXIS_TDATA_WIDTH {32} \
    CONFIG.AXIS_TUSER_WIDTH {96} \
    CONFIG.CMN_INC_VFB {false} \
    CONFIG.CMN_NUM_LANES {2} \
    CONFIG.CMN_PXL_FORMAT {RAW8} \
    CONFIG.DPY_LINE_RATE {1000} \
    CONFIG.SupportLevel {1} \
  ] $mipi_csi2_rx_subsyst_1


  # Create instance: v_frmbuf_wr_1, and set properties
  set v_frmbuf_wr_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:v_frmbuf_wr:3.0 v_frmbuf_wr_1 ]
  set_property -dict [list \
    CONFIG.HAS_UYVY8 {0} \
    CONFIG.HAS_Y8 {1} \
    CONFIG.HAS_YUYV8 {0} \
    CONFIG.HAS_Y_UV8 {0} \
    CONFIG.MAX_COLS {8192} \
    CONFIG.MAX_ROWS {1944} \
    CONFIG.SAMPLES_PER_CLOCK {4} \
  ] $v_frmbuf_wr_1


  # Create instance: axi_interconnect_1, and set properties
  set axi_interconnect_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_1 ]
  set_property -dict [list \
    CONFIG.NUM_MI {1} \
    CONFIG.NUM_SI {2} \
    CONFIG.S00_HAS_REGSLICE {3} \
  ] $axi_interconnect_1


  # Create instance: xlconcat_0, and set properties
  set xlconcat_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0 ]
  set_property CONFIG.NUM_PORTS {6} $xlconcat_0


  # Create instance: ps8_0_axi_periph, and set properties
  set ps8_0_axi_periph [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 ps8_0_axi_periph ]
  set_property -dict [list \
    CONFIG.NUM_MI {16} \
    CONFIG.NUM_SI {2} \
    CONFIG.S00_HAS_REGSLICE {3} \
    CONFIG.S01_HAS_REGSLICE {1} \
    CONFIG.S02_HAS_REGSLICE {4} \
  ] $ps8_0_axi_periph


  # Create instance: LEDS_CONTROLLER, and set properties
  set LEDS_CONTROLLER [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 LEDS_CONTROLLER ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS {0} \
    CONFIG.C_ALL_INPUTS_2 {0} \
    CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_ALL_OUTPUTS_2 {1} \
    CONFIG.C_DOUT_DEFAULT_2 {0x00000001} \
    CONFIG.C_GPIO2_WIDTH {1} \
    CONFIG.C_GPIO_WIDTH {4} \
    CONFIG.C_IS_DUAL {1} \
  ] $LEDS_CONTROLLER


  # Create instance: system_management_wiz_0, and set properties
  set system_management_wiz_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:system_management_wiz:1.3 system_management_wiz_0 ]
  set_property -dict [list \
    CONFIG.CHANNEL_ENABLE_VBRAM {false} \
    CONFIG.CHANNEL_ENABLE_VCCAUX {false} \
    CONFIG.CHANNEL_ENABLE_VCCINT {false} \
    CONFIG.CHANNEL_ENABLE_VP_VN {false} \
    CONFIG.ENABLE_TEMP_BUS {true} \
    CONFIG.ENABLE_VCCPSAUX_ALARM {false} \
    CONFIG.ENABLE_VCCPSINTFP_ALARM {false} \
    CONFIG.ENABLE_VCCPSINTLP_ALARM {false} \
    CONFIG.UNDER_TEMP_ALARM {false} \
    CONFIG.VCCAUX_ALARM {false} \
    CONFIG.VCCINT_ALARM {false} \
  ] $system_management_wiz_0


  # Create instance: proc_sys_reset_1, and set properties
  set proc_sys_reset_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_1 ]

  # Create instance: Mipi_1_reset, and set properties
  set Mipi_1_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 Mipi_1_reset ]

  # Create instance: util_ds_buf_0, and set properties
  set util_ds_buf_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.2 util_ds_buf_0 ]
  set_property CONFIG.C_BUF_TYPE {BUFG} $util_ds_buf_0


  # Create instance: buffer_reset, and set properties
  set buffer_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 buffer_reset ]

  # Create instance: axi_timer_0, and set properties
  set axi_timer_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_timer:2.0 axi_timer_0 ]

  # Create instance: TEMP_CONTROLLER, and set properties
  set TEMP_CONTROLLER [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 TEMP_CONTROLLER ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS {1} \
    CONFIG.C_GPIO_WIDTH {10} \
  ] $TEMP_CONTROLLER


  # Create instance: MAX_FAN_CONTROLLER_0, and set properties
  set MAX_FAN_CONTROLLER_0 [ create_bd_cell -type ip -vlnv user.org:user:MAX_FAN_CONTROLLER:1.0 MAX_FAN_CONTROLLER_0 ]

  # Create instance: csi_byte_metadata, and set properties
  set csi_byte_metadata [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_subset_converter:1.1 csi_byte_metadata ]
  set_property -dict [list \
    CONFIG.M_HAS_TKEEP {1} \
    CONFIG.M_HAS_TLAST {1} \
    CONFIG.M_HAS_TREADY {1} \
    CONFIG.M_HAS_TSTRB {0} \
    CONFIG.M_TDATA_NUM_BYTES {4} \
    CONFIG.M_TDEST_WIDTH {10} \
    CONFIG.M_TID_WIDTH {0} \
    CONFIG.M_TUSER_WIDTH {4} \
    CONFIG.S_HAS_TKEEP {1} \
    CONFIG.S_HAS_TLAST {1} \
    CONFIG.S_HAS_TREADY {1} \
    CONFIG.S_HAS_TSTRB {0} \
    CONFIG.S_TDATA_NUM_BYTES {4} \
    CONFIG.S_TDEST_WIDTH {10} \
    CONFIG.S_TID_WIDTH {0} \
    CONFIG.S_TUSER_WIDTH {96} \
    CONFIG.TDATA_REMAP {tdata[31:0]} \
    CONFIG.TKEEP_REMAP {tkeep[3:0]} \
    CONFIG.TUSER_REMAP {3'b000,tuser[0]} \
  ] $csi_byte_metadata


  # Create instance: csi_stream_reset, and set properties
  set csi_stream_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 csi_stream_reset ]
  set_property CONFIG.C_AUX_RESET_HIGH {0} $csi_stream_reset


  # Create instance: csi_reset_zero, and set properties
  set csi_reset_zero [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 csi_reset_zero ]
  set_property CONFIG.CONST_VAL {0} $csi_reset_zero


  # Create instance: csi_reset_one, and set properties
  set csi_reset_one [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 csi_reset_one ]
  set_property CONFIG.CONST_VAL {1} $csi_reset_one


  # Create instance: camera_power_gpio, and set properties
  set camera_power_gpio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 camera_power_gpio ]
  set_property -dict [list \
    CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_DOUT_DEFAULT {0x00000001} \
    CONFIG.C_GPIO_WIDTH {1} \
  ] $camera_power_gpio


  # Create instance: camera_sccb_gpio, and set properties
  set camera_sccb_gpio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 camera_sccb_gpio ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS {0} \
    CONFIG.C_ALL_OUTPUTS {0} \
    CONFIG.C_ALL_OUTPUTS_2 {1} \
    CONFIG.C_DOUT_DEFAULT {0x00000000} \
    CONFIG.C_DOUT_DEFAULT_2 {0x00000000} \
    CONFIG.C_GPIO2_WIDTH {2} \
    CONFIG.C_GPIO_WIDTH {2} \
    CONFIG.C_IS_DUAL {1} \
    CONFIG.C_TRI_DEFAULT {0x00000003} \
  ] $camera_sccb_gpio


  # Create instance: raw10_repack, and set properties
  set block_name axis_raw10_repack
  set block_cell_name raw10_repack
  if { [catch {set raw10_repack [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $raw10_repack eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: raw_isp_bridge, and set properties
  set block_name axis_camera_isp_bridge_v9
  set block_cell_name raw_isp_bridge
  if { [catch {set raw_isp_bridge [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $raw_isp_bridge eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: raw_isp, and set properties
  set raw_isp [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_raw_correct_v5:1.0 raw_isp ]

  # Create instance: color_isp, and set properties
  set color_isp [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_color_pipeline_v9:1.0 color_isp ]

  # Create instance: scene_stats, and set properties
  set scene_stats [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_scene_metrics_v8:1.0 scene_stats ]

  # Create instance: rgb_output, and set properties
  set rgb_output [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_rgb_output:1.0 rgb_output ]

  # Create instance: polyphase_route, and set properties
  set block_name axis_rgb_polyphase_route
  set block_cell_name polyphase_route
  if { [catch {set polyphase_route [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $polyphase_route eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: local_contrast, and set properties
  set local_contrast [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_local_contrast:1.0 local_contrast ]

  # Create instance: polyphase, and set properties
  set polyphase [ create_bd_cell -type ip -vlnv xilinx.com:ip:v_proc_ss:2.3 polyphase ]
  set_property -dict [list \
    CONFIG.C_COLORSPACE_SUPPORT {2} \
    CONFIG.C_ENABLE_CSC {false} \
    CONFIG.C_H_SCALER_PHASES {64} \
    CONFIG.C_H_SCALER_TAPS {6} \
    CONFIG.C_MAX_COLS {2592} \
    CONFIG.C_MAX_DATA_WIDTH {8} \
    CONFIG.C_MAX_ROWS {1944} \
    CONFIG.C_NUM_VIDEO_COMPONENTS {3} \
    CONFIG.C_SAMPLES_PER_CLK {1} \
    CONFIG.C_SCALER_ALGORITHM {2} \
    CONFIG.C_TOPOLOGY {0} \
    CONFIG.C_V_SCALER_PHASES {64} \
    CONFIG.C_V_SCALER_TAPS {6} \
  ] $polyphase


  # Create instance: raw_isp_gpio, and set properties
  set raw_isp_gpio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 raw_isp_gpio ]
  set_property -dict [list \
    CONFIG.C_ALL_INPUTS {1} \
    CONFIG.C_ALL_OUTPUTS_2 {1} \
    CONFIG.C_DOUT_DEFAULT_2 {0x00000000} \
    CONFIG.C_GPIO2_WIDTH {4} \
    CONFIG.C_GPIO_WIDTH {32} \
    CONFIG.C_IS_DUAL {1} \
  ] $raw_isp_gpio


  # Create instance: raw_isp_signature, and set properties
  set raw_isp_signature [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 raw_isp_signature ]
  set_property -dict [list \
    CONFIG.CONST_VAL {0x4953500D} \
    CONFIG.CONST_WIDTH {32} \
  ] $raw_isp_signature


  # Create instance: isp_mode, and set properties
  set isp_mode [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 isp_mode ]
  set_property -dict [list \
    CONFIG.DIN_FROM {1} \
    CONFIG.DIN_TO {0} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {2} \
  ] $isp_mode


  # Create instance: isp_bgr, and set properties
  set isp_bgr [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 isp_bgr ]
  set_property -dict [list \
    CONFIG.DIN_FROM {2} \
    CONFIG.DIN_TO {2} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $isp_bgr


  # Create instance: isp_polyphase, and set properties
  set isp_polyphase [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 isp_polyphase ]
  set_property -dict [list \
    CONFIG.DIN_FROM {3} \
    CONFIG.DIN_TO {3} \
    CONFIG.DIN_WIDTH {4} \
    CONFIG.DOUT_WIDTH {1} \
  ] $isp_polyphase


  # Create instance: adaptive_tone, and set properties
  set adaptive_tone [ create_bd_cell -type ip -vlnv xilinx.com:hls:camera_adaptive_tone:1.0 adaptive_tone ]

  # Create instance: tone_control, and set properties
  set tone_control [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 tone_control ]
  set_property -dict [list \
    CONFIG.NUM_MI {3} \
    CONFIG.NUM_SI {1} \
  ] $tone_control


  # Create instance: integer_remap, and set properties
  set integer_remap [ create_bd_cell -type ip -vlnv xilinx.com:hls:integer_remap:1.0 integer_remap ]

  # Create instance: writer_byte_adapter, and set properties
  set block_name axis_byte_to_writer_ppc4
  set block_cell_name writer_byte_adapter
  if { [catch {set writer_byte_adapter [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $writer_byte_adapter eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create interface connections
  connect_bd_intf_net -intf_net adaptive_tone_output_r [get_bd_intf_pins adaptive_tone/output_r] [get_bd_intf_pins scene_stats/input_r]
  connect_bd_intf_net -intf_net axi_interconnect_1_M00_AXI [get_bd_intf_pins axi_interconnect_1/M00_AXI] [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP1_FPD]
  connect_bd_intf_net -intf_net camera_sccb_gpio_GPIO [get_bd_intf_pins camera_sccb_gpio/GPIO] [get_bd_intf_ports camera_sccb]
  connect_bd_intf_net -intf_net color_isp_output_r [get_bd_intf_pins color_isp/output_r] [get_bd_intf_pins polyphase_route/S_AXIS]
  connect_bd_intf_net -intf_net csi_byte_metadata_M_AXIS [get_bd_intf_pins csi_byte_metadata/M_AXIS] [get_bd_intf_pins raw_isp_bridge/S_AXIS]
  connect_bd_intf_net -intf_net integer_remap_m_axi_map_memory [get_bd_intf_pins integer_remap/m_axi_map_memory] [get_bd_intf_pins axi_interconnect_1/S01_AXI]
  connect_bd_intf_net -intf_net integer_remap_output_r [get_bd_intf_pins integer_remap/output_r] [get_bd_intf_pins local_contrast/input_r]
  connect_bd_intf_net -intf_net local_contrast_output_r [get_bd_intf_pins local_contrast/output_r] [get_bd_intf_pins adaptive_tone/input_r]
  connect_bd_intf_net -intf_net mipi_csi2_rx_subsyst_1_video_out [get_bd_intf_pins mipi_csi2_rx_subsyst_1/video_out] [get_bd_intf_pins csi_byte_metadata/S_AXIS]
  connect_bd_intf_net -intf_net mipi_phy_if_1_1 [get_bd_intf_ports mipi_phy_if_1] [get_bd_intf_pins mipi_csi2_rx_subsyst_1/mipi_phy_if]
  connect_bd_intf_net -intf_net polyphase_m_axis [get_bd_intf_pins polyphase/m_axis] [get_bd_intf_pins polyphase_route/FROM_SCALER]
  connect_bd_intf_net -intf_net polyphase_route_M_AXIS [get_bd_intf_pins polyphase_route/M_AXIS] [get_bd_intf_pins integer_remap/input_r]
  connect_bd_intf_net -intf_net polyphase_route_TO_SCALER [get_bd_intf_pins polyphase_route/TO_SCALER] [get_bd_intf_pins polyphase/s_axis]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M00_AXI [get_bd_intf_pins ps8_0_axi_periph/M00_AXI] [get_bd_intf_pins mipi_csi2_rx_subsyst_1/csirxss_s_axi]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M01_AXI [get_bd_intf_pins ps8_0_axi_periph/M01_AXI] [get_bd_intf_pins v_frmbuf_wr_1/s_axi_CTRL]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M02_AXI [get_bd_intf_pins ps8_0_axi_periph/M02_AXI] [get_bd_intf_pins LEDS_CONTROLLER/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M03_AXI [get_bd_intf_pins ps8_0_axi_periph/M03_AXI] [get_bd_intf_pins system_management_wiz_0/S_AXI_LITE]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M04_AXI [get_bd_intf_pins ps8_0_axi_periph/M04_AXI] [get_bd_intf_pins TEMP_CONTROLLER/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M05_AXI [get_bd_intf_pins ps8_0_axi_periph/M05_AXI] [get_bd_intf_pins MAX_FAN_CONTROLLER_0/S00_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M06_AXI [get_bd_intf_pins ps8_0_axi_periph/M06_AXI] [get_bd_intf_pins axi_timer_0/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M07_AXI [get_bd_intf_pins ps8_0_axi_periph/M07_AXI] [get_bd_intf_pins camera_power_gpio/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M08_AXI [get_bd_intf_pins ps8_0_axi_periph/M08_AXI] [get_bd_intf_pins camera_sccb_gpio/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M09_AXI [get_bd_intf_pins ps8_0_axi_periph/M09_AXI] [get_bd_intf_pins raw_isp/s_axi_control]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M10_AXI [get_bd_intf_pins ps8_0_axi_periph/M10_AXI] [get_bd_intf_pins raw_isp_gpio/S_AXI]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M11_AXI [get_bd_intf_pins ps8_0_axi_periph/M11_AXI] [get_bd_intf_pins color_isp/s_axi_control]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M12_AXI [get_bd_intf_pins ps8_0_axi_periph/M12_AXI] [get_bd_intf_pins scene_stats/s_axi_control]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M13_AXI [get_bd_intf_pins ps8_0_axi_periph/M13_AXI] [get_bd_intf_pins rgb_output/s_axi_control]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M14_AXI [get_bd_intf_pins ps8_0_axi_periph/M14_AXI] [get_bd_intf_pins polyphase/s_axi_ctrl]
  connect_bd_intf_net -intf_net ps8_0_axi_periph_M15_AXI [get_bd_intf_pins ps8_0_axi_periph/M15_AXI] [get_bd_intf_pins tone_control/S00_AXI]
  connect_bd_intf_net -intf_net raw10_repack_M_AXIS [get_bd_intf_pins raw10_repack/M_AXIS] [get_bd_intf_pins writer_byte_adapter/S_AXIS]
  connect_bd_intf_net -intf_net raw_isp_bridge_M_AXIS [get_bd_intf_pins raw_isp_bridge/M_AXIS] [get_bd_intf_pins raw10_repack/S_AXIS]
  connect_bd_intf_net -intf_net raw_isp_bridge_TO_ISP [get_bd_intf_pins raw_isp_bridge/TO_ISP] [get_bd_intf_pins raw_isp/input_r]
  connect_bd_intf_net -intf_net raw_isp_bridge_TO_RGB [get_bd_intf_pins raw_isp_bridge/TO_RGB] [get_bd_intf_pins color_isp/input_r]
  connect_bd_intf_net -intf_net raw_isp_output_r [get_bd_intf_pins raw_isp/output_r] [get_bd_intf_pins raw_isp_bridge/FROM_ISP]
  connect_bd_intf_net -intf_net rgb_output_output_r [get_bd_intf_pins rgb_output/output_r] [get_bd_intf_pins raw_isp_bridge/FROM_RGB]
  connect_bd_intf_net -intf_net scene_stats_output_r [get_bd_intf_pins scene_stats/output_r] [get_bd_intf_pins rgb_output/input_r]
  connect_bd_intf_net -intf_net tone_control_M00_AXI [get_bd_intf_pins tone_control/M00_AXI] [get_bd_intf_pins local_contrast/s_axi_control]
  connect_bd_intf_net -intf_net tone_control_M01_AXI [get_bd_intf_pins tone_control/M01_AXI] [get_bd_intf_pins adaptive_tone/s_axi_control]
  connect_bd_intf_net -intf_net tone_control_M02_AXI [get_bd_intf_pins tone_control/M02_AXI] [get_bd_intf_pins integer_remap/s_axi_control]
  connect_bd_intf_net -intf_net v_frmbuf_wr_1_m_axi_mm_video [get_bd_intf_pins v_frmbuf_wr_1/m_axi_mm_video] [get_bd_intf_pins axi_interconnect_1/S00_AXI]
  connect_bd_intf_net -intf_net writer_byte_adapter_M_AXIS [get_bd_intf_pins writer_byte_adapter/M_AXIS] [get_bd_intf_pins v_frmbuf_wr_1/s_axis_video]
  connect_bd_intf_net -intf_net zynq_ultra_ps_e_0_M_AXI_HPM0_FPD [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_FPD] [get_bd_intf_pins ps8_0_axi_periph/S00_AXI]

  # Create port connections
  connect_bd_net -net ARESETN_1  [get_bd_pins proc_sys_reset_0/interconnect_aresetn] \
  [get_bd_pins ps8_0_axi_periph/ARESETN] \
  [get_bd_pins axi_interconnect_1/ARESETN] \
  [get_bd_pins LEDS_CONTROLLER/s_axi_aresetn]
  connect_bd_net -net AXI_FUN_CONTROLLER_0_fan_state  [get_bd_pins MAX_FAN_CONTROLLER_0/fan_state] \
  [get_bd_ports fan_ctrl]
  connect_bd_net -net LEDS_CONTROLLER_gpio_io_o  [get_bd_pins LEDS_CONTROLLER/gpio_io_o] \
  [get_bd_ports leds_ctrl]
  connect_bd_net -net camera_power_gpio_gpio_io_o  [get_bd_pins camera_power_gpio/gpio_io_o] \
  [get_bd_ports cam0_pwup_control]
  connect_bd_net -net camera_sccb_gpio_gpio2_io_o  [get_bd_pins camera_sccb_gpio/gpio2_io_o] \
  [get_bd_pins raw10_repack/mode]
  connect_bd_net -net clk_wiz_0_clk_out1  [get_bd_pins clk_wiz_0/clk_out1] \
  [get_bd_pins ps8_0_axi_periph/S00_ACLK] \
  [get_bd_pins proc_sys_reset_0/slowest_sync_clk] \
  [get_bd_pins mipi_csi2_rx_subsyst_1/lite_aclk] \
  [get_bd_pins mipi_csi2_rx_subsyst_1/dphy_clk_200M] \
  [get_bd_pins mipi_csi2_rx_subsyst_1/video_aclk] \
  [get_bd_pins ps8_0_axi_periph/ACLK] \
  [get_bd_pins v_frmbuf_wr_1/ap_clk] \
  [get_bd_pins axi_interconnect_1/ACLK] \
  [get_bd_pins axi_interconnect_1/S00_ACLK] \
  [get_bd_pins axi_interconnect_1/M00_ACLK] \
  [get_bd_pins zynq_ultra_ps_e_0/saxihp1_fpd_aclk] \
  [get_bd_pins LEDS_CONTROLLER/s_axi_aclk] \
  [get_bd_pins system_management_wiz_0/s_axi_aclk] \
  [get_bd_pins zynq_ultra_ps_e_0/maxihpm0_fpd_aclk] \
  [get_bd_pins ps8_0_axi_periph/S01_ACLK] \
  [get_bd_pins axi_timer_0/s_axi_aclk] \
  [get_bd_pins TEMP_CONTROLLER/s_axi_aclk] \
  [get_bd_pins MAX_FAN_CONTROLLER_0/s00_axi_aclk] \
  [get_bd_pins csi_byte_metadata/aclk] \
  [get_bd_pins csi_stream_reset/slowest_sync_clk] \
  [get_bd_pins camera_power_gpio/s_axi_aclk] \
  [get_bd_pins camera_sccb_gpio/s_axi_aclk] \
  [get_bd_pins raw10_repack/aclk] \
  [get_bd_pins ps8_0_axi_periph/M00_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M01_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M02_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M03_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M04_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M05_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M06_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M07_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M08_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M09_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M10_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M11_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M12_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M13_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M14_ACLK] \
  [get_bd_pins ps8_0_axi_periph/M15_ACLK] \
  [get_bd_pins raw_isp/ap_clk] \
  [get_bd_pins color_isp/ap_clk] \
  [get_bd_pins scene_stats/ap_clk] \
  [get_bd_pins local_contrast/ap_clk] \
  [get_bd_pins rgb_output/ap_clk] \
  [get_bd_pins raw_isp_bridge/aclk] \
  [get_bd_pins polyphase_route/aclk] \
  [get_bd_pins polyphase/aclk_axis] \
  [get_bd_pins polyphase/aclk_ctrl] \
  [get_bd_pins raw_isp_gpio/s_axi_aclk] \
  [get_bd_pins tone_control/ACLK] \
  [get_bd_pins tone_control/S00_ACLK] \
  [get_bd_pins tone_control/M00_ACLK] \
  [get_bd_pins tone_control/M01_ACLK] \
  [get_bd_pins adaptive_tone/ap_clk] \
  [get_bd_pins tone_control/M02_ACLK] \
  [get_bd_pins axi_interconnect_1/S01_ACLK] \
  [get_bd_pins integer_remap/ap_clk] \
  [get_bd_pins writer_byte_adapter/aclk]
  connect_bd_net -net csi_reset_gpio_bit_Dout  [get_bd_pins LEDS_CONTROLLER/gpio2_io_o] \
  [get_bd_pins csi_stream_reset/ext_reset_in]
  connect_bd_net -net csi_reset_one_dout  [get_bd_pins csi_reset_one/dout] \
  [get_bd_pins csi_stream_reset/dcm_locked]
  connect_bd_net -net csi_reset_zero_dout  [get_bd_pins csi_reset_zero/dout] \
  [get_bd_pins csi_stream_reset/mb_debug_sys_rst] \
  [get_bd_pins xlconcat_0/In0] \
  [get_bd_pins xlconcat_0/In1] \
  [get_bd_pins xlconcat_0/In2] \
  [get_bd_pins xlconcat_0/In4]
  connect_bd_net -net csi_stream_reset_peripheral_aresetn  [get_bd_pins csi_stream_reset/peripheral_aresetn] \
  [get_bd_pins v_frmbuf_wr_1/ap_rst_n] \
  [get_bd_pins csi_byte_metadata/aresetn] \
  [get_bd_pins mipi_csi2_rx_subsyst_1/video_aresetn] \
  [get_bd_pins raw10_repack/aresetn] \
  [get_bd_pins raw_isp/ap_rst_n] \
  [get_bd_pins color_isp/ap_rst_n] \
  [get_bd_pins scene_stats/ap_rst_n] \
  [get_bd_pins local_contrast/ap_rst_n] \
  [get_bd_pins rgb_output/ap_rst_n] \
  [get_bd_pins raw_isp_bridge/aresetn] \
  [get_bd_pins polyphase_route/aresetn] \
  [get_bd_pins adaptive_tone/ap_rst_n] \
  [get_bd_pins integer_remap/ap_rst_n] \
  [get_bd_pins writer_byte_adapter/aresetn]
  connect_bd_net -net isp_bgr_Dout  [get_bd_pins isp_bgr/Dout] \
  [get_bd_pins raw_isp_bridge/bgr]
  connect_bd_net -net isp_mode_Dout  [get_bd_pins isp_mode/Dout] \
  [get_bd_pins raw_isp_bridge/mode]
  connect_bd_net -net isp_polyphase_Dout  [get_bd_pins isp_polyphase/Dout] \
  [get_bd_pins polyphase_route/enable]
  connect_bd_net -net mipi_csi2_rx_subsyst_1_clkoutphy_out  [get_bd_pins mipi_csi2_rx_subsyst_1/clkoutphy_out] \
  [get_bd_pins Mipi_1_reset/slowest_sync_clk]
  connect_bd_net -net mipi_csi2_rx_subsyst_1_csirxss_csi_irq  [get_bd_pins mipi_csi2_rx_subsyst_1/csirxss_csi_irq] \
  [get_bd_pins xlconcat_0/In3]
  connect_bd_net -net pl_ref_clk_1  [get_bd_ports input_clock] \
  [get_bd_pins proc_sys_reset_1/slowest_sync_clk] \
  [get_bd_pins util_ds_buf_0/BUFG_I]
  connect_bd_net -net proc_sys_reset_0_peripheral_aresetn  [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
  [get_bd_pins axi_interconnect_1/M00_ARESETN] \
  [get_bd_pins axi_interconnect_1/S00_ARESETN] \
  [get_bd_pins mipi_csi2_rx_subsyst_1/lite_aresetn] \
  [get_bd_pins ps8_0_axi_periph/S00_ARESETN] \
  [get_bd_pins system_management_wiz_0/s_axi_aresetn] \
  [get_bd_pins ps8_0_axi_periph/S01_ARESETN] \
  [get_bd_pins axi_timer_0/s_axi_aresetn] \
  [get_bd_pins TEMP_CONTROLLER/s_axi_aresetn] \
  [get_bd_pins MAX_FAN_CONTROLLER_0/s00_axi_aresetn] \
  [get_bd_pins csi_stream_reset/aux_reset_in] \
  [get_bd_pins camera_power_gpio/s_axi_aresetn] \
  [get_bd_pins camera_sccb_gpio/s_axi_aresetn] \
  [get_bd_pins ps8_0_axi_periph/M00_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M01_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M02_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M03_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M04_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M05_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M06_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M07_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M08_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M09_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M10_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M11_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M12_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M13_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M14_ARESETN] \
  [get_bd_pins ps8_0_axi_periph/M15_ARESETN] \
  [get_bd_pins raw_isp_gpio/s_axi_aresetn] \
  [get_bd_pins polyphase/aresetn_ctrl] \
  [get_bd_pins tone_control/ARESETN] \
  [get_bd_pins tone_control/S00_ARESETN] \
  [get_bd_pins tone_control/M00_ARESETN] \
  [get_bd_pins tone_control/M01_ARESETN] \
  [get_bd_pins tone_control/M02_ARESETN] \
  [get_bd_pins axi_interconnect_1/S01_ARESETN]
  connect_bd_net -net raw_isp_gpio_gpio2_io_o  [get_bd_pins raw_isp_gpio/gpio2_io_o] \
  [get_bd_pins isp_polyphase/Din] \
  [get_bd_pins isp_mode/Din] \
  [get_bd_pins isp_bgr/Din]
  connect_bd_net -net raw_isp_signature_dout  [get_bd_pins raw_isp_signature/dout] \
  [get_bd_pins raw_isp_gpio/gpio_io_i]
  connect_bd_net -net system_management_wiz_0_temp_out  [get_bd_pins system_management_wiz_0/temp_out] \
  [get_bd_pins TEMP_CONTROLLER/gpio_io_i] \
  [get_bd_pins MAX_FAN_CONTROLLER_0/temp]
  connect_bd_net -net util_ds_buf_0_BUFG_O  [get_bd_pins util_ds_buf_0/BUFG_O] \
  [get_bd_pins clk_wiz_0/clk_in1] \
  [get_bd_pins buffer_reset/slowest_sync_clk]
  connect_bd_net -net v_frmbuf_wr_1_interrupt  [get_bd_pins v_frmbuf_wr_1/interrupt] \
  [get_bd_pins xlconcat_0/In5]
  connect_bd_net -net xlconcat_0_dout  [get_bd_pins xlconcat_0/dout] \
  [get_bd_pins zynq_ultra_ps_e_0/pl_ps_irq0]
  connect_bd_net -net zynq_ultra_ps_e_0_pl_resetn0  [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0] \
  [get_bd_pins proc_sys_reset_0/ext_reset_in] \
  [get_bd_pins proc_sys_reset_1/ext_reset_in] \
  [get_bd_pins Mipi_1_reset/ext_reset_in] \
  [get_bd_pins buffer_reset/ext_reset_in]

  # Create address segments
  assign_bd_address -offset 0xA0010000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs LEDS_CONTROLLER/S_AXI/Reg] -force
  assign_bd_address -offset 0xA0000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs MAX_FAN_CONTROLLER_0/S00_AXI/S00_AXI_reg] -force
  assign_bd_address -offset 0xA0060000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs TEMP_CONTROLLER/S_AXI/Reg] -force
  assign_bd_address -offset 0xA0180000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs adaptive_tone/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA00B0000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs axi_timer_0/S_AXI/Reg] -force
  assign_bd_address -offset 0xA00C0000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs camera_power_gpio/S_AXI/Reg] -force
  assign_bd_address -offset 0xA00D0000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs camera_sccb_gpio/S_AXI/Reg] -force
  assign_bd_address -offset 0xA0100000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs color_isp/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA0190000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs integer_remap/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA0130000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs local_contrast/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA0041000 -range 0x00001000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs mipi_csi2_rx_subsyst_1/csirxss_s_axi/Reg] -force
  assign_bd_address -offset 0xA0140000 -range 0x00040000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs polyphase/s_axi_ctrl/Reg] -force
  assign_bd_address -offset 0xA00E0000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs raw_isp/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA00F0000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs raw_isp_gpio/S_AXI/Reg] -force
  assign_bd_address -offset 0xA0120000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs rgb_output/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA0110000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs scene_stats/s_axi_control/Reg] -force
  assign_bd_address -offset 0xA0050000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs system_management_wiz_0/S_AXI_LITE/Reg] -force
  assign_bd_address -offset 0xA0080000 -range 0x00010000 -target_address_space [get_bd_addr_spaces zynq_ultra_ps_e_0/Data] [get_bd_addr_segs v_frmbuf_wr_1/s_axi_CTRL/Reg] -force
  assign_bd_address -offset 0x00000000 -range 0x80000000 -target_address_space [get_bd_addr_spaces v_frmbuf_wr_1/Data_m_axi_mm_video] [get_bd_addr_segs zynq_ultra_ps_e_0/SAXIGP3/HP1_DDR_LOW] -force
  assign_bd_address -offset 0xE0000000 -range 0x10000000 -target_address_space [get_bd_addr_spaces v_frmbuf_wr_1/Data_m_axi_mm_video] [get_bd_addr_segs zynq_ultra_ps_e_0/SAXIGP3/HP1_PCIE_LOW] -force
  assign_bd_address -offset 0xC0000000 -range 0x20000000 -target_address_space [get_bd_addr_spaces v_frmbuf_wr_1/Data_m_axi_mm_video] [get_bd_addr_segs zynq_ultra_ps_e_0/SAXIGP3/HP1_QSPI] -force
  assign_bd_address -offset 0x00000000 -range 0x80000000 -target_address_space [get_bd_addr_spaces integer_remap/Data_m_axi_map_memory] [get_bd_addr_segs zynq_ultra_ps_e_0/SAXIGP3/HP1_DDR_LOW] -force

  # Exclude Address Segments
  exclude_bd_addr_seg -offset 0xFF000000 -range 0x01000000 -target_address_space [get_bd_addr_spaces v_frmbuf_wr_1/Data_m_axi_mm_video] [get_bd_addr_segs zynq_ultra_ps_e_0/SAXIGP3/HP1_LPS_OCM]


  # Restore current instance
  current_bd_instance $oldCurInst

  # Create PFM attributes
  set_property PFM_NAME {vendor:board:name:1.0} [get_files [current_bd_design].bd]
  set_property PFM.CLOCK {pl_ref_clk {id "16" is_default "true" proc_sys_reset "/proc_sys_reset_1" status "fixed" freq_hz "25000000"}} [get_bd_ports /input_clock]
  set_property PFM.AXI_PORT {M_AXI_HPM0_LPD {memport "M_AXI_GP" sptag "" memory "" is_range "false"}} [get_bd_cells /zynq_ultra_ps_e_0]
  set_property PFM.CLOCK {clk_out1 {id "17" is_default "false" proc_sys_reset "/proc_sys_reset_0" status "fixed" freq_hz "200000000"}} [get_bd_cells /clk_wiz_0]
  set_property PFM.CLOCK {clkoutphy_out {id "14" is_default "false" proc_sys_reset "/Mipi_1_reset" status "fixed" freq_hz "1500000000.0"}} [get_bd_cells /mipi_csi2_rx_subsyst_1]
  set_property PFM.CLOCK {BUFG_O {id "20" is_default "false" proc_sys_reset "/buffer_reset" status "fixed" freq_hz "25000000"}} [get_bd_cells /util_ds_buf_0]


  validate_bd_design
  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


