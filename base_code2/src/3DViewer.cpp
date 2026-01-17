#define GLM_ENABLE_EXPERIMENTAL
#include "3DViewer.h"
#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "tinyfiledialogs.h"

C3DViewer::C3DViewer()
{
}

C3DViewer::~C3DViewer()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
    if (m_pickingShaderProgram) glDeleteProgram(m_pickingShaderProgram);
    if (m_pickingFBO) glDeleteFramebuffers(1, &m_pickingFBO);
    if (m_pickingTexture) glDeleteTextures(1, &m_pickingTexture);
    if (m_pickingDepthBuffer) glDeleteRenderbuffers(1, &m_pickingDepthBuffer);
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

    m_window = glfwCreateWindow(width, height, "OBJ Viewer - OpenGL", NULL, NULL);
    if (!m_window)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    glEnable(GL_DEPTH_TEST);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        auto ptr = reinterpret_cast<C3DViewer*>(glfwGetWindowUserPointer(window));
        if (ptr)
            ptr->resize(w, h);
        });

    if (!setupShader()) return false;
    if (!setupPickingShader()) return false;
    if (!setupBoundingBoxShader()) return false;  // NUEVO
    setupPickingFramebuffer();
    setupBoundingBox();  // NUEVO

    if (width <= 0 || height <= 0)
    {
        width = 1280;
        height = 720;
    }

    glViewport(0, 0, width, height);

    glfwSetKeyCallback(m_window, keyCallbackStatic);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallbackStatic);
    glfwSetCursorPosCallback(m_window, cursorPosCallbackStatic);

    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.1f, 100.0f);
    m_viewMatrix = glm::lookAt(m_cameraPos, m_cameraTarget, m_cameraUp);
    m_modelMatrix = glm::mat4(1.0f);

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

        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render();

        glfwSwapBuffers(m_window);
    }
}

void C3DViewer::onKey(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        else if (key == GLFW_KEY_O)
            loadOBJFile();
        else if (key == GLFW_KEY_DELETE && m_selectedSubMesh >= 0)  // NUEVO
        {
            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            if (m_selectedSubMesh < (int)subMeshes.size())
            {
                subMeshes.erase(subMeshes.begin() + m_selectedSubMesh);
                m_selectedSubMesh = -1;
                assignPickingColors();
                std::cout << "Sub-mesh eliminado" << std::endl;
            }
        }
    }
}

// Modificada: Verificación de ImGui para evitar conflictos
void C3DViewer::onMouseButton(int button, int action, int mods)
{
    // NUEVO: Verificar si ImGui capturó el evento
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    if (button >= 0 && button < 3)
    {
        double xpos, ypos;
        glfwGetCursorPos(m_window, &xpos, &ypos);

        if (action == GLFW_PRESS)
        {
            mouseButtonsDown[button] = true;
            m_lastMouseX = xpos;
            m_lastMouseY = ypos;

            if (button == GLFW_MOUSE_BUTTON_LEFT && m_objLoaded)
            {
                int pickedID = performPicking((int)xpos, (int)ypos);
                if (pickedID > 0 && pickedID <= (int)m_objLoader.getSubMeshes().size())
                {
                    m_selectedSubMesh = pickedID - 1;
                    std::cout << "Sub-mesh seleccionado: " << m_selectedSubMesh << std::endl;
                }
                else
                {
                    m_selectedSubMesh = -1;
                    std::cout << "Ningún sub-mesh seleccionado" << std::endl;
                }
            }
        }
        else if (action == GLFW_RELEASE)
        {
            mouseButtonsDown[button] = false;
            m_isDragging = false;
        }
    }
}

// Modificada: Verificación de ImGui para evitar conflictos
void C3DViewer::onCursorPos(double xpos, double ypos)
{
    // NUEVO: Verificar si ImGui capturó el evento
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        return;
    }

    if (!m_objLoaded) return;

    double deltaX = xpos - m_lastMouseX;
    double deltaY = ypos - m_lastMouseY;

    if (mouseButtonsDown[GLFW_MOUSE_BUTTON_LEFT])
    {
        m_isDragging = true;

        float sensitivity = 0.005f;
        float angleX = static_cast<float>(deltaY) * sensitivity;
        float angleY = static_cast<float>(deltaX) * sensitivity;

        glm::quat qx = glm::angleAxis(angleX, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat qy = glm::angleAxis(angleY, glm::vec3(0.0f, 1.0f, 0.0f));

        m_objectRotation = qy * qx * m_objectRotation;
        m_objectRotation = glm::normalize(m_objectRotation);
    }

    if (mouseButtonsDown[GLFW_MOUSE_BUTTON_RIGHT])
    {
        m_isDragging = true;

        float sensitivity = 0.002f;
        glm::vec3 translation(
            static_cast<float>(deltaX) * sensitivity,
            static_cast<float>(-deltaY) * sensitivity,
            0.0f
        );

        if (m_selectedSubMesh >= 0)
        {
            // Trasladar sub-mesh seleccionado
            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            subMeshes[m_selectedSubMesh].translation += translation;
        }
        else
        {
            // Trasladar objeto completo
            m_objectTranslation += translation;
        }
    }

    m_lastMouseX = xpos;
    m_lastMouseY = ypos;
}

