module toggle_fsm;

input clock;
input reset;
input go;
output out;

register state;

always @(posedge clock) begin
    if (reset) state <= 0;
    else state <= state ^ go;
end

assign out = state;

endmodule
