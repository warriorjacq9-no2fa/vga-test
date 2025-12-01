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

char keys;
#define KSET(n) (keys |= 1 << n)
#define KGET(n) (keys & 1 << n)
#define KCLR(n) (keys &= ~(1 << n))

#define KP1UP 0
#define KP1DN 1
#define KP1SRV 2
#define KP2UP 3
#define KP2DN 4
#define KP2SRV 5

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
    dut->rst_n = 1;
    dut->eval();
    tick();
    dut->rst_n = 0;
    dut->clk = 0;
    dut->eval();
    tick();
    dut->rst_n = 1;
}

inline uint8_t _2to8(uint8_t v) { return (((v >> 3) & 0x02) | (v & 0x01)) * 0x55; }

void vtb() {
    int x = 0;
    int y = 0;
    bool last_hsync = true;
    bool last_vsync = true;
    
    constexpr uint8_t HSYNC_MASK = 0b10000000;
    constexpr uint8_t VSYNC_MASK = 0b00001000;
    constexpr uint8_t DE_MASK    = 0b00000001;

    dut = new VTOP_MODULE;
    
    constexpr double frame_time = 1.0 / 30.0; // 30 FPS

    auto frame_start = std::chrono::high_resolution_clock::now();
    
    reset();

    while (!Verilated::gotFinish()) {
        tick();

        bool hsync = dut->uo_out & HSYNC_MASK;
        bool vsync = dut->uo_out & VSYNC_MASK;
        bool de    = dut->uio_out & DE_MASK;

        // Detect rising edges of syncs
        if (!last_hsync && hsync) {
            x = 0;
            y++;
        }
        if (!last_vsync && vsync) {
            y = 0;
            texUpdate = true;
            auto frame_end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(frame_end - frame_start).count();
            if(elapsed < frame_time) {
                std::this_thread::sleep_for(std::chrono::duration<double>(frame_time - elapsed));
            }
            frame_start = std::chrono::high_resolution_clock::now();
        }

        last_hsync = hsync;
        last_vsync = vsync;

        if (x < WIDTH && y < HEIGHT) {
            char r = 0, g = 0, b = 0;
            if(de) {
                r = _2to8(dut->uo_out & 0b00010001);
                g = _2to8((dut->uo_out & 0b00100010) >> 1);
                b = _2to8((dut->uo_out & 0b01000100) >> 2);
            }

            unsigned char* p = &pixels[(y * WIDTH + x) * 4];
            p[0] = r; p[1] = g; p[2] = b; p[3] = 0xFF;
        }
        x++;

        dut->ui_in = keys;

    }
    dut->final();
    delete dut;
}

void kbd(GLFWwindow *win, int key, int sc, int action, int mods) {
    if(action == GLFW_PRESS) {
        switch(key) {
            case GLFW_KEY_W:
                KSET(KP1UP);
                break;
            case GLFW_KEY_UP:
                KSET(KP2UP);
                break;
            case GLFW_KEY_S:
                KSET(KP1DN);
                break;
            case GLFW_KEY_DOWN:
                KSET(KP2DN);
                break;
            case GLFW_KEY_Q:
                KSET(KP1SRV);
                break;
            case GLFW_KEY_ENTER:
                KSET(KP2SRV);
                break;
        }
    } else if(action == GLFW_RELEASE) {
        switch(key) {
            case GLFW_KEY_W:
                KCLR(KP1UP);
                break;
            case GLFW_KEY_UP:
                KCLR(KP2UP);
                break;
            case GLFW_KEY_S:
                KCLR(KP1DN);
                break;
            case GLFW_KEY_DOWN:
                KCLR(KP2DN);
                break;
            case GLFW_KEY_Q:
                KCLR(KP1SRV);
                break;
            case GLFW_KEY_ENTER:
                KCLR(KP2SRV);
                break;
        }
    }
}

int main(int argc, char** args) {
    texUpdate = false;
	std::thread thread(displayRun, &pixels, &gl_done, &texUpdate, kbd);
    wait_gl_done();

    Verilated::commandArgs(argc, args);

    vtb();

    return 0;
}
