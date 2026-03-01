#define GLM_ENABLE_EXPERIMENTAL
#include "3DViewer.h"
#include <iostream>
#include <cmath> 
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
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
    if (m_lightShaderProgram) glDeleteProgram(m_lightShaderProgram);
    if (m_lightSphereVAO) glDeleteVertexArrays(1, &m_lightSphereVAO);
    if (m_lightSphereVBO) glDeleteBuffers(1, &m_lightSphereVBO);
    if (m_lightSphereEBO) glDeleteBuffers(1, &m_lightSphereEBO);
    // Normal/vertex overlay resources removed
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
    glfwWindowHint(GLFW_SAMPLES, 4);

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

    if (m_lineAntiAlias)
        glEnable(GL_MULTISAMPLE);
    else
        glDisable(GL_MULTISAMPLE);

    // Inicializar estado de depth-test y culling segun flags
    if (m_depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (m_backfaceCullingEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(m_cullFaceMode);
    }
    else {
        glDisable(GL_CULL_FACE);
    }

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

    // Compilar y enlazar shaders principales
    if (!setupShader()) return false;
    initLights();
    if (!setupLightVisualization()) return false;

    if (width <= 0 || height <= 0)
    {
        width = 1280;
        height = 720;
    }

    // Actualiza viewport y matrices cuando la ventana cambia de tamaño
    glViewport(0, 0, width, height);

    // Crear viewport y configurar estados GL iniciales
    glfwSetKeyCallback(m_window, keyCallbackStatic);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallbackStatic);
    glfwSetCursorPosCallback(m_window, cursorPosCallbackStatic);

    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.1f, 100.0f);

    // Inicializar vectores de camara a partir de posicion/target actuales
    m_cameraFront = glm::normalize(m_cameraTarget - m_cameraPos);
    // Derivar yaw/pitch desde front
    m_cameraYaw = glm::degrees(std::atan2(m_cameraFront.z, m_cameraFront.x));
    m_cameraPitch = glm::degrees(std::asin(glm::clamp(m_cameraFront.y, -1.0f, 1.0f)));
    m_worldUp = m_cameraUp;
    computeCameraVectors();
    updateViewMatrix();

    m_modelMatrix = glm::mat4(1.0f);

    return true;
}

void C3DViewer::update()
{

    // Calcular FPS promedio en ventana deslizante (m_fpsWindowSeconds)
    double now = glfwGetTime();
    m_frameTimestamps.push_back(now);

    // Eliminar timestamps anteriores a la ventana
    while (!m_frameTimestamps.empty() && (now - m_frameTimestamps.front()) > m_fpsWindowSeconds)
    {
        m_frameTimestamps.pop_front();
    }

    // Calcular FPS: numero de frames en ventana / duracion real (mejor que dividir por ventana fija)
    double span = m_frameTimestamps.empty() ? 0.0 : (m_frameTimestamps.back() - m_frameTimestamps.front());
    if (span > 1e-6 && m_frameTimestamps.size() > 1)
    {
        m_fpsAverage = (double)(m_frameTimestamps.size() - 1) / span;
    }
    else
    {
        // Si no hay suficiente historial, aproximamos con el ultimo delta si fuera posible
        m_fpsAverage = 0.0;
    }

    double deltaTime = 0.0;
    if (m_lastLightUpdateTime > 0.0)
    {
        deltaTime = now - m_lastLightUpdateTime;
    }
    m_lastLightUpdateTime = now;
    updateLightAnimation(deltaTime);
}

void C3DViewer::mainLoop()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        // Usar color de fondo editable
        glClearColor(m_backgroundColor.r, m_backgroundColor.g, m_backgroundColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render();

        glfwSwapBuffers(m_window);
    }
}

