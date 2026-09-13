module counter;

input clock;
input reset;
output [3:0] count;

register [3:0] cnt;

always @(posedge clock) begin
    if (reset) cnt <= 0;
    else cnt <= cnt + 1;
end

assign count = cnt;

endmodule
