#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdlib> // para rand()
#include <cstring> // para std::fill
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"


struct RGBA {
    unsigned char r, g, b, a;
};

class CPixelRender {

public:
    CPixelRender();

    virtual ~CPixelRender();

    bool setup();

    void setPixel(int x, int y, const RGBA& color);

    virtual void update();

    virtual void render();

    virtual void drawInterface();

    void mainLoop();

protected:
    virtual void onKey(int key, int scancode, int action, int mods);

    virtual void onMouseButton(int button, int action, int mods);

    virtual void onCursorPos(double xpos, double ypos);

private:

    void resize(int new_width, int new_height);

    bool setupShader();

    bool checkCompileErrors(GLuint shader, const char* type);

    void setupQuad();

    static void keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods);

    static void mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods);

    static void cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos);



protected:
    int width = 512;
    int height = 512;
    int m_nPixels = 100000;
    GLFWwindow* m_window = nullptr;
    std::vector<RGBA> m_buffer;
    GLuint m_texture = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_shaderProgram = 0;
    int pixelsModifiedThisSecond = 0;
    double lastTime = 0.0;
    bool mouseButtonsDown[3] = { false, false, false };
    const char* vertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )glsl";

    const char* fragmentShaderSrc = R"glsl(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        void main() {
            FragColor = texture(uTexture, TexCoord);
        }
    )glsl";
};