void C3DViewer::onKey(int key, int scancode, int action, int mods)
{
    // aceptar press y repeat para movimiento continuo
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        else if (key == GLFW_KEY_O)
            loadOBJFile();
        // MOVIMIENTO CAMERA - adelante / atras en direccion front (UP / DOWN)
        else if (key == GLFW_KEY_UP)
        {
            glm::vec3 delta = m_cameraFront * m_cameraSpeed;
            m_cameraPos += delta;
            m_cameraTarget += delta;
            updateViewMatrix();
        }
        else if (key == GLFW_KEY_DOWN)
        {
            glm::vec3 delta = m_cameraFront * m_cameraSpeed;
            m_cameraPos -= delta;
            m_cameraTarget -= delta;
            updateViewMatrix();
        }
        // ROTACION CAMERA con teclas LEFT/RIGHT (gira yaw)
        else if (key == GLFW_KEY_LEFT)
        {
            float step = 5.0f; // grados por pulsacion
            m_cameraYaw -= step;
            computeCameraVectors();
            updateViewMatrix();
        }
        else if (key == GLFW_KEY_RIGHT)
        {
            float step = 5.0f;
            m_cameraYaw += step;
            computeCameraVectors();
            updateViewMatrix();
        }
    }
}


void C3DViewer::onMouseButton(int button, int action, int mods)
{

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
    // Maneja movimiento del cursor para rotar/trasladar segun boton
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        return;
    }

    double deltaX = xpos - m_lastMouseX;
    double deltaY = ypos - m_lastMouseY;

    // Si mantiene pulsado boton izquierdo el codigo existente rota el objeto;
    // si mantiene pulsado boton medio (wheel) rotamos la camara (mouse-look)
    if (mouseButtonsDown[GLFW_MOUSE_BUTTON_MIDDLE] && m_mouseLookEnabled)
    {
        m_isDragging = true;
        // sensibilidad: grados por pixel
        float sx = static_cast<float>(deltaX) * m_mouseSensitivity;
        float sy = static_cast<float>(deltaY) * m_mouseSensitivity;

        // Yaw aumenta con movimiento X, pitch decrece con movimiento Y (invertido)
        m_cameraYaw += sx;
        m_cameraPitch -= sy;

        // limitar pitch para evitar flip
        if (m_cameraPitch > 89.0f) m_cameraPitch = 89.0f;
        if (m_cameraPitch < -89.0f) m_cameraPitch = -89.0f;

        computeCameraVectors();
        updateViewMatrix();
    }
    else
    {
        // Mantener comportamiento previo para rotar/trasladar objeto
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

            // Trasladar objeto completo
            m_objectTranslation += translation;
        }
    }

    m_lastMouseX = xpos;
    m_lastMouseY = ypos;
}

void C3DViewer::loadOBJFile()
{
    // Abre dialog y carga un archivo OBJ en memoria
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
            // overlay generation (normals/vertex points) removed
            std::cout << "OBJ cargado exitosamente" << std::endl;
        }
        else
        {
            std::cerr << "Error al cargar el archivo OBJ" << std::endl;
        }
    }
}

void C3DViewer::renderOBJ()
{
    // Renderiza la escena 3D: relleno principal iluminado
    if (!m_objLoaded) return;

    glUseProgram(m_shaderProgram);
    uploadLightUniforms();

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

    glUniform3fv(glGetUniformLocation(m_shaderProgram, "viewPos"), 1, glm::value_ptr(m_cameraPos));

    GLint modelLoc = glGetUniformLocation(m_shaderProgram, "model");
    GLint materialKaLoc = glGetUniformLocation(m_shaderProgram, "materialKa");
    GLint materialKdLoc = glGetUniformLocation(m_shaderProgram, "materialKd");
    GLint materialKsLoc = glGetUniformLocation(m_shaderProgram, "materialKs");
    GLint hasAmbientMapLoc = glGetUniformLocation(m_shaderProgram, "hasAmbientMap");
    GLint hasDiffuseMapLoc = glGetUniformLocation(m_shaderProgram, "hasDiffuseMap");
    GLint hasSpecularMapLoc = glGetUniformLocation(m_shaderProgram, "hasSpecularMap");
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texAmbient"), 0);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texDiffuse"), 1);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texSpecular"), 2);

    // 1) DIBUJAR RELLENO — siempre renderizamos el relleno de los sub-meshes
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(m_fillPolygonOffsetFactor, m_fillPolygonOffsetUnits);

        for (size_t i = 0; i < m_objLoader.getSubMeshes().size(); ++i)
        {
            const auto& subMesh = m_objLoader.getSubMeshes()[i];

            glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), subMesh.translation);
            m_modelMatrix = subMeshTransform * baseModel;

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m_modelMatrix));

            const Material& material = subMesh.material;
            glUniform3fv(materialKaLoc, 1, glm::value_ptr(material.Ka));
            glUniform3fv(materialKdLoc, 1, glm::value_ptr(material.Kd));
            glUniform3fv(materialKsLoc, 1, glm::value_ptr(material.Ks));

            bool hasAmbient = material.ambientTexture != 0;
            bool hasDiffuse = material.diffuseTexture != 0;
            bool hasSpecular = material.specularTexture != 0;

            glUniform1i(hasAmbientMapLoc, hasAmbient ? 1 : 0);
            glUniform1i(hasDiffuseMapLoc, hasDiffuse ? 1 : 0);
            glUniform1i(hasSpecularMapLoc, hasSpecular ? 1 : 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, hasAmbient ? material.ambientTexture : 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, hasDiffuse ? material.diffuseTexture : 0);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, hasSpecular ? material.specularTexture : 0);

            glBindVertexArray(subMesh.VAO);
            glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Normal and vertex overlays removed
}

