module lfsr;

input clock;
input reset;
output [7:0] state;

register [7:0] lfsr_reg;

always @(posedge clock) begin
    if (reset)
        lfsr_reg <= 255;
    else
        lfsr_reg <= lfsr_reg ^ 1;
end

assign state = lfsr_reg;

endmodule
