#ifndef DISPLAY_H
#define DISPLAY_H
#include <vector>

#define WIDTH 640
#define HEIGHT 480

int displayRun(std::vector<unsigned char>* pixels, int* gl_done);
#endif