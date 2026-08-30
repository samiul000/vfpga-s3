module lfsr;

input clock;
input reset;
output [7:0] state;

register [7:0] lfsr_reg;

always @(posedge clock or posedge reset) begin
    if (reset)
        lfsr_reg <= 8'hFF;
    else begin
        lfsr_reg[7] <= lfsr_reg[0] ^ lfsr_reg[5];
        lfsr_reg[6] <= lfsr_reg[7];
        lfsr_reg[5] <= lfsr_reg[6];
        lfsr_reg[4] <= lfsr_reg[5];
        lfsr_reg[3] <= lfsr_reg[4];
        lfsr_reg[2] <= lfsr_reg[3];
        lfsr_reg[1] <= lfsr_reg[2];
        lfsr_reg[0] <= lfsr_reg[1];
    end
end

assign state = lfsr_reg;

endmodule
