module full_adder;

input a;
input b;
input cin;
output s;
output cout;

wire ab;
wire t1;
wire t2;

assign ab = a ^ b;
assign s = ab ^ cin;
assign t1 = a & b;
assign t2 = cin & ab;
assign cout = t1 | t2;

endmodule
