module fan_controller#(
    parameter integer CLK_FREQ_HZ = 200_000_000,
    parameter integer PWM_FREQ_HZ = 25_000
)(
    input  wire        clk,
    input  wire        reset_n,
    input  wire [9:0]  temp,
    input  wire [9:0]  temp_thrashold,
    input  wire [9:0]  temp_floor_thrashold,
    input  wire [7:0]  duty_pct,   // 0..100
    output wire        fan_state
);

    localparam integer PERIOD = CLK_FREQ_HZ / PWM_FREQ_HZ;
    localparam integer CNT_W  = $clog2(PERIOD);

    reg [CNT_W-1:0] counter;
    reg             logic_fan_state;
    reg             fan_state_r;

    assign fan_state = fan_state_r;

    // PWM counter
    always @(posedge clk) begin
        if (!reset_n)
            counter <= {CNT_W{1'b0}};
        else if (counter == PERIOD-1)
            counter <= {CNT_W{1'b0}};
        else
            counter <= counter + 1'b1;
    end

    // Fan enable logic (with hysteresis)
    always @(posedge clk) begin
        if (!reset_n)
            logic_fan_state <= 1'b0;
        else if (logic_fan_state && temp <= temp_floor_thrashold)
            logic_fan_state <= 1'b0;
        else if (!logic_fan_state && temp >= temp_thrashold)
            logic_fan_state <= 1'b1;
    end

    // PWM output (REGISTERED, no division)
    always @(posedge clk) begin
        if (!reset_n)
            fan_state_r <= 1'b0;
        else if (logic_fan_state)
            fan_state_r <= (counter * 7'd100 < duty_pct * PERIOD);
        else
            fan_state_r <= 1'b0;
    end

endmodule
