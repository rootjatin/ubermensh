module top(
    input  logic clk,
    input  logic rst_n,
    output logic [7:0] led
);
    logic [3:0] count;
    logic done;

    counter #(.MAX(10)) u_counter (
        .clk   (clk),
        .rst_n (rst_n),
        .count (count),
        .done  (done)
    );

    // Display: lower 4 bits = count, top bit = done
    always_comb begin
        led = 8'b0;
        led[3:0] = count;
        led[7]   = done;
    end
endmodule