void C3DViewer::loadOBJFile()
{
    const char* filterPatterns[1] = { "*.obj" };
    const char* filePath = tinyfd_openFileDialog(
        "Seleccionar archivo OBJ",
        "",
        1,
        filterPatterns,
        "Archivos OBJ (*.obj)",
        0
    );

    if (filePath)
    {
        std::cout << "Cargando: " << filePath << std::endl;
        if (m_objLoader.load(filePath))
        {
            m_objLoaded = true;
            assignPickingColors();  // NUEVO
            std::cout << "OBJ cargado exitosamente" << std::endl;
        }
        else
        {
            std::cerr << "Error al cargar el archivo OBJ" << std::endl;
        }
    }
}

// Modificada: Renderizado del bounding box
void C3DViewer::renderOBJ()
{
    if (!m_objLoaded) return;

    glUseProgram(m_shaderProgram);

    glm::mat4 normalizationMatrix = glm::mat4(1.0f);
    normalizationMatrix = glm::scale(normalizationMatrix, m_objLoader.getScaleFactor() * m_objectScale);
    normalizationMatrix = glm::translate(normalizationMatrix, -m_objLoader.getCenter());

    glm::mat4 rotationMatrix = glm::mat4_cast(m_objectRotation);
    glm::mat4 objectTransform = glm::translate(glm::mat4(1.0f), m_objectTranslation);
    glm::mat4 baseModel = objectTransform * rotationMatrix * normalizationMatrix;

    GLint viewLoc = glGetUniformLocation(m_shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_shaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

    glm::vec3 lightPos(2.0f, 2.0f, 2.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "viewPos"), 1, glm::value_ptr(m_cameraPos));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));

    GLint modelLoc = glGetUniformLocation(m_shaderProgram, "model");

    for (size_t i = 0; i < m_objLoader.getSubMeshes().size(); ++i)
    {
        const auto& subMesh = m_objLoader.getSubMeshes()[i];

        glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), subMesh.translation);
        m_modelMatrix = subMeshTransform * baseModel;

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m_modelMatrix));

        glm::vec3 color = subMesh.material.Kd;
        glUniform3fv(glGetUniformLocation(m_shaderProgram, "objectColor"), 1, glm::value_ptr(color));

        glBindVertexArray(subMesh.VAO);
        glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // NUEVO: Renderizar bounding box del submesh seleccionado
    if (m_selectedSubMesh >= 0 && m_selectedSubMesh < (int)m_objLoader.getSubMeshes().size())
    {
        const auto& selectedSubMesh = m_objLoader.getSubMeshes()[m_selectedSubMesh];
        renderBoundingBox(selectedSubMesh, baseModel);
    }
}

void C3DViewer::render()
{
    update();

    if (m_objLoaded)
    {
        renderOBJ();
    }

    drawInterface();
}

