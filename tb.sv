`default_nettype none
`timescale 1ns / 1ps

module tb;
    reg clk;
    reg rst_n;

    wire [7:0] uo_out;
    wire [7:0] uio_out;

    TopModule dut (
        .uo_out(uo_out),
        .uio_out(uio_out),
        .clk (clk),
        .rst_n (rst_n)
    );

    localparam CLK_PERIOD = 5;
    always #(CLK_PERIOD/2) clk=~clk;

    initial begin
        $dumpfile("sim.vcd");
        $dumpvars(0, tb);
    end

    initial begin
        #1 rst_n<=1'bx;
        clk<=1'bx;
        #(CLK_PERIOD*3) rst_n<=1;
        #(CLK_PERIOD*3) rst_n<=0;
        clk<=0;
        repeat(5) @(posedge clk);
        rst_n<=1;
        @(posedge clk);
        repeat(420000) @(posedge clk);
        $finish(2);
    end
endmodule