module top(
    input  logic       clk,      // FPGA clock
    input  logic       rst_n,    // active-low reset
    output logic [7:0] led
);

    logic [23:0] div_count;   // clock divider counter
    logic        slow_clk;    // slower pulse
    logic [7:0]  pattern;     // LED pattern

    // Clock divider: generate a slow pulse
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            div_count <= 24'd0;
            slow_clk  <= 1'b0;
        end else begin
            if (div_count == 24'd9_999_999) begin
                div_count <= 24'd0;
                slow_clk  <= 1'b1;
            end else begin
                div_count <= div_count + 1'b1;
                slow_clk  <= 1'b0;
            end
        end
    end

    // Shift LED pattern on each slow pulse
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            pattern <= 8'b0000_0001;
        end else if (slow_clk) begin
            pattern <= {pattern[6:0], pattern[7]}; // rotate left
        end
    end

    // Drive LEDs
    assign led = pattern;

endmodule
