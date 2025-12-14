#include "PixelRender.h"
#include <cmath>
CPixelRender::CPixelRender() : m_buffer(width* height) 
{
    std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 0,0,0,0 });
}

CPixelRender::~CPixelRender()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool CPixelRender::setup()
{
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, "CPixelRender Window", NULL, NULL);
    if (!m_window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Inicializar glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Opcional

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        auto ptr = reinterpret_cast<CPixelRender*>(glfwGetWindowUserPointer(window));
        if (ptr) {
            ptr->resize(w, h);
        }
    });

    // Setup textura
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_buffer.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Setup shader
    if (!setupShader()) return false;

    // Setup VAO y VBO para el quad
    setupQuad();

    glViewport(0, 0, width, height);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallbackStatic);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallbackStatic);
    glfwSetCursorPosCallback(m_window, cursorPosCallbackStatic);

    return true;
}

void CPixelRender::setPixel(int x, int y, const RGBA& color)
{
    if (x < 0 || x >= width || y < 0 || y >= height) return;

    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    // Alpha blending: src over dst. alpha in [0,1]
    float a = color.a / 255.0f;
    if (a >= 0.999f) {
        // fully opaque, replace
        m_buffer[idx] = color;
        return;
    }
    if (a <= 0.001f) {
        // fully transparent, do nothing
        return;
    }
    RGBA dst = m_buffer[idx];
    float dstA = dst.a / 255.0f;

    // resulting alpha
    float outA = a + dstA * (1.0f - a);
    // avoid division by zero when premultiplying
    auto blendChannel = [&](unsigned char srcC, unsigned char dstC)->unsigned char {
        float srcF = srcC / 255.0f;
        float dstF = dstC / 255.0f;
        float outF = 0.0f;
        if (outA > 0.0f) {
            outF = (srcF * a + dstF * dstA * (1.0f - a)) / outA;
        }
        else {
            outF = 0.0f;
        }
        return static_cast<unsigned char>(std::round(std::min(1.0f, std::max(0.0f, outF)) * 255.0f));
        };

    RGBA out;
    out.a = static_cast<unsigned char>(std::round(std::min(1.0f, std::max(0.0f, outA)) * 255.0f));
    out.r = blendChannel(color.r, dst.r);
    out.g = blendChannel(color.g, dst.g);
    out.b = blendChannel(color.b, dst.b);

    m_buffer[idx] = out;
}

void CPixelRender::update()
{

}


void CPixelRender::mainLoop() 
{
    while (!glfwWindowShouldClose(m_window)) 
    {
        glfwPollEvents();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render();


        glfwSwapBuffers(m_window);
    }
}


void CPixelRender::onKey(int key, int scancode, int action, int mods) 
{
}

void CPixelRender::onMouseButton(int button, int action, int mods) 
{
}

void CPixelRender::onCursorPos(double xpos, double ypos) 
{
}

void CPixelRender::render() 
{

    update();

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, m_buffer.data());
    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    drawInterface();

}

void CPixelRender::drawInterface()
{
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Aquí colocas el código ImGui para el slider
    ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once); // Tamaño inicial 400x300, solo al crear ventana
    ImGui::Begin("Control Panel");
    if (ImGui::SliderInt("n-pixels", &m_nPixels, 1, 1000000)) {
        // valor actualizado automáticamente en m_nPixels
    }
    ImGui::End();
    ImGui::Render();
    // Rendirizar ImGui con OpenGL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (deltaTime >= 1.0) {
        char title[256];
        snprintf(title, sizeof(title), "CPixelRender - Pixels modified/s: %.2lf M", pixelsModifiedThisSecond / 1000000.0);
        glfwSetWindowTitle(m_window, title);
        pixelsModifiedThisSecond = 0;
        lastTime = currentTime;
    }
}

void CPixelRender::resize(int new_width, int new_height) 
{
    width = new_width;
    height = new_height;
    m_buffer.resize(width * height);
    std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 0,0,0,0 });
    glViewport(0, 0, width, height);
    // Redimensionar la textura en GPU
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}



bool CPixelRender::setupShader() 
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vertexShader);
    if (!checkCompileErrors(vertexShader, "VERTEX")) return false;

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);
    if (!checkCompileErrors(fragmentShader, "FRAGMENT")) return false;

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);
    if (!checkCompileErrors(m_shaderProgram, "PROGRAM")) return false;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

bool CPixelRender::checkCompileErrors(GLuint shader, const char* type) 
{
    GLint success;
    GLchar infoLog[1024];
    if (strcmp(type, "PROGRAM") != 0) 
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) 
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            fprintf(stderr, "ERROR::SHADER_COMPILATION_ERROR of type: %s\n%s\n", type, infoLog);
            return false;
        }
    }
    else 
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            fprintf(stderr, "ERROR::PROGRAM_LINKING_ERROR of type: %s\n%s\n", type, infoLog);
            return false;
        }
    }
    return true;
}

void CPixelRender::setupQuad() 
{
    float vertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f, // top-left
            1.0f,  1.0f,  1.0f, 1.0f, // top-right
            1.0f, -1.0f,  1.0f, 0.0f, // bottom-right

        -1.0f,  1.0f,  0.0f, 1.0f, // top-left
            1.0f, -1.0f,  1.0f, 0.0f, // bottom-right
        -1.0f, -1.0f,  0.0f, 0.0f  // bottom-left
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void CPixelRender::keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    CPixelRender* self = (CPixelRender*)glfwGetWindowUserPointer(window);
    if (self) 
        self->onKey(key, scancode, action, mods);
}

void CPixelRender::mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods) 
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    CPixelRender* self = (CPixelRender*)glfwGetWindowUserPointer(window);
    if (self)
    {
        self->onMouseButton(button, action, mods);
    }
}

void CPixelRender::cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos) 
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    CPixelRender* self = (CPixelRender*)glfwGetWindowUserPointer(window);
    if (self) self->onCursorPos(xpos, ypos);
}

