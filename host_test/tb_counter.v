// Icarus Verilog testbench for the emitted 8-bit counter.
// DUT lives in golden_counter.v (verbatim emitter output, shared with Yosys).
// Golden vector matches the fabric suite: 300 cycles -> 44 (300 & 0xFF).
// Run: iverilog -Ihost_test -o /tmp/tb_counter host_test/tb_counter.v && vvp /tmp/tb_counter
`include "golden_counter.v"

module tb_counter;
reg clock, reset;
wire [7:0] count;
counter dut(clock, reset, count);
integer i;
initial begin
    clock = 0; reset = 1;
    #10; clock = 1; #10; clock = 0; // one reset cycle clears count_reg
    reset = 0;
    for (i = 0; i < 300; i = i + 1) begin
        #10; clock = 1; #10; clock = 0;
    end
    if (count === 8'd44) $display("TB-COUNTER PASS");
    else $display("TB-COUNTER FAIL got %0d exp 44", count);
    $finish;
end
endmodule
