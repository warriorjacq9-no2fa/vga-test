#ifndef DISPLAY_H
#define DISPLAY_H
#include <vector>

#define WIDTH 800
#define HEIGHT 525

int displayRun(std::vector<unsigned char>* pixels, bool* gl_done, bool* texUpdate);
#endif