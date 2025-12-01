#include <verilated.h>
#include <iostream>
#include <thread>
#include <deque>
#include <chrono>
#include <condition_variable>
#include <GLFW/glfw3.h>
#include <portaudio.h>
#include "pa_ringbuffer.h"

#include "display.h"      // your display system
#define S2(x) #x
#define S(x) S2(x)
#define HEADER(x) S(x.h)

#ifndef VTOP_MODULE
#define VTOP_MODULE Vtest
#endif

#define SAMPLE_RATE 44100
#define TARGET_FPS 60.0

#include HEADER(VTOP_MODULE)

// --------------------------------------------------------------------
// GLOBALS
// --------------------------------------------------------------------

char keys = 0;
bool gl_done = false;
bool texUpdate = false;

// Video framebuffer
std::vector<unsigned char> pixels(WIDTH * HEIGHT * 4);

VTOP_MODULE* dut;

// -------------------------------------------------
// Audio globals (lock-free ring buffer)
// -------------------------------------------------
PaUtilRingBuffer ringBuffer;
void* ringBufferData = nullptr;

float audio_lp = 0.0f;              // EMA low-pass accumulator
int pwm_counter = 0;                // decimation counter

constexpr double DUT_CLOCK_HZ = 25175000.0;     // from user
constexpr double SAMPLE_RATE_D = SAMPLE_RATE;   // 44100 Hz host audio
constexpr int DECIMATION = (int)(DUT_CLOCK_HZ / SAMPLE_RATE_D);  // ~571 ticks/sample
constexpr float EMA_ALPHA = 0.00005f;

// Timing / FPS
double currentFPS = TARGET_FPS;
auto lastFrameTime = std::chrono::high_resolution_clock::now();

// --------------------------------------------------------------------
// UTILITY MACROS
// --------------------------------------------------------------------

#define KSET(n) (keys |= 1 << n)
#define KGET(n) (keys & 1 << n)
#define KCLR(n) (keys &= ~(1 << n))

#define KP1UP 0
#define KP1DN 1
#define KP1SRV 2
#define KP2UP 3
#define KP2DN 4
#define KP2SRV 5

inline uint8_t _2to8(uint8_t v) {
    return (((v >> 3) & 0x02) | (v & 0x01)) * 0x55;
}

// ---------------------------------------------------------
// PortAudio callback — lock-free
// ---------------------------------------------------------
static int paCallback(
    const void*,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void*)
{
    float* out = (float*)outputBuffer;

    unsigned long available = PaUtil_GetRingBufferReadAvailable(&ringBuffer);
    unsigned long toRead = (available >= framesPerBuffer ? framesPerBuffer : available);

    PaUtil_ReadRingBuffer(&ringBuffer, out, toRead);

    // Zero-fill on underrun
    for (unsigned long i = toRead; i < framesPerBuffer; i++)
        out[i] = 0.0f;

    return paContinue;
}

// ---------------------------------------------------------
// Init PortAudio + ring buffer
// ---------------------------------------------------------
static void init_audio() {
    PaError err;

    ringBufferData = malloc(sizeof(float) * 65536);
    PaUtil_InitializeRingBuffer(&ringBuffer, sizeof(float), 65536, ringBufferData);

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    PaStream* stream;

    err = Pa_OpenDefaultStream(
        &stream,
        0,
        1,
        paFloat32,
        SAMPLE_RATE,
        256,
        paCallback,
        nullptr
    );
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    return;

error:
    fprintf(stderr, "PortAudio error %d: %s\n", err, Pa_GetErrorText(err));
}

// ---------------------------------------------------------
// Basic Verilator ticking
// ---------------------------------------------------------
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
    dut->clk   = 0;
    dut->eval();
    tick();

    dut->rst_n = 1;
}

// ---------------------------------------------------------
void updateFPS() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastFrameTime).count();
    lastFrameTime = now;

    if (elapsed > 0.0) {
        double instantFPS = 1.0 / elapsed;
        currentFPS = 0.9 * currentFPS + 0.1 * instantFPS;
    }
}

