module dreg;

input clock;
input d;
output q;

register q_reg;

always @(posedge clock) begin
    q_reg <= d;
end

assign q = q_reg;

endmodule
