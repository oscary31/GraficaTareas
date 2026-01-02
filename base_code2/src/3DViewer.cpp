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

    // Habilitar depth testing
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
    setupPickingFramebuffer();

    glViewport(0, 0, width, height);

    glfwSetKeyCallback(m_window, keyCallbackStatic);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallbackStatic);
    glfwSetCursorPosCallback(m_window, cursorPosCallbackStatic);

    // Inicializar matrices
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
    }
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
            m_lastMouseX = xpos;
            m_lastMouseY = ypos;

            // Picking con botón izquierdo
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

void C3DViewer::onCursorPos(double xpos, double ypos)
{
    if (!m_objLoaded) return;

    double deltaX = xpos - m_lastMouseX;
    double deltaY = ypos - m_lastMouseY;

    // Botón derecho: Rotar objeto
    if (mouseButtonsDown[GLFW_MOUSE_BUTTON_RIGHT])
    {
        m_isDragging = true;

        float sensitivity = 0.005f;
        float angleX = static_cast<float>(deltaY) * sensitivity;
        float angleY = static_cast<float>(deltaX) * sensitivity;

        // Crear quaterniones para rotación
        glm::quat qx = glm::angleAxis(angleX, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat qy = glm::angleAxis(angleY, glm::vec3(0.0f, 1.0f, 0.0f));

        // Acumular rotación
        m_objectRotation = qy * qx * m_objectRotation;
        m_objectRotation = glm::normalize(m_objectRotation);
    }

    // Botón medio: Trasladar objeto o sub-mesh
    if (mouseButtonsDown[GLFW_MOUSE_BUTTON_MIDDLE])
    {
        m_isDragging = true;

        float sensitivity = 0.005f;
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
            std::cout << "OBJ cargado exitosamente" << std::endl;
            std::cout << "Centro: (" << m_objLoader.getCenter().x << ", "
                << m_objLoader.getCenter().y << ", "
                << m_objLoader.getCenter().z << ")" << std::endl;
            std::cout << "Factor de escala: " << m_objLoader.getScaleFactor().x << std::endl;
        }
        else
        {
            std::cerr << "Error al cargar el archivo OBJ" << std::endl;
        }
    }
}

void C3DViewer::renderOBJ()
{
    if (!m_objLoaded) return;

    glUseProgram(m_shaderProgram);

    // Configurar matrices base
    glm::mat4 normalizationMatrix = glm::mat4(1.0f);
    normalizationMatrix = glm::scale(normalizationMatrix, m_objLoader.getScaleFactor() * m_objectScale);
    normalizationMatrix = glm::translate(normalizationMatrix, -m_objLoader.getCenter());

    glm::mat4 rotationMatrix = glm::mat4_cast(m_objectRotation);
    glm::mat4 objectTransform = glm::translate(glm::mat4(1.0f), m_objectTranslation);
    glm::mat4 baseModel = objectTransform * rotationMatrix * normalizationMatrix;

    // Enviar matrices de vista y proyección
    GLint viewLoc = glGetUniformLocation(m_shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_shaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

    // Lighting
    glm::vec3 lightPos(2.0f, 2.0f, 2.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "viewPos"), 1, glm::value_ptr(m_cameraPos));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));

    GLint modelLoc = glGetUniformLocation(m_shaderProgram, "model");

    // Renderizar cada submesh
    for (size_t i = 0; i < m_objLoader.getSubMeshes().size(); ++i)
    {
        const auto& subMesh = m_objLoader.getSubMeshes()[i];

        // Aplicar transformación individual del submesh
        glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), subMesh.translation);
        m_modelMatrix = subMeshTransform * baseModel;

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m_modelMatrix));

        // Color: resaltar si está seleccionado
        glm::vec3 color = subMesh.material.Kd;
        if ((int)i == m_selectedSubMesh)
        {
            color = glm::vec3(1.0f, 0.8f, 0.2f); // Color amarillo para seleccionado
        }
        glUniform3fv(glGetUniformLocation(m_shaderProgram, "objectColor"), 1, glm::value_ptr(color));

        glBindVertexArray(subMesh.VAO);
        glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
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

    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Once);
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

        ImGui::Separator();
        ImGui::Text("Transformaciones del Objeto:");

        // Traslación
        if (ImGui::DragFloat3("Traslacion", &m_objectTranslation.x, 0.01f))
        {
            // Actualizar automáticamente
        }

        // Escala
        if (ImGui::DragFloat3("Escala", &m_objectScale.x, 0.01f, 0.01f, 10.0f))
        {
            // Actualizar automáticamente
        }

        // Rotación (mostrar como ángulos de Euler)
        glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(m_objectRotation));
        if (ImGui::DragFloat3("Rotacion (grados)", &eulerAngles.x, 1.0f))
        {
            m_objectRotation = glm::quat(glm::radians(eulerAngles));
        }

        if (ImGui::Button("Resetear Transformaciones"))
        {
            m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
            m_objectScale = glm::vec3(1.0f);
            m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_selectedSubMesh = -1;

            // Resetear transformaciones de submeshes
            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            for (auto& sm : subMeshes)
            {
                sm.translation = glm::vec3(0.0f);
            }
        }

        // Info de sub-mesh seleccionado
        if (m_selectedSubMesh >= 0)
        {
            ImGui::Separator();
            ImGui::Text("Sub-mesh Seleccionado:");
            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            auto& selectedSM = subMeshes[m_selectedSubMesh];

            if (ImGui::DragFloat3("Traslacion Sub-mesh", &selectedSM.translation.x, 0.01f))
            {
                // Actualizar automáticamente
            }

            ImGui::ColorEdit3("Color Material", &selectedSM.material.Kd.x);
        }
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void C3DViewer::resize(int new_width, int new_height)
{
    width = new_width;
    height = new_height;
    glViewport(0, 0, width, height);
    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.1f, 100.0f);

    // Recrear framebuffer de picking con nuevo tamaño
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
        std::cerr << "Error: Framebuffer de picking no está completo" << std::endl;
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

    // Renderizar cada submesh con su color único
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

    // Leer pixel en posición del mouse (invertir Y)
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