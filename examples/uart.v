module uart_tx;

input clock;
input reset;
input [7:0] data;
input start;
output tx;

register [3:0] bit_idx;
register [9:0] shift_reg;
register busy;

always @(posedge clock or posedge reset) begin
    if (reset) begin
        shift_reg <= 10'h3FF;
        bit_idx <= 0;
        busy <= 0;
    end else if (start && !busy) begin
        shift_reg <= {1'b1, data, 1'b0};
        bit_idx <= 0;
        busy <= 1;
    end else if (busy) begin
        shift_reg <= {1'b1, shift_reg[9:1]};
        bit_idx <= bit_idx + 1;
        if (bit_idx == 9) busy <= 0;
    end
end

assign tx = shift_reg[0];

endmodule
