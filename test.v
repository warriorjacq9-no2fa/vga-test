module test (
  input  wire [7:0] ui_in,    // Dedicated inputs
  output wire [7:0] uo_out,   // Dedicated outputs
  input  wire [7:0] uio_in,   // IOs: Input path
  output wire [7:0] uio_out,  // IOs: Output path
  output wire [7:0] uio_oe,   // IOs: Enable path (active high: 0=input, 1=output)
  input  wire       ena,      // always 1 when the design is powered, so you can ignore it
  input  wire       clk,      // clock
  input  wire       rst_n     // reset_n - low to reset
);

    assign uio_oe = 8'b0;

    reg [3:0] r, g, b;
    wire [10:0] x, y;
    wire de;
    wire hsync, vsync;

    assign uo_out[7:0] = {g[3:0], r[3:0]};
    assign uio_out[3:0] = b[3:0];
    assign uio_out[7:4] = {1'b0, de, vsync, hsync};


    // ******************** GRAPHICS ********************
    vga vga (
	    .clk(clk), 
	    .hsync(hsync),
		.vsync(vsync),
		.x(x),
		.y(y),
		.de(de),
        .rst_n(rst_n)
	);


    /* verilator lint_off LATCH */
    always @(*) begin // Display logic
        if(de == 1) begin
            r = 4'hF;
            g = 4'hE;
            b = 4'hD;
        end
    end

    wire _unused = &{ui_in, uio_in, ena, x, y, 1'b0};
endmodule
