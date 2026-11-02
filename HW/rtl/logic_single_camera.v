// Production CSI-byte pipeline: the populated connector uses CSI receiver 1.
module logic (
    input wire reset,
    input wire pl_ref_clk,
    output wire fan_pwm,
    input wire [3:0] btns_tri_i,
    output wire [3:0] leds_tri_o,
    input wire [1:0] mipi_data_1_signal_n,
    input wire [1:0] mipi_data_1_signal_p,
    input wire mipi_clock_1_signal_n,
    input wire mipi_clock_1_signal_p,
    output wire [0:0] cam0_gpio,
    inout wire cam0_i2c_scl_io,
    inout wire cam0_i2c_sda_io,
    output wire cam_0_xclk
);
assign cam_0_xclk = pl_ref_clk;
design_1_wrapper design_1_wrapper_i (
    .cam0_pwup_control(cam0_gpio),
    .fan_ctrl(fan_pwm),
    .camera_sccb_tri_io({cam0_i2c_sda_io,cam0_i2c_scl_io}),
    .leds_ctrl(leds_tri_o),
    .mipi_phy_if_1_clk_n(mipi_clock_1_signal_n),
    .mipi_phy_if_1_clk_p(mipi_clock_1_signal_p),
    .mipi_phy_if_1_data_n(mipi_data_1_signal_n),
    .mipi_phy_if_1_data_p(mipi_data_1_signal_p),
    .input_clock(pl_ref_clk)
);
endmodule
