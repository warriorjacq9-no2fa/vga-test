/* verilator lint_off UNUSEDPARAM */
// INPUT 25.175MHz
module vga
#( parameter // Default is 640x480
    ACTIVE_WIDTH	= 	640,
    H_FP	        = 	16,
	H_BP		    = 	48,
    TOTAL_WIDTH		=	800,
    
    ACTIVE_HEIGHT	= 	480,
    V_FP        	= 	10,
    V_BP    		= 	33,
    TOTAL_HEIGHT	=	525
)
(
    input  wire clk,
    
    output reg hsync, vsync,
    output reg [10:0] x, y,
    output reg de,

	input wire rst_n
);


    // next state regs
    reg hsync_next, vsync_next;
    reg [10:0] x_next, y_next;
    reg active_area_next;
    
    // sequential logic
    always @(posedge clk or negedge rst_n)
    begin
        if(~rst_n)
        begin
            hsync <= 0;
            vsync <= 0;
            x <= 0;
            y <= 0;
            de <= 0;
        end else begin
            hsync <= hsync_next;
            vsync <= vsync_next;
            x <= x_next;
            y <= y_next;
            de <= active_area_next;
        end
    end
    
    // combinational logic
    always @(*)
    begin
        hsync_next = ~(ACTIVE_WIDTH + H_FP <= x && x < TOTAL_WIDTH - H_BP);
        vsync_next = ~(ACTIVE_HEIGHT + V_FP <= y && y < TOTAL_HEIGHT - V_BP);
        
        if(x == TOTAL_WIDTH - 1)
        begin
            x_next = 0;
            if(y == TOTAL_HEIGHT - 1)
                y_next = 0;
            else
                y_next = y + 1;
        end else begin
            x_next = x + 1;
            y_next = y;
        end
            
            
        active_area_next = x < ACTIVE_WIDTH && y < ACTIVE_HEIGHT;
    end


endmodule
