module counter;

input clock;
input reset;
output [7:0] count;

register [7:0] count_reg;

always @(posedge clock or posedge reset) begin
    if (reset)
        count_reg <= 8'b0;
    else
        count_reg <= count_reg + 1;
end

assign count = count_reg;

endmodule
