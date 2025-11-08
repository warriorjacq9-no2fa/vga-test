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
#include <condition_variable>
std::condition_variable cv;
std::mutex mtx;

bool gl_done = false;
bool texUpdate = false;

std::vector<unsigned char> pixels(WIDTH * HEIGHT * 4);

VTOP_MODULE* dut;

static inline void wait_gl_done() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ return gl_done; });
}

static inline void tick() {
    dut->clk = 1;
    dut->eval();

    dut->clk = 0;
    dut->eval();
}

static inline void reset() {
    dut->rst_n = 0;
    dut->clk = 0;
    dut->eval();
    tick();
    dut->rst_n = 1;
}

inline uint8_t _4to8(uint8_t v) { return (v << 4) | v; }

void vtb() {
    int x = 0;
    int y = 0;
    bool last_hsync = true;
    bool last_vsync = true;
    
    constexpr uint8_t HSYNC_MASK = 0b00010000;
    constexpr uint8_t VSYNC_MASK = 0b00100000;
    constexpr uint8_t DE_MASK    = 0b01000000;

    dut = new VTOP_MODULE;
    
    reset();
    
    while (!Verilated::gotFinish()) {
        tick();

        bool hsync = dut->uio_out & HSYNC_MASK;
        bool vsync = dut->uio_out & VSYNC_MASK;
        bool de    = dut->uio_out & DE_MASK;

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
                r = _4to8(dut->uo_out & 0b00001111);
                g = _4to8((dut->uo_out & 0b11110000) >> 4);
                b = _4to8(dut->uio_out & 0b00001111);
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
    texUpdate = false;
	std::thread thread(displayRun, &pixels, &gl_done, &texUpdate);

    wait_gl_done();

    Verilated::commandArgs(argc, args);

    vtb();

    return 0;
}