void C3DViewer::computeCameraVectors()
{
    // Recalcula vectores de camara (front, right, up) desde yaw/pitch
    // Convertir yaw/pitch (grados) a vector front
    glm::vec3 front;
    front.x = std::cos(glm::radians(m_cameraYaw)) * std::cos(glm::radians(m_cameraPitch));
    front.y = std::sin(glm::radians(m_cameraPitch));
    front.z = std::sin(glm::radians(m_cameraYaw)) * std::cos(glm::radians(m_cameraPitch));
    m_cameraFront = glm::normalize(front);

    // right y up
    m_cameraRight = glm::normalize(glm::cross(m_cameraFront, m_worldUp));
    m_cameraUp = glm::normalize(glm::cross(m_cameraRight, m_cameraFront));
    // actualizar target coherente
    m_cameraTarget = m_cameraPos + m_cameraFront;
}

void C3DViewer::updateViewMatrix()
{
    // Actualiza la matriz view usando posicion y front de la camara
    m_viewMatrix = glm::lookAt(m_cameraPos, m_cameraPos + m_cameraFront, m_cameraUp);
}

void C3DViewer::render()
{
    update();

    if (m_objLoaded)
    {
        renderOBJ();
    }

    renderLightIndicators();

    drawInterface();
}

void C3DViewer::drawInterface()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(400, (float)height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Control Panel", nullptr, panelFlags);

    ImGui::Text("Presiona 'O' para cargar OBJ");

    if (ImGui::Button("Abrir archivo OBJ"))
    {
        loadOBJFile();
    }
   

    if (m_objLoaded)
    { 
        ImGui::SameLine();
        if (ImGui::Button("Guardar OBJ modificado"))
        {
            saveOBJFile();
        }
        ImGui::Separator();
        ImGui::Text("OBJ Cargado");
        ImGui::Text("Submeshes: %zu", m_objLoader.getSubMeshes().size());

        ImGui::Separator();
        ImGui::Text("Controles:");
        ImGui::BulletText("Click izq + arrastrar: Rotar");
        ImGui::BulletText("Click der + arrastrar: Trasladar");

        // Render: fondo, FPS, antialiasing, depth-test y culling
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Escena:");

        if (ImGui::Checkbox("Mostrar FPS (promedio 5s)", &m_showFPS))
        {
        }

        if (m_showFPS)
        {
            ImGui::Text("FPS (media %.1fs): %.2f", m_fpsWindowSeconds, m_fpsAverage);
        }

        // Control de antialiasing
        if (ImGui::Checkbox("Antialiasing", &m_lineAntiAlias))
        {
                // suavizado global de geometria + mejoras para lineas
                if (m_lineAntiAlias) {
                glEnable(GL_MULTISAMPLE);
                    glEnable(GL_LINE_SMOOTH);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
            }
                else {
                glDisable(GL_MULTISAMPLE);
                glDisable(GL_LINE_SMOOTH);
                glDisable(GL_BLEND);
               
            }
        }

        if (ImGui::ColorEdit3("Color Fondo", &m_backgroundColor.x))
        {
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Render:");

        // Depth test (z-buffer)
        if (ImGui::Checkbox("Depth Test (Z-buffer)", &m_depthTestEnabled))
        {
            if (m_depthTestEnabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
        }

        // Back-face culling
        if (ImGui::Checkbox("Back-face Culling", &m_backfaceCullingEnabled))
        {
            if (m_backfaceCullingEnabled)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(m_cullFaceMode);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
        }

        // (Opciones de relleno/alambrado removidas)

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Luces:");
        ImGui::SliderFloat("Velocidad animacion", &m_lightAnimationSpeed, 0.0f, 3.0f, "%.2fx");

        const char* shadingModes[] = { "Phong", "Blinn-Phong", "Flat" };
        for (int i = 0; i < MAX_LIGHTS; ++i)
        {
            ImGui::PushID(i);
            std::string label = "Luz " + std::to_string(i + 1);
            ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "%s", label.c_str());
            ImGui::Checkbox("Activa", &m_lights[i].enabled);
            ImGui::ColorEdit3("Ambiental", &m_lights[i].ambient.x);
            ImGui::ColorEdit3("Difusa", &m_lights[i].diffuse.x);
            ImGui::ColorEdit3("Especular", &m_lights[i].specular.x);
            ImGui::Checkbox("Atenuacion", &m_lights[i].attenuationEnabled);
            ImGui::Combo("Modelo", &m_lights[i].shadingModel, shadingModes, IM_ARRAYSIZE(shadingModes));
            ImGui::PopID();
            if (i < MAX_LIGHTS - 1)
            {
                ImGui::Separator();
            }
        }

        // Geometry overlay options removed (normals/vertices)

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Transformaciones del Objeto:");


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

        if (ImGui::Button("Centrar sub-mallas"))
        {
            m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);

            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            for (auto& sm : subMeshes)
            {
                sm.translation = glm::vec3(0.0f);
            }
        }

        if (ImGui::Button("Resetear Transformaciones"))
        {
            m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
            m_objectScale = glm::vec3(1.0f);
            m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
            for (auto& sm : subMeshes)
            {
                sm.translation = glm::vec3(0.0f);
            }
        }

        // (Opciones de bounding box removidas)
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void C3DViewer::resize(int new_width, int new_height)
{
    // Evitar division por cero
    if (new_width <= 0 || new_height <= 0)
    {
        std::cerr << "Advertencia: Dimensiones invalidas ("
            << new_width << "x" << new_height << "), ignorando resize" << std::endl;
        return;
    }
    // Establecer dimensiones minimas
    width = std::max(1, new_width);
    height = std::max(1, new_height);

    glViewport(0, 0, width, height);
    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.1f, 100.0f);

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

// Bounding box functionality removed.

// Normal and vertex overlay functionality removed.

void C3DViewer::initLights()
{
    const float phaseStep = glm::two_pi<float>() / static_cast<float>(MAX_LIGHTS);

    m_lights[0].ambient = glm::vec3(0.25f, 0.07f, 0.05f);
    m_lights[0].diffuse = glm::vec3(1.0f, 0.3f, 0.2f);
    m_lights[0].specular = glm::vec3(1.0f, 0.5f, 0.4f);

    m_lights[1].ambient = glm::vec3(0.07f, 0.25f, 0.08f);
    m_lights[1].diffuse = glm::vec3(0.3f, 1.0f, 0.3f);
    m_lights[1].specular = glm::vec3(0.5f, 1.0f, 0.5f);

    m_lights[2].ambient = glm::vec3(0.05f, 0.08f, 0.25f);
    m_lights[2].diffuse = glm::vec3(0.25f, 0.35f, 1.0f);
    m_lights[2].specular = glm::vec3(0.45f, 0.55f, 1.0f);

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        auto& light = m_lights[i];
        light.orbitRadius = 2.5f;
        light.orbitHeight = 1.2f;
        light.orbitSpeed = 0.6f + 0.15f * i;
        light.phaseOffset = phaseStep * static_cast<float>(i);
        light.orbitAngle = 0.0f;
        light.enabled = true;
        light.attenuationEnabled = true;
        light.shadingModel = 0; // Phong por defecto
        float angle = light.phaseOffset;
        light.position = glm::vec3(
            light.orbitRadius * std::cos(angle),
            light.orbitHeight,
            light.orbitRadius * std::sin(angle));
    }

    m_lightAnimationSpeed = 1.0f;
    m_lastLightUpdateTime = 0.0;
    m_lightGlobalAngle = 0.0f;
}

void C3DViewer::updateLightAnimation(double deltaTime)
{
    if (deltaTime <= 0.0)
        return;

    float baseSpeed = (MAX_LIGHTS > 0) ? m_lights[0].orbitSpeed : 0.0f;
    m_lightGlobalAngle += baseSpeed * m_lightAnimationSpeed * static_cast<float>(deltaTime);

    for (auto& light : m_lights)
    {
        float angle = m_lightGlobalAngle + light.phaseOffset;
        light.orbitAngle = angle;
        light.position.x = light.orbitRadius * std::cos(angle);
        light.position.z = light.orbitRadius * std::sin(angle);
        light.position.y = light.orbitHeight + 0.2f * std::sin(angle * 0.5f);
    }
}

void C3DViewer::uploadLightUniforms()
{
    if (m_shaderProgram == 0)
        return;

    glm::vec3 positions[MAX_LIGHTS];
    glm::vec3 ambients[MAX_LIGHTS];
    glm::vec3 diffuses[MAX_LIGHTS];
    glm::vec3 speculars[MAX_LIGHTS];
    int enabled[MAX_LIGHTS];
    int shadingModels[MAX_LIGHTS];
    int attenuationFlags[MAX_LIGHTS];

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        positions[i] = m_lights[i].position;
        ambients[i] = m_lights[i].ambient;
        diffuses[i] = m_lights[i].diffuse;
        speculars[i] = m_lights[i].specular;
        enabled[i] = m_lights[i].enabled ? 1 : 0;
        shadingModels[i] = m_lights[i].shadingModel;
        attenuationFlags[i] = m_lights[i].attenuationEnabled ? 1 : 0;
    }

    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightPos"), MAX_LIGHTS, glm::value_ptr(positions[0]));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightAmbient"), MAX_LIGHTS, glm::value_ptr(ambients[0]));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightDiffuse"), MAX_LIGHTS, glm::value_ptr(diffuses[0]));
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "lightSpecular"), MAX_LIGHTS, glm::value_ptr(speculars[0]));
    glUniform1iv(glGetUniformLocation(m_shaderProgram, "lightEnabled"), MAX_LIGHTS, enabled);
    glUniform1iv(glGetUniformLocation(m_shaderProgram, "lightShadingModel"), MAX_LIGHTS, shadingModels);
    glUniform1iv(glGetUniformLocation(m_shaderProgram, "lightUseAttenuation"), MAX_LIGHTS, attenuationFlags);
}