void C3DViewer::drawInterface()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_Once);
    ImGui::Begin("Control Panel");

    ImGui::Text("Presiona 'O' para cargar OBJ");

    if (ImGui::Button("Abrir archivo OBJ"))
    {
        loadOBJFile();
    }

    if (m_objLoaded)
    {
        ImGui::Separator();
        ImGui::Text("OBJ Cargado");
        ImGui::Text("Submeshes: %zu", m_objLoader.getSubMeshes().size());

        if (m_selectedSubMesh >= 0)
        {
            ImGui::Text("Sub-mesh seleccionado: %d", m_selectedSubMesh);
        }
        else
        {
            ImGui::Text("Ningun sub-mesh seleccionado");
        }

        ImGui::Separator();
        ImGui::Text("Controles:");
        ImGui::BulletText("Click izq: Seleccionar sub-mesh");
        ImGui::BulletText("Click medio + arrastrar: Trasladar");
        ImGui::BulletText("Click der + arrastrar: Rotar");
        ImGui::BulletText("Delete: Eliminar sub-mesh seleccionado");  // NUEVO

        ImGui::Separator();
        ImGui::Text("Transformaciones del Objeto:");

        if (ImGui::DragFloat3("Traslacion##obj", &m_objectTranslation.x, 0.01f))
        {
        }

        if (ImGui::DragFloat3("Escala##obj", &m_objectScale.x, 0.01f, 0.01f, 10.0f))
        {
        }

        glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(m_objectRotation));
        if (ImGui::DragFloat3("Rotacion (grados)##obj", &eulerAngles.x, 1.0f))
        {
            m_objectRotation = glm::quat(glm::radians(eulerAngles));
        }

        if (ImGui::Button("Resetear Transformaciones"))
        {
            m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
            m_objectScale = glm::vec3(1.0f);
            m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_selectedSubMesh = -1;

            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            for (auto& sm : subMeshes)
            {
                sm.translation = glm::vec3(0.0f);
            }
        }

        // NUEVO: Configuración de sub-mesh seleccionado
        if (m_selectedSubMesh >= 0 && m_selectedSubMesh < (int)m_objLoader.getSubMeshes().size())
        {
            ImGui::Separator();
            ImGui::Text("Sub-mesh Seleccionado: %d", m_selectedSubMesh);
            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            auto& selectedSM = subMeshes[m_selectedSubMesh];

            if (ImGui::DragFloat3("Traslacion Sub-mesh", &selectedSM.translation.x, 0.01f))
            {
            }

            if (ImGui::ColorEdit3("Color Material (Kd)", &selectedSM.material.Kd.x))
            {
            }

            if (ImGui::Button("Eliminar Sub-mesh"))
            {
                subMeshes.erase(subMeshes.begin() + m_selectedSubMesh);
                m_selectedSubMesh = -1;
                assignPickingColors();
                std::cout << "Sub-mesh eliminado desde interfaz" << std::endl;
            }

            ImGui::Separator();
            ImGui::Text("Bounding Box:");
            ImGui::ColorEdit3("Color Bounding Box", &m_boundingBoxColor.x);
        }
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void C3DViewer::resize(int new_width, int new_height)
{
    // Evitar divisi?n por cero
    if (new_width <= 0 || new_height <= 0)
    {
        std::cerr << "Advertencia: Dimensiones inv?lidas ("
            << new_width << "x" << new_height << "), ignorando resize" << std::endl;
        return;
    }
    // Establecer dimensiones m?nimas
    width = std::max(1, new_width);
    height = std::max(1, new_height);

    glViewport(0, 0, width, height);
    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.1f, 100.0f);

    // Recrear framebuffer de picking con nuevo tama?o
    if (m_pickingFBO)
    {
        glDeleteFramebuffers(1, &m_pickingFBO);
        glDeleteTextures(1, &m_pickingTexture);
        glDeleteRenderbuffers(1, &m_pickingDepthBuffer);
        setupPickingFramebuffer();
    }
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

