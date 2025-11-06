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
    Verilated::flushCall();
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
        if (!last_hsync && hsync) {
            x = 0;
            y++;
        }
        if (!last_vsync && vsync) {
            y = 0;
        }

        last_hsync = hsync;
        last_vsync = vsync;

        if (de && x < WIDTH && y < HEIGHT) {
            char r = (dut->uo_out & 0b00001111) * 0x11;
            char g = ((dut->uo_out & 0b11110000) >> 4) * 0x11;
            char b = (dut->uio_out & 0b00001111) * 0x11;

            int index = (y * WIDTH + x) * 3;
            pixels[index + 0] = r;
            pixels[index + 1] = g;
            pixels[index + 2] = b;
        }
        x++;
        if (x >= WIDTH) x = WIDTH - 1;

    }
    dut->final();
    delete dut;
}

int main(int argc, char** args) {
    int gl_done;
	thread thread(displayRun, &pixels, &gl_done);

    while(!gl_done)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Verilated::commandArgs(argc, args);

    vtb();

    return 0;
}