bool C3DViewer::setupLightVisualization()
{
    return setupLightSphereMesh() && setupLightShader();
}

bool C3DViewer::setupLightSphereMesh()
{
    if (m_lightSphereVAO != 0)
        return true;

    const unsigned int stacks = 12;
    const unsigned int slices = 18;
    std::vector<glm::vec3> vertices;
    vertices.reserve((stacks + 1) * (slices + 1));
    std::vector<unsigned int> indices;
    indices.reserve(stacks * slices * 6);

    for (unsigned int stack = 0; stack <= stacks; ++stack)
    {
        float v = static_cast<float>(stack) / static_cast<float>(stacks);
        float phi = glm::pi<float>() * v;
        for (unsigned int slice = 0; slice <= slices; ++slice)
        {
            float u = static_cast<float>(slice) / static_cast<float>(slices);
            float theta = glm::two_pi<float>() * u;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            vertices.emplace_back(x, y, z);
        }
    }

    for (unsigned int stack = 0; stack < stacks; ++stack)
    {
        for (unsigned int slice = 0; slice < slices; ++slice)
        {
            unsigned int first = stack * (slices + 1) + slice;
            unsigned int second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    m_lightSphereIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &m_lightSphereVAO);
    glGenBuffers(1, &m_lightSphereVBO);
    glGenBuffers(1, &m_lightSphereEBO);

    glBindVertexArray(m_lightSphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_lightSphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lightSphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return true;
}

bool C3DViewer::setupLightShader()
{
    if (m_lightShaderProgram != 0)
        return true;

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &lightIndicatorVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);
    if (!checkCompileErrors(vertexShader, "LIGHT_VERTEX")) return false;

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &lightIndicatorFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);
    if (!checkCompileErrors(fragmentShader, "LIGHT_FRAGMENT")) return false;

    m_lightShaderProgram = glCreateProgram();
    glAttachShader(m_lightShaderProgram, vertexShader);
    glAttachShader(m_lightShaderProgram, fragmentShader);
    glLinkProgram(m_lightShaderProgram);
    if (!checkCompileErrors(m_lightShaderProgram, "LIGHT_PROGRAM")) return false;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void C3DViewer::renderLightIndicators()
{
    if (m_lightShaderProgram == 0 || m_lightSphereVAO == 0)
        return;

    glUseProgram(m_lightShaderProgram);

    GLint viewLoc = glGetUniformLocation(m_lightShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_lightShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(m_lightShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(m_lightShaderProgram, "lightColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

    glBindVertexArray(m_lightSphereVAO);

    for (const auto& light : m_lights)
    {
        glm::vec3 displayColor = light.enabled ? light.diffuse : glm::vec3(0.1f);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), light.position);
        model = glm::scale(model, glm::vec3(0.12f));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(colorLoc, 1, glm::value_ptr(displayColor));
        glDrawElements(GL_TRIANGLES, m_lightSphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}

void C3DViewer::saveOBJFile()
{
    const char* filterPatterns[1] = { "*.obj" };
    const char* filePath = tinyfd_saveFileDialog(
        "Guardar OBJ modificado",
        "modified.obj",
        1,
        filterPatterns,
        "Archivos OBJ (*.obj)"
    );

    if (filePath)
    {
        std::string path(filePath);
        if (saveOBJWithMTL(path))
        {
            std::cout << "OBJ guardado en: " << path << std::endl;
        }
        else
        {
            std::cerr << "Error al guardar OBJ en: " << path << std::endl;
        }
    }
}

bool C3DViewer::saveOBJWithMTL(const std::string& objPath)
{
    if (!m_objLoaded) {
        std::cerr << "No hay OBJ cargado para exportar." << std::endl;
        return false;
    }

    // Derivar ruta .mtl
    std::string mtlPath = objPath;
    size_t dot = mtlPath.find_last_of('.');
    if (dot != std::string::npos)
        mtlPath = mtlPath.substr(0, dot) + ".mtl";
    else
        mtlPath += ".mtl";

    std::ofstream mtlFile(mtlPath, std::ios::out);
    if (!mtlFile.is_open()) {
        std::cerr << "No se pudo abrir MTL para escritura: " << mtlPath << std::endl;
        return false;
    }

    std::ofstream objFile(objPath, std::ios::out);
    if (!objFile.is_open()) {
        std::cerr << "No se pudo abrir OBJ para escritura: " << objPath << std::endl;
        mtlFile.close();
        return false;
    }

    // Escribir referencia al MTL
    objFile << "# Exportado por OBJ Viewer\n";
    // Obtener solo el nombre de fichero del .mtl 
    std::string mtlFilename;
    size_t slash = mtlPath.find_last_of("/\\");
    if (slash != std::string::npos)
        mtlFilename = mtlPath.substr(slash + 1);
    else
        mtlFilename = mtlPath;
    objFile << "mtllib " << mtlFilename << "\n";

    // Preparar matrices globales, aplicar las mismas transformaciones que en render
    glm::mat4 normalizationMatrix = glm::mat4(1.0f);
    normalizationMatrix = glm::scale(normalizationMatrix, m_objLoader.getScaleFactor() * m_objectScale);
    normalizationMatrix = glm::translate(normalizationMatrix, -m_objLoader.getCenter());

    glm::mat4 rotationMatrix = glm::mat4_cast(m_objectRotation);
    glm::mat4 objectTransform = glm::translate(glm::mat4(1.0f), m_objectTranslation);
    glm::mat4 baseModel = objectTransform * rotationMatrix * normalizationMatrix;

    // Colecciones globales para indices
    size_t vertexOffset = 0;
    size_t normalOffset = 0;

    // Guardar materiales en MTL, uno por sub-mesh
    const auto& subMeshes = m_objLoader.getSubMeshes();
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        const auto& sm = subMeshes[i];
        std::string matName = "mat_" + std::to_string(i);
        mtlFile << "newmtl " << matName << "\n";
        // Kd (difuso) — usar color actual del submesh
        mtlFile << "Kd " << std::fixed << std::setprecision(6)
            << sm.material.Kd.r << " " << sm.material.Kd.g << " " << sm.material.Kd.b << "\n";
        mtlFile << "Ka 0.000000 0.000000 0.000000\n";
        mtlFile << "Ks 0.100000 0.100000 0.100000\n";
        mtlFile << "Ns 10.0\n\n";
    }

    // Ahora escribir vertices/normales y caras en OBJ — mantener indices globales
    // Primero, recorrer sub-meshes y volcar vertices y normales transformados
    std::vector<glm::vec3> allNormals; allNormals.reserve(1024);
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        const auto& sm = subMeshes[i];
        glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), sm.translation);
        std::vector<glm::vec3> allNormals; allNormals.reserve(1024);
        for (size_t i = 0; i < subMeshes.size(); ++i)
        {
            const auto& sm = subMeshes[i];
            glm::mat4 subMeshTransform = glm::translate(glm::mat4(1.0f), sm.translation);
            glm::mat4 model = baseModel * subMeshTransform;

            // mat3 para normales (inv-transpose) basada en la misma matriz 'model'
            glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

            for (const auto& v : sm.vertices)
            {
                glm::vec4 vt = model * glm::vec4(v, 1.0f);
                objFile << "v " << std::fixed << std::setprecision(6)
                    << vt.x << " " << vt.y << " " << vt.z << "\n";
            }

            // normales (si existen en el submesh) tambien transformadas y normalizadas
            if (!sm.normals.empty())
            {
                for (const auto& n : sm.normals)
                {
                    glm::vec3 nt = glm::normalize(normalMat * n);
                    allNormals.push_back(nt);
                    objFile << "vn " << std::fixed << std::setprecision(6)
                        << nt.x << " " << nt.y << " " << nt.z << "\n";
                }
            }
            else
            {
                // Si no hay normales, no escribiremos vn
            }
        }
    }

    // Ahora escribir las caras por sub-mesh (usando offsets)
    vertexOffset = 0;
    normalOffset = 0;
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        const auto& sm = subMeshes[i];
        std::string matName = "mat_" + std::to_string(i);

        objFile << "\n# SubMesh " << i << "\n";
        objFile << "g SubMesh_" << i << "\n";
        objFile << "usemtl " << matName << "\n";

        bool hasNormals = !sm.normals.empty();

        // Las caras usan indices basados en la cantidad global pasada hasta ahora
        for (size_t f = 0; f + 2 < sm.indices.size(); f += 3)
        {
            int ia = sm.indices[f + 0];
            int ib = sm.indices[f + 1];
            int ic = sm.indices[f + 2];

            // OBJ usa indices 1-based, y añadimos vertexOffset
            if (hasNormals)
            {
                // asumir correspondencia vertice->normal (por indice)
                objFile << "f "
                    << (vertexOffset + ia + 1) << "//" << (normalOffset + ia + 1) << " "
                    << (vertexOffset + ib + 1) << "//" << (normalOffset + ib + 1) << " "
                    << (vertexOffset + ic + 1) << "//" << (normalOffset + ic + 1) << "\n";
            }
            else
            {
                objFile << "f "
                    << (vertexOffset + ia + 1) << " "
                    << (vertexOffset + ib + 1) << " "
                    << (vertexOffset + ic + 1) << "\n";
            }
        }

        vertexOffset += sm.vertices.size();
        if (!sm.normals.empty())
            normalOffset += sm.normals.size();
    }

    objFile.close();
    mtlFile.close();

    return true;
}