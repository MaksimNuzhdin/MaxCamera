set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]

set_property PACKAGE_PIN AB11 [get_ports pl_ref_clk]
set_property IOSTANDARD LVCMOS18 [get_ports pl_ref_clk]
create_clock -period 40.000 -name pl_ref_clk -waveform {0.000 20.000} [get_ports pl_ref_clk]


set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

# Buttons

set_property PACKAGE_PIN AA13 [get_ports { btns_tri_i[0] }]
set_property PACKAGE_PIN AE14 [get_ports { btns_tri_i[1] }]
set_property PACKAGE_PIN AE15 [get_ports { btns_tri_i[2] }]
set_property PACKAGE_PIN AG14 [get_ports { btns_tri_i[3] }]

set_false_path -through [get_ports btns_tri_i[*]]

set_property IOSTANDARD LVCMOS33 [get_ports {btns_tri_i[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {btns_tri_i[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {btns_tri_i[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {btns_tri_i[3]}]


#leds

set_property PACKAGE_PIN W13    [get_ports { leds_tri_o[0] }]
set_property PACKAGE_PIN Y12	[get_ports { leds_tri_o[1] }]
set_property PACKAGE_PIN AA12   [get_ports { leds_tri_o[2] }]
set_property PACKAGE_PIN AB13   [get_ports { leds_tri_o[3] }]

set_property IOSTANDARD LVCMOS33 [get_ports {leds_tri_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {leds_tri_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {leds_tri_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {leds_tri_o[3]}]

# FAN controller

set_property PACKAGE_PIN W12     [get_ports fan_pwm]
set_property IOSTANDARD LVCMOS33 [get_ports fan_pwm]

set_property PACKAGE_PIN AH13 [get_ports cam0_i2c_scl_io]
set_property PACKAGE_PIN AE13 [get_ports cam0_i2c_sda_io]

set_property IOSTANDARD LVCMOS33 [get_ports cam0_i2c_scl_io]
set_property IOSTANDARD LVCMOS33 [get_ports cam0_i2c_sda_io]


set_property PULLUP true [get_ports cam0_i2c_scl_io]
set_property PULLUP true [get_ports cam0_i2c_sda_io]

set_property PACKAGE_PIN AG13 [get_ports cam_0_xclk]
set_property IOSTANDARD LVCMOS33 [get_ports cam_0_xclk]







set_property PACKAGE_PIN AH14 [get_ports {cam0_gpio[0]}]
set_property PULLUP true [get_ports {cam0_gpio[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {cam0_gpio[0]}]



#set_false_path -from [get_ports leds_tri_o[*]]


set_false_path -through [get_ports {cam0_i2c_scl_io cam0_i2c_sda_io}]