void C3DViewer::setupPickingFramebuffer()
{
    // VALIDACI?N: No crear framebuffer con dimensiones inv?lidas
    if (width <= 0 || height <= 0)
    {
        std::cerr << "Error: No se puede crear framebuffer con dimensiones "
            << width << "x" << height << std::endl;
        return;
    }

    glGenFramebuffers(1, &m_pickingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pickingFBO);

    // Textura de color para picking
    glGenTextures(1, &m_pickingTexture);
    glBindTexture(GL_TEXTURE_2D, m_pickingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pickingTexture, 0);

    // Depth buffer
    glGenRenderbuffers(1, &m_pickingDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_pickingDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_pickingDepthBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Framebuffer de picking no est? completo" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void C3DViewer::renderForPicking()
{
    if (!m_objLoaded) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_pickingFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_pickingShaderProgram);

    // Configurar matrices base
    glm::mat4 normalizationMatrix = glm::mat4(1.0f);
    normalizationMatrix = glm::scale(normalizationMatrix, m_objLoader.getScaleFactor() * m_objectScale);
    normalizationMatrix = glm::translate(normalizationMatrix, -m_objLoader.getCenter());

    glm::mat4 rotationMatrix = glm::mat4_cast(m_objectRotation);
    glm::mat4 objectTransform = glm::translate(glm::mat4(1.0f), m_objectTranslation);
    glm::mat4 baseModel = objectTransform * rotationMatrix * normalizationMatrix;

    GLint viewLoc = glGetUniformLocation(m_pickingShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_pickingShaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

    GLint modelLoc = glGetUniformLocation(m_pickingShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(m_pickingShaderProgram, "pickingColor");

    // Renderizar cada submesh con su color ?nico
    for (const auto& subMesh : m_objLoader.getSubMeshes())
    {
        glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), subMesh.translation);
        m_modelMatrix = subMeshTransform * baseModel;

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
        glUniform3fv(colorLoc, 1, glm::value_ptr(subMesh.pickingColor));

        glBindVertexArray(subMesh.VAO);
        glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int C3DViewer::performPicking(int mouseX, int mouseY)
{
    renderForPicking();

    glBindFramebuffer(GL_FRAMEBUFFER, m_pickingFBO);

    // Leer pixel en posici?n del mouse (invertir Y)
    unsigned char pixel[3];
    glReadPixels(mouseX, height - mouseY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convertir color RGB a ID
    int pickedID = pixel[0] + pixel[1] * 256 + pixel[2] * 256 * 256;

    return pickedID;
}

bool C3DViewer::setupPickingShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &pickingVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);
    if (!checkCompileErrors(vertexShader, "PICKING_VERTEX")) return false;

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &pickingFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);
    if (!checkCompileErrors(fragmentShader, "PICKING_FRAGMENT")) return false;

    m_pickingShaderProgram = glCreateProgram();
    glAttachShader(m_pickingShaderProgram, vertexShader);
    glAttachShader(m_pickingShaderProgram, fragmentShader);
    glLinkProgram(m_pickingShaderProgram);
    if (!checkCompileErrors(m_pickingShaderProgram, "PICKING_PROGRAM")) return false;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void C3DViewer::assignPickingColors()
{
    auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());

    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        int id = i + 1;

        int r = (id & 0x000000FF) >> 0;
        int g = (id & 0x0000FF00) >> 8;
        int b = (id & 0x00FF0000) >> 16;

        subMeshes[i].pickingColor = glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);

        std::cout << "SubMesh " << i << " - ID: " << id
            << " Color: (" << r << ", " << g << ", " << b << ")" << std::endl;
    }
}

void C3DViewer::setupBoundingBox()
{
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    unsigned int indices[] = {
        0, 1,  1, 2,  2, 3,  3, 0,
        4, 5,  5, 6,  6, 7,  7, 4,
        0, 4,  1, 5,  2, 6,  3, 7
    };

    glGenVertexArrays(1, &m_boundingBoxVAO);
    glGenBuffers(1, &m_boundingBoxVBO);
    glGenBuffers(1, &m_boundingBoxEBO);

    glBindVertexArray(m_boundingBoxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_boundingBoxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_boundingBoxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

bool C3DViewer::setupBoundingBoxShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &boundingBoxVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);
    if (!checkCompileErrors(vertexShader, "BBOX_VERTEX")) return false;

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &boundingBoxFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);
    if (!checkCompileErrors(fragmentShader, "BBOX_FRAGMENT")) return false;

    m_boundingBoxShaderProgram = glCreateProgram();
    glAttachShader(m_boundingBoxShaderProgram, vertexShader);
    glAttachShader(m_boundingBoxShaderProgram, fragmentShader);
    glLinkProgram(m_boundingBoxShaderProgram);
    if (!checkCompileErrors(m_boundingBoxShaderProgram, "BBOX_PROGRAM")) return false;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void C3DViewer::calculateSubMeshBounds(const SubMesh& subMesh, glm::vec3& minBounds, glm::vec3& maxBounds)
{
    if (subMesh.vertices.empty()) {
        minBounds = glm::vec3(0.0f);
        maxBounds = glm::vec3(0.0f);
        return;
    }

    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& vertex : subMesh.vertices)
    {
        minBounds = glm::min(minBounds, vertex);
        maxBounds = glm::max(maxBounds, vertex);
    }
}

void C3DViewer::renderBoundingBox(const SubMesh& subMesh, const glm::mat4& baseModel)
{
    glm::vec3 minBounds, maxBounds;
    calculateSubMeshBounds(subMesh, minBounds, maxBounds);

    glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    glm::vec3 size = maxBounds - minBounds;

    glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), subMesh.translation);
    glm::mat4 boxModel = subMeshTransform * baseModel;

    boxModel = glm::translate(boxModel, center);
    boxModel = glm::scale(boxModel, size);

    glUseProgram(m_boundingBoxShaderProgram);

    GLint viewLoc = glGetUniformLocation(m_boundingBoxShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_boundingBoxShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(m_boundingBoxShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(m_boundingBoxShaderProgram, "boxColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(boxModel));
    glUniform3fv(colorLoc, 1, glm::value_ptr(m_boundingBoxColor));

    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(m_boundingBoxVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}