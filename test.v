module test (
    input  wire clk,        // 25.125 MHz pixel clock
    input  wire rst_n,      // active-low reset
    output wire [7:0] uo_out,   // {green[3:0], red[3:0]}
    output wire [7:0] uio_out   // {vsync, hsync, blue[3:0], 2'b00}
);

    // VGA timing constants
    localparam H_VISIBLE  = 640;
    localparam H_FRONT    = 16;
    localparam H_SYNC     = 96;
    localparam H_BACK     = 48;
    localparam H_TOTAL    = H_VISIBLE + H_FRONT + H_SYNC + H_BACK; // 800

    localparam V_VISIBLE  = 480;
    localparam V_FRONT    = 10;
    localparam V_SYNC     = 2;
    localparam V_BACK     = 33;
    localparam V_TOTAL    = V_VISIBLE + V_FRONT + V_SYNC + V_BACK; // 525

    // Counters
    reg [9:0] h_cnt;
    reg [9:0] v_cnt;

    // Horizontal counter
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            h_cnt <= 0;
        else if (h_cnt == H_TOTAL - 1)
            h_cnt <= 0;
        else
            h_cnt <= h_cnt + 1;
    end

    // Vertical counter
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            v_cnt <= 0;
        else if (h_cnt == H_TOTAL - 1) begin
            if (v_cnt == V_TOTAL - 1)
                v_cnt <= 0;
            else
                v_cnt <= v_cnt + 1;
        end
    end

    // Generate sync signals (active low)
    wire hsync = ~((h_cnt >= (H_VISIBLE + H_FRONT)) &&
                   (h_cnt <  (H_VISIBLE + H_FRONT + H_SYNC)));

    wire vsync = ~((v_cnt >= (V_VISIBLE + V_FRONT)) &&
                   (v_cnt <  (V_VISIBLE + V_FRONT + V_SYNC)));

    // Video active region
    wire display_active = (h_cnt < H_VISIBLE) && (v_cnt < V_VISIBLE);

    // Color gradient: red = X position, green = Y position, blue = X^Y pattern
    wire [3:0] red   = display_active ? (h_cnt[9:6]) : 4'b0000;
    wire [3:0] green = display_active ? (v_cnt[8:5]) : 4'b0000;
    wire [3:0] blue  = display_active ? (h_cnt[9:6] ^ v_cnt[8:5]) : 4'b0000;

    // Map to outputs
    assign uo_out  = {green, red};
    assign uio_out = {vsync, hsync, blue};

endmodule