// --------------------------------------------------------------------
// MAIN SIMULATION LOOP (updated audio system)
// --------------------------------------------------------------------
void vtb() {
    int x = 0;
    int y = 0;
    bool last_hsync = true;
    bool last_vsync = true;

    constexpr uint8_t HSYNC_MASK = 0b10000000;
    constexpr uint8_t VSYNC_MASK = 0b00001000;
    constexpr uint8_t DE_MASK    = 0b00000001;

    dut = new VTOP_MODULE;
    reset();

    constexpr double frame_time = 1.0 / TARGET_FPS;
    auto frame_start = std::chrono::high_resolution_clock::now();

    while (!Verilated::gotFinish()) {
        tick();

        // -----------------------------
        // PWM AUDIO (fast version)
        // -----------------------------
        bool pwm_bit = (dut->uio_out & 0x80) != 0;

        float target = pwm_bit ? 1.0f : -1.0f;
        audio_lp += (target - audio_lp) * EMA_ALPHA;

        pwm_counter++;
        if (pwm_counter >= DECIMATION) {
            pwm_counter = 0;
            float sample = audio_lp * 0.1f;
            PaUtil_WriteRingBuffer(&ringBuffer, &sample, 1);
        }

        // -----------------------------
        // Video timing
        // -----------------------------
        bool hsync = dut->uo_out & HSYNC_MASK;
        bool vsync = dut->uo_out & VSYNC_MASK;
        bool de    = dut->uio_out & DE_MASK;

        if (!last_hsync && hsync) {
            x = 0;
            y++;
        }
        if (!last_vsync && vsync) {
            y = 0;
            texUpdate = true;

            updateFPS();

            auto frame_end = std::chrono::high_resolution_clock::now();
            auto elapsed   = std::chrono::duration<double>(frame_end - frame_start).count();
            if (elapsed < frame_time)
                std::this_thread::sleep_for(std::chrono::duration<double>(frame_time - elapsed));

            frame_start = std::chrono::high_resolution_clock::now();
        }

        last_hsync = hsync;
        last_vsync = vsync;

        // -----------------------------
        // Pixel output
        // -----------------------------
        if (x < WIDTH && y < HEIGHT) {
            unsigned char r = 0, g = 0, b = 0;

            if (de) {
                r = _2to8(dut->uo_out & 0b00010001);
                g = _2to8((dut->uo_out & 0b00100010) >> 1);
                b = _2to8((dut->uo_out & 0b01000100) >> 2);
            }

            unsigned char* p = &pixels[(y * WIDTH + x) * 4];
            p[0] = r; p[1] = g; p[2] = b; p[3] = 0xFF;
        }

        x++;

        // Keyboard inputs
        dut->ui_in = keys;
    }

    dut->final();
    delete dut;
}

// ---------------------------------------------------------
void kbd(GLFWwindow*, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_W) KSET(KP1UP);
        if (key == GLFW_KEY_S) KSET(KP1DN);
        if (key == GLFW_KEY_Q) KSET(KP1SRV);

        if (key == GLFW_KEY_UP)    KSET(KP2UP);
        if (key == GLFW_KEY_DOWN)  KSET(KP2DN);
        if (key == GLFW_KEY_ENTER) KSET(KP2SRV);
    } else if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_W) KCLR(KP1UP);
        if (key == GLFW_KEY_S) KCLR(KP1DN);
        if (key == GLFW_KEY_Q) KCLR(KP1SRV);

        if (key == GLFW_KEY_UP)    KCLR(KP2UP);
        if (key == GLFW_KEY_DOWN)  KCLR(KP2DN);
        if (key == GLFW_KEY_ENTER) KCLR(KP2SRV);
    }
}

// ---------------------------------------------------------
static std::condition_variable cv;
static std::mutex mtx;

static inline void wait_gl_done() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ return gl_done; });
}

// ---------------------------------------------------------
int main(int argc, char** args) {
    init_audio();

    texUpdate = false;
    std::thread thread(displayRun, &pixels, &gl_done, &texUpdate, kbd);
    wait_gl_done();

    Verilated::commandArgs(argc, args);
    vtb();

    return 0;
}
