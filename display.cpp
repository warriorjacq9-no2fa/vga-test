#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "display.h"
#include <cstring>
#include <mutex>
#include <condition_variable>

extern std::mutex mtx;
extern std::condition_variable cv;

// Vertex shader for textured quad
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Fragment shader
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D screenTexture;
void main()
{
    FragColor = texture(screenTexture, TexCoord);
}
)";

// Error callback
void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int displayRun(std::vector<unsigned char>* pixels, bool* gl_done, bool* texUpdate)
{
    glfwSetErrorCallback(glfwErrorCallback);

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "VGA Testbench", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Viewport
    glViewport(0, 0, WIDTH, HEIGHT);

    // Create shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    // Link shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Quad vertex data
    float vertices[] = {
        // positions   // texcoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels->data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glBindTexture(GL_TEXTURE_2D, texture);

    GLuint pboIds[2];
    glGenBuffers(2, pboIds);

    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, WIDTH * HEIGHT * 4, nullptr, GL_DYNAMIC_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    int index = 0, nextIndex = 0;

    // Frame info inits
    double previousTime = glfwGetTime();
    int frameCount = 0;
    int fcModel = 0;
    char* s = new char[45];

    {
        std::lock_guard<std::mutex> lock(mtx);
        *gl_done = true;
    }
    cv.notify_one();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - previousTime;
        frameCount++;

        if (deltaTime >= 1.0) {
            int fps = frameCount / deltaTime;
            snprintf(s, 45, "VGA Testbench | %d FPS (%d window)",
                (int)(fcModel / deltaTime), fps
            );
            glfwSetWindowTitle(window, s);

            frameCount = 0;
            fcModel = 0;
            previousTime = currentTime;
        }

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT);
        if(*texUpdate) {
            fcModel++;
            *texUpdate = false;
            // Bind the PBO you’ll update from CPU side
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[index]);

            // Map buffer to write new pixels
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, WIDTH * HEIGHT * 4, pixels->data());

            // Bind the other PBO for texture upload
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[index]);
            void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, WIDTH * HEIGHT * 4,
                                        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            if (ptr) {
                memcpy(ptr, pixels->data(), WIDTH * HEIGHT * 4);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[nextIndex]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, 0);

            // Unbind PBO
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            index = (index + 1) % 2;
            nextIndex = (index + 1) % 2;
        }
        // Draw
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);

    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &texture);

    glfwDestroyWindow(window);
    glfwTerminate();

    delete[] s;

    exit(0);
}