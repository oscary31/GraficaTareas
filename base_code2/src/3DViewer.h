#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"


class C3DViewer 
{

public:
    C3DViewer();

    bool setup();

    void mainLoop();

    virtual ~C3DViewer();

private:
    virtual void onKey(int key, int scancode, int action, int mods);

    virtual void onMouseButton(int button, int action, int mods);

    virtual void onCursorPos(double xpos, double ypos);

    virtual void update();

    virtual void render();

    virtual void drawInterface();

    void resize(int new_width, int new_height);

    bool setupShader();

    bool checkCompileErrors(GLuint shader, const char* type);

    void setupTriangle();

    static void keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods);

    static void mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods);

    static void cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos);


protected:
    int width = 720;
    int height = 480;
    GLFWwindow* m_window = nullptr;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_shaderProgram = 0;
    double lastTime = 0.0;
    bool mouseButtonsDown[3] = { false, false, false };
    const char* vertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        void main() 
        {
            gl_Position = vec4(aPos, 1.0);
            vColor = aColor;
        }
    )glsl";

    const char* fragmentShaderSrc = R"glsl(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )glsl";
};
