#include <verilated.h>
#include <iostream>
#include <thread>
#include "display.h"
#define S2(x) #x
#define S(x) S2(x)
#define HEADER(x) S(x.h)
#include HEADER(VTOP_MODULE)

using namespace std;

vector<unsigned char> pixels(WIDTH * HEIGHT * 3);

VTOP_MODULE* dut;

uint64_t main_time = 0;
double sc_timestamp() {
    return main_time;
}

void tick() {
    main_time++;

    dut->clk = 1;
    dut->eval();

    dut->clk = 0;
    dut->eval();
}

void reset() {
    dut->rst_n = 0;
    dut->clk = 0;
    dut->eval();
    tick();
    dut->rst_n = 1;
}

void vtb() {
    int x = 0;
    int y = 0;
    dut = new VTOP_MODULE;
    
    reset();
    
    while (!Verilated::gotFinish()) {
        tick();
        tick();

        x++;
        if(!(dut->uio_out & 0b00010000)) { // HSYNC
            x = 0;
            y++;
            //cout << "Newline" << endl;
        }
        if(!(dut->uio_out & 0b00100000)){ // VSYNC
            y = 0;
        }
        
        char r = (dut->uo_out & 0b00001111) * 0x11;
        char g = (dut->uo_out & 0b11110000) * 0x11;
        char b = (dut->uio_out & 0b00001111) * 0x11;
        bool de = dut->uio_out & 0b01000000; // bit 6 we added above
        if (x < WIDTH && y < HEIGHT) {
            int index = (y * WIDTH + x) * 3;
            pixels[index + 0] = r;
            pixels[index + 1] = g;
            pixels[index + 2] = b;
        }

    }
    dut->final();
    delete dut;
}

int main(int argc, char** args) {
    int gl_done;
	thread thread(displayRun, &pixels, &gl_done);

    while(!gl_done);

    Verilated::commandArgs(argc, args);

    vtb();

    return 0;
}