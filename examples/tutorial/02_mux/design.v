module mux2;

input clock;
input sel;
input a;
input b;
output y;

register y_reg;

always @(posedge clock) begin
    if (sel) y_reg <= a;
    else y_reg <= b;
end

assign y = y_reg;

endmodule
