module alu(input clk, input rst,
           input [15:0] a, input [15:0] b,
           input [1:0] op,
           output [15:0] reg_result);
    reg [15:0] reg_result;
    always @(posedge clk) begin
        if (rst) begin
            reg_result <= 16'd0;
        end else begin
            if (op[0] == 1'b0) begin
                if (op[1] == 1'b0) begin
                    reg_result <= a + b;
                end else begin
                    reg_result <= a & b;
                end
            end else begin
                if (op[1] == 1'b0) begin
                    reg_result <= a | b;
                end else begin
                    reg_result <= a ^ b;
                end
            end
        end
    end
endmodule
