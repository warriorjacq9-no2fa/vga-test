#ifndef DISPLAY_H
#define DISPLAY_H
#include <vector>
#include <GLFW/glfw3.h>

#define WIDTH 800
#define HEIGHT 525

int displayRun(std::vector<unsigned char>* pixels, bool* gl_done, bool* texUpdate, GLFWkeyfun kbd);
#endif