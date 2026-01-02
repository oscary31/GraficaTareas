#include "3DViewer.h"
#include <iostream>

C3DViewer::C3DViewer()
{
}

C3DViewer::~C3DViewer()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool C3DViewer::setup()
{
    if (!glfwInit()) 
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, "C3DViewer Window: Hello Triangle", NULL, NULL);
    if (!m_window) 
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Inicializar glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
    {
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
        auto ptr = reinterpret_cast<C3DViewer*>(glfwGetWindowUserPointer(window));
        if (ptr) 
            ptr->resize(w, h);
    });

    // Setup shader
    if (!setupShader()) return false;

    // Setup VAO y VBO para el triángulo
    setupTriangle();

    glViewport(0, 0, width, height);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallbackStatic);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallbackStatic);
    glfwSetCursorPosCallback(m_window, cursorPosCallbackStatic);

    return true;
}

void C3DViewer::update()
{

}

void C3DViewer::mainLoop() 
{
    while (!glfwWindowShouldClose(m_window)) 
    {
        glfwPollEvents();

        // color de borrado
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        // borrando búferes
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render();

        glfwSwapBuffers(m_window);
    }
}

void C3DViewer::onKey(int key, int scancode, int action, int mods) 
{

    if (action == GLFW_PRESS)
    {
        std::cout << "Key " << key << " pressed\n";
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
    else if (action == GLFW_RELEASE)
        std::cout << "Key " << key << " released\n";
}

void C3DViewer::onMouseButton(int button, int action, int mods) 
{
    if (button >= 0 && button < 3)
    {
        double xpos, ypos;
        glfwGetCursorPos(m_window, &xpos, &ypos);
        if (action == GLFW_PRESS)
        {
            mouseButtonsDown[button] = true;
            // Obtener posición actual del cursor
            std::cout << "Mouse button " << button << " pressed at position (" << xpos << ", " << ypos << ")\n";
        }
        else if (action == GLFW_RELEASE)
        {

            mouseButtonsDown[button] = false;
            std::cout << "Mouse button " << button << " released at position (" << xpos << ", " << ypos << ")\n";
        }
    }
}

void C3DViewer::onCursorPos(double xpos, double ypos) 
{
    if (mouseButtonsDown[0] || mouseButtonsDown[1] || mouseButtonsDown[2]) 
    {
        std::cout << "Mouse move at (" << xpos << ", " << ypos << ")\n";
    }
}

void C3DViewer::render() 
{
    update();

    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_vao);

    // dibujo 2 triángulos por ahora...
    glDrawArrays(GL_TRIANGLES, 0, 6);

    drawInterface();
}

void C3DViewer::drawInterface()
{
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Aquí colocas el código ImGui para el slider
    ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Once); // Tamaño inicial 400x300, solo al crear ventana
    ImGui::Begin("Control Panel");
    static int anyDummyValue = 50;
    if (ImGui::SliderInt("slider-demo", &anyDummyValue, 1, 100)) 
    {
        // valor actualizado automáticamente en anyDummyValue
    }
    ImGui::End();
    ImGui::Render();
    // Rendirizar ImGui con OpenGL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (deltaTime >= 1.0) 
    {
        lastTime = currentTime;
    }
}

void C3DViewer::resize(int new_width, int new_height) 
{
    width = new_width;
    height = new_height;

    // actualizamos el viewport cada vez que haya un resize
    glViewport(0, 0, width, height);
}

bool C3DViewer::setupShader() 
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

bool C3DViewer::checkCompileErrors(GLuint shader, const char* type) 
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

void C3DViewer::setupTriangle()
{
    float vertices[] = 
    {
        // x      y      z     r     g     b 
        -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 
         1.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 
         1.0f, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void C3DViewer::keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    C3DViewer* self = (C3DViewer*)glfwGetWindowUserPointer(window);
    if (self) 
        self->onKey(key, scancode, action, mods);
}

void C3DViewer::mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods) 
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    C3DViewer* self = (C3DViewer*)glfwGetWindowUserPointer(window);
    if (self)
        self->onMouseButton(button, action, mods);
}

void C3DViewer::cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos) 
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    C3DViewer* self = (C3DViewer*)glfwGetWindowUserPointer(window);
    if (self) 
        self->onCursorPos(xpos, ypos);
}

