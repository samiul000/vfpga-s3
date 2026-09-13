module shift2;

input clock;
input en;
input din;
output q;

register s0;
register s1;

always @(posedge clock) begin
    if (en) s0 <= din;
    else s0 <= s0;
end

always @(posedge clock) begin
    if (en) s1 <= s0;
    else s1 <= s0;
end

assign q = s1;

endmodule
