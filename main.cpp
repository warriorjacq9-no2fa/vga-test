#include <verilated.h>
#include <iostream>
#include <thread>
#include "display.h"
#define S2(x) #x
#define S(x) S2(x)
#define HEADER(x) S(x.h)

#ifndef VTOP_MODULE
#define VTOP_MODULE Vtest
#endif

#include HEADER(VTOP_MODULE)

using namespace std;

vector<unsigned char> pixels(WIDTH * HEIGHT * 4);
bool texUpdate;

VTOP_MODULE* dut;

inline void tick() {
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
    bool last_hsync = true;
    bool last_vsync = true;
    dut = new VTOP_MODULE;
    
    reset();
    
    while (!Verilated::gotFinish()) {
        tick();

        bool hsync = dut->uio_out & 0b00010000;
        bool vsync = dut->uio_out & 0b00100000;
        bool de    = dut->uio_out & 0b01000000;

        // Detect rising edges of syncs
        if (last_hsync && !hsync) {
            x = 0;
            y++;
        }
        if (last_vsync && !vsync) {
            y = 0;
            texUpdate = true;
        }

        last_hsync = hsync;
        last_vsync = vsync;

        if (x < WIDTH && y < HEIGHT) {
            char r = 0, g = 0, b = 0;
            if(de) {
                r = (dut->uo_out & 0b00001111) * 0x11;
                g = ((dut->uo_out & 0b11110000) >> 4) * 0x11;
                b = (dut->uio_out & 0b00001111) * 0x11;
            }

            unsigned char* p = &pixels[(y * WIDTH + x) * 4];
            p[0] = r; p[1] = g; p[2] = b; p[3] = 0xFF;
        }
        x++;

    }
    dut->final();
    delete dut;
}

int main(int argc, char** args) {
    int gl_done;
    texUpdate = false;
	thread thread(displayRun, &pixels, &gl_done, &texUpdate);

    while(!gl_done)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Verilated::commandArgs(argc, args);

    vtb();

    return 0;
}