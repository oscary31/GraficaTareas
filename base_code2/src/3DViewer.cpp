#define GLM_ENABLE_EXPERIMENTAL
#include "3DViewer.h"
#include <iostream>
#include <cmath> 
#include <algorithm>
#include <cstring>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include "tinyfiledialogs.h"
#include <stb_image.h>

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
    if (m_skyboxShaderProgram) glDeleteProgram(m_skyboxShaderProgram);
    if (m_skyboxVAO) glDeleteVertexArrays(1, &m_skyboxVAO);
    if (m_skyboxVBO) glDeleteBuffers(1, &m_skyboxVBO);
    if (m_skyboxTexture) glDeleteTextures(1, &m_skyboxTexture);
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

    // Mostrar un primer frame de carga para evitar ventana en blanco
    renderLoadingScreen("Cargando escena...");

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
    if (!setupSkybox())
    {
        std::cerr << "Warning: No se pudo cargar la skybox." << std::endl;
    }
    loadOBJFile();

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
    glfwGetCursorPos(m_window, &m_lastMouseX, &m_lastMouseY);
    m_hasMousePosition = true;

    m_projectionMatrix = glm::perspective(glm::radians(45.0f),
        (float)width / (float)height,
        0.01f, 100.0f);

    // Inicializar vectores de camara a partir de posicion/target actuales
    m_cameraFront = glm::normalize(m_cameraTarget - m_cameraPos);
    // Derivar yaw/pitch desde front
    m_cameraYaw = glm::degrees(std::atan2(m_cameraFront.z, m_cameraFront.x));
    m_cameraPitch = glm::degrees(std::asin(glm::clamp(m_cameraFront.y, -1.0f, 1.0f)));
    m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    computeCameraVectors();
    updateViewMatrix();

    m_modelMatrix = glm::mat4(1.0f);

    return true;
}

void C3DViewer::renderLoadingScreen(const std::string& message)
{
    if (!m_window)
        return;

    // Start a new ImGui frame and draw a centered message
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Fullscreen opaque window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,1));
    ImGui::Begin("##LoadingScreen", nullptr, flags);

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(message.c_str()).x) * 0.5f);
    ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetFontSize()) * 0.5f);
    ImGui::Text(message.c_str());

    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::Render();

    // Clear and render
    glClearColor(m_backgroundColor.x, m_backgroundColor.y, m_backgroundColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwPollEvents();
    glfwSwapBuffers(m_window);
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
    updateHowlPanAnimation(deltaTime);
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
        else if (m_cameraMovementMode == 1)
        {
            glm::vec3 delta(0.0f);
            glm::vec3 planarForward(m_cameraFront.x, 0.0f, m_cameraFront.z);
            if (glm::length(planarForward) > 1e-6f)
                planarForward = glm::normalize(planarForward);
            else
                planarForward = glm::vec3(0.0f, 0.0f, -1.0f);

            glm::vec3 planarRight(m_cameraRight.x, 0.0f, m_cameraRight.z);
            if (glm::length(planarRight) > 1e-6f)
                planarRight = glm::normalize(planarRight);
            else
                planarRight = glm::vec3(1.0f, 0.0f, 0.0f);

            if (key == GLFW_KEY_W)
                delta += planarForward * m_cameraSpeed;
            else if (key == GLFW_KEY_S)
                delta -= planarForward * m_cameraSpeed;
            else if (key == GLFW_KEY_A)
                delta -= planarRight * m_cameraSpeed;
            else if (key == GLFW_KEY_D)
                delta += planarRight * m_cameraSpeed;
            else if (key == GLFW_KEY_E)
                delta -= m_worldUp * m_cameraSpeed;
            else if (key == GLFW_KEY_Q)
                delta += m_worldUp * m_cameraSpeed;

            if (glm::length(delta) > 0.0f)
            {
                m_cameraPos += delta;
                m_cameraTarget += delta;
                updateViewMatrix();
            }
        }
        // MOVIMIENTO CAMERA - adelante / atras en direccion front (UP / DOWN)
        else if (key == GLFW_KEY_UP)
        {
            glm::vec3 delta = getCameraMovementDirection() * m_cameraSpeed;
            m_cameraPos += delta;
            m_cameraTarget += delta;
            updateViewMatrix();
        }
        else if (key == GLFW_KEY_DOWN)
        {
            glm::vec3 delta = getCameraMovementDirection() * m_cameraSpeed;
            m_cameraPos -= delta;
            m_cameraTarget -= delta;
            updateViewMatrix();
        }
        // ROTACION CAMERA con teclas LEFT/RIGHT (gira yaw)
        else if (m_cameraMovementMode == 0 && key == GLFW_KEY_LEFT)
        {
            float step = 5.0f; // grados por pulsacion
            m_cameraYaw -= step;
            computeCameraVectors();
            updateViewMatrix();
        }
        else if (m_cameraMovementMode == 0 && key == GLFW_KEY_RIGHT)
        {
            float step = 5.0f;
            m_cameraYaw += step;
            computeCameraVectors();
            updateViewMatrix();
        }
    }
}

glm::vec3 C3DViewer::getCameraMovementDirection() const
{
    if (m_cameraMovementMode == 1)
        return m_cameraFront;

    glm::vec3 moveDirection(m_cameraFront.x, 0.0f, m_cameraFront.z);
    if (glm::length(moveDirection) <= 1e-6f)
        return glm::vec3(0.0f, 0.0f, -1.0f);

    return glm::normalize(moveDirection);
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
            if (m_cameraMovementMode == 0)
            {
                mouseButtonsDown[button] = (button == GLFW_MOUSE_BUTTON_LEFT);
            }
            else
            {
                mouseButtonsDown[button] = true;
            }
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
        m_hasMousePosition = true;
        return;
    }

    if (!m_hasMousePosition)
    {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        m_hasMousePosition = true;
        return;
    }

    double deltaX = xpos - m_lastMouseX;
    double deltaY = ypos - m_lastMouseY;

    if (m_mouseLookEnabled && mouseButtonsDown[GLFW_MOUSE_BUTTON_LEFT])
    {
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

    m_lastMouseX = xpos;
    m_lastMouseY = ypos;
}

void C3DViewer::loadOBJFile()
{
	// Cargar el modelo principal (mesa)
    const std::string objPath = "src/Objetos/table/table.obj";
    std::cout << "Cargando: " << objPath << std::endl;
    if (m_objLoader.load(objPath))
    {
        // Configuraciones iniciales para el modelo principal
        m_objLoaded = true;
        m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
        m_objectScale = glm::vec3(1.0f, 1.2f, 1.2f);
        m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        auto& subMeshes = const_cast<std::vector<SubMesh>&>(m_objLoader.getSubMeshes());
        for (auto& sm : subMeshes)
        {
            sm.translation = glm::vec3(0.0f);
        }

        placeCameraOnTable();
        loadSceneProps();
        updateScenePropsPlacement();
        std::cout << "OBJ cargado exitosamente" << std::endl;
    }
    else
    {
        std::cerr << "Error al cargar el archivo OBJ" << std::endl;
    }
}

bool C3DViewer::loadSceneProps()
{
    const std::string stovePath = "src/Objetos/stove/kitchen_stove.obj";
    const std::string howlPath = "src/Objetos/howl/howls_moving_castle_breakfast.obj";
    const std::string jugPath = "src/Objetos/Old_Copper_Jug_obj/Old_Copper_Jug.obj";
	const std::string platePath = "src/Objetos/plate-and-ustensils/plate-and-ustensils.obj";
	const std::string axePath = "src/Objetos/axe/Modern_wooden_axe.obj";

    m_stoveLoaded = m_stoveLoader.load(stovePath);
    if (!m_stoveLoaded)
    {
        std::cerr << "Warning: No se pudo cargar stove: " << stovePath << std::endl;
    }
    
    m_howlLoaded = m_howlLoader.load(howlPath);
    if (!m_howlLoaded)
    {
        std::cerr << "Warning: No se pudo cargar howl: " << howlPath << std::endl;
    }

    m_jugLoaded = m_jugLoader.load(jugPath);
    if (!m_jugLoaded)
    {
        std::cerr << "Warning: No se pudo cargar jug: " << jugPath << std::endl;
    }

    m_plateLoaded = m_plateLoader.load(platePath);
    if (!m_plateLoaded)
    {
        std::cerr << "Warning: No se pudo cargar plate: " << platePath << std::endl;
    }

    m_axeLoaded = m_axeLoader.load(axePath);
    if (!m_axeLoaded)
    {
        std::cerr << "Warning: No se pudo cargar axe: " << axePath << std::endl;
    }

    return m_stoveLoaded || m_howlLoaded || m_jugLoaded || m_plateLoaded || m_axeLoaded;
}

glm::vec3 C3DViewer::getNormalizedMinBounds(const OBJLoader& loader, const glm::vec3& scale) const
{
    return (loader.getMinBounds() - loader.getCenter()) * loader.getScaleFactor() * scale;
}

glm::vec3 C3DViewer::getNormalizedMaxBounds(const OBJLoader& loader, const glm::vec3& scale) const
{
    return (loader.getMaxBounds() - loader.getCenter()) * loader.getScaleFactor() * scale;
}

void C3DViewer::updateScenePropsPlacement()
{
    if (!m_objLoaded)
        return;

    glm::vec3 tableMin = getNormalizedMinBounds(m_objLoader, m_objectScale);
    glm::vec3 tableMax = getNormalizedMaxBounds(m_objLoader, m_objectScale);
    glm::vec3 tableSize = tableMax - tableMin;

    glm::vec3 tableCenterWorld = m_objectTranslation;
    glm::vec3 toCamera = glm::vec3(m_cameraPos.x - tableCenterWorld.x, 0.0f, m_cameraPos.z - tableCenterWorld.z);
    glm::vec3 awayFromCamera = (glm::length(toCamera) > 1e-6f)
        ? -glm::normalize(toCamera)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 tableAnchor = tableCenterWorld + glm::vec3(
        awayFromCamera.x * tableSize.x * 0.25f,
        0.0f,
        awayFromCamera.z * tableSize.z * 0.25f);
    float tableTopY = m_objectTranslation.y + tableMax.y;

    if (m_stoveLoaded)
    {
        glm::vec3 stoveMin = getNormalizedMinBounds(m_stoveLoader, m_stoveScale);
        glm::vec3 stoveMax = getNormalizedMaxBounds(m_stoveLoader, m_stoveScale);
        m_stoveTranslation = glm::vec3(
            tableAnchor.x - 0.3f,
            tableTopY - stoveMin.y,
            tableAnchor.z);
        m_stoveRotation = glm::quat(glm::vec3(0.0f, glm::radians(360.0f), 0.0f));

        if (m_howlLoaded)
        {
            glm::vec3 howlMin = getNormalizedMinBounds(m_howlLoader, m_howlScale);
            float stoveTopY = m_stoveTranslation.y + stoveMax.y;
            m_howlTranslation = glm::vec3(
                m_stoveTranslation.x + 0.05f,
                stoveTopY - howlMin.y,
                m_stoveTranslation.z +0.01 );
            m_howlBaseTranslation = m_howlTranslation;
            m_howlAnimationPhase = 0.0f;
            m_howlRotation = glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f));
        }

        // Si se cargo la jarra de metal, posicionarla sobre la mesa cerca del stove
        if (m_jugLoaded)
        {
            
            m_jugScale = glm::vec3(0.12f);

            glm::vec3 jugMin = getNormalizedMinBounds(m_jugLoader, m_jugScale);
            glm::vec3 jugMax = getNormalizedMaxBounds(m_jugLoader, m_jugScale);

            // Tomar posicion basada en el anchor de la mesa y ajustar en X/Z
            glm::vec3 jugPos = glm::vec3(
                tableAnchor.x + tableSize.x * 0.2f,
                tableTopY - jugMin.y,
                tableAnchor.z - tableSize.z * 0.1f);
            m_jugTranslation = jugPos;
            m_jugRotation = glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f));
        }
    }

    // Position plate on the table (near the stove/anchor)
    if (m_plateLoaded)
    {
        m_plateScale = glm::vec3(0.15f);
        glm::vec3 plateMin = getNormalizedMinBounds(m_plateLoader, m_plateScale);
        glm::vec3 plateMax = getNormalizedMaxBounds(m_plateLoader, m_plateScale);
        glm::vec3 platePos = glm::vec3(
            0.23f,
            tableTopY - plateMin.y,
            -2.876);
        m_plateTranslation = platePos;
        m_plateRotation = glm::quat(glm::vec3(0.0f, glm::radians(0.0f), 0.0f));
    }

    // Position axe on the table (to the side, slightly tilted)
    if (m_axeLoaded)
    {
        m_axeScale = glm::vec3(0.4f, 0.3f,0.24f);
        glm::vec3 axeMin = getNormalizedMinBounds(m_axeLoader, m_axeScale);
        glm::vec3 axeMax = getNormalizedMaxBounds(m_axeLoader, m_axeScale);
        /*glm::vec3 axePos = glm::vec3(
            tableAnchor.x - tableSize.x * 0.18f,
            tableTopY - axeMin.y,
            tableAnchor.z - tableSize.z * 0.12f);*/
		glm::vec3 axePos = glm::vec3(-0.27f, tableTopY - axeMin.y - 0.027f, -2.82f);
        m_axeTranslation = axePos;
        m_axeRotation = glm::quat(glm::vec3(glm::radians(90.0f), glm::radians(-5.0f), glm::radians(90.0f)));
    }
}

void C3DViewer::updateHowlPanAnimation(double deltaTime)
{
    if (!m_howlLoaded || !m_howlAnimationEnabled)
        return;

    if (deltaTime <= 0.0)
        return;

    m_howlAnimationPhase += m_howlAnimationSpeed * static_cast<float>(deltaTime);
    if (m_howlAnimationPhase > glm::two_pi<float>())
    {
        m_howlAnimationPhase = std::fmod(m_howlAnimationPhase, glm::two_pi<float>());
    }

    // Y: sube hasta +0.01 y vuelve a la base
    float yOffset = 0.01f * (1.0f - std::cos(m_howlAnimationPhase)) * 0.5f;
    // Z: adelante/atras +/-0.02
    float zOffset = 0.02f * std::sin(m_howlAnimationPhase);

    m_howlTranslation = m_howlBaseTranslation + glm::vec3(0.0f, yOffset, zOffset);
}

void C3DViewer::placeCameraOnTable()
{
    if (!m_objLoaded)
        return;

    glm::vec3 localMin = (m_objLoader.getMinBounds() - m_objLoader.getCenter()) * m_objLoader.getScaleFactor() * m_objectScale;
    glm::vec3 localMax = (m_objLoader.getMaxBounds() - m_objLoader.getCenter()) * m_objLoader.getScaleFactor() * m_objectScale;
    glm::vec3 sceneSize = localMax - localMin;

    float eyeHeight = std::max(0.025f, sceneSize.y * 0.08f);
    float frontMargin = std::max(0.05f, sceneSize.z * 0.15f);
    float startZ = localMax.z - frontMargin;
    if (startZ < localMin.z)
    {
        startZ = (localMin.z + localMax.z) * 0.5f;
    }

    m_cameraPos = glm::vec3(
        m_objectTranslation.x,
        m_objectTranslation.y + localMax.y + eyeHeight,
        m_objectTranslation.z + startZ);

    m_cameraYaw = -90.0f;
    m_cameraPitch = -12.0f;
    m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    computeCameraVectors();
    updateViewMatrix();
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
    GLint useEnvironmentMapLoc = glGetUniformLocation(m_shaderProgram, "useEnvironmentMap");
    GLint environmentReflectivityLoc = glGetUniformLocation(m_shaderProgram, "environmentReflectivity");
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texAmbient"), 0);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texDiffuse"), 1);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texSpecular"), 2);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "texEnvironment"), 3);

    
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(m_fillPolygonOffsetFactor, m_fillPolygonOffsetUnits);

        auto renderLoader = [&](const OBJLoader& loader, const glm::vec3& translation, const glm::vec3& scale, const glm::quat& rotation)
        {
            bool isPlateObject = (&loader == &m_plateLoader);
            bool isJugObject = (&loader == &m_jugLoader);
            bool isAxeObject = (&loader == &m_axeLoader);

            glm::mat4 normalizationMatrix = glm::mat4(1.0f);
            normalizationMatrix = glm::scale(normalizationMatrix, loader.getScaleFactor() * scale);
            normalizationMatrix = glm::translate(normalizationMatrix, -loader.getCenter());

            glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
            glm::mat4 objectTransform = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 baseModel = objectTransform * rotationMatrix * normalizationMatrix;

            for (size_t i = 0; i < loader.getSubMeshes().size(); ++i)
            {
                const auto& subMesh = loader.getSubMeshes()[i];

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

                bool isAxeHeadMaterial = false;
                if (isAxeObject)
                {
                    const std::string& matName = subMesh.materialName.empty() ? material.name : subMesh.materialName;
                    isAxeHeadMaterial = (matName == "Material.002");
                }

                bool useEnvMap = (m_skyboxTexture != 0) && (isPlateObject || isJugObject || isAxeHeadMaterial);
                float envReflectivity = 0.0f;
                if (isAxeHeadMaterial)
                    envReflectivity = 0.9f;
                else if (isJugObject)
                    envReflectivity = 0.52f;
                else if (isPlateObject)
                    envReflectivity = 0.28f;

                glUniform1i(hasAmbientMapLoc, hasAmbient ? 1 : 0);
                glUniform1i(hasDiffuseMapLoc, hasDiffuse ? 1 : 0);
                glUniform1i(hasSpecularMapLoc, hasSpecular ? 1 : 0);
                glUniform1i(useEnvironmentMapLoc, useEnvMap ? 1 : 0);
                glUniform1f(environmentReflectivityLoc, envReflectivity);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, hasAmbient ? material.ambientTexture : 0);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, hasDiffuse ? material.diffuseTexture : 0);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, hasSpecular ? material.specularTexture : 0);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_CUBE_MAP, useEnvMap ? m_skyboxTexture : 0);

                glBindVertexArray(subMesh.VAO);
                glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
        };

        renderLoader(m_objLoader, m_objectTranslation, m_objectScale, m_objectRotation);

        if (m_stoveLoaded)
        {
            renderLoader(m_stoveLoader, m_stoveTranslation, m_stoveScale, m_stoveRotation);
        }

        if (m_howlLoaded)
        {
            renderLoader(m_howlLoader, m_howlTranslation, m_howlScale, m_howlRotation);
        }
        if (m_jugLoaded)
        {
            renderLoader(m_jugLoader, m_jugTranslation, m_jugScale, m_jugRotation);
        }
        if (m_plateLoaded)
        {
            renderLoader(m_plateLoader, m_plateTranslation, m_plateScale, m_plateRotation);
        }

        if (m_axeLoaded)
        {
            renderLoader(m_axeLoader, m_axeTranslation, m_axeScale, m_axeRotation);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

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

    renderSkybox();

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
   

    if (m_objLoaded)
    { 
        
        ImGui::Separator();
        ImGui::Text("Controles:");
        const char* cameraModes[] = { "FPS", "GOD" };
        ImGui::Combo("Modo camara", &m_cameraMovementMode, cameraModes, IM_ARRAYSIZE(cameraModes));
        if (m_cameraMovementMode == 0)
        {
            ImGui::BulletText("Click izq + arrastrar: Rotar camara");
            ImGui::BulletText("Objeto bloqueado en modo FPS");
            ImGui::BulletText("Left/Right: Girar camara");
            ImGui::BulletText("Up/Down: Avanzar y retroceder");
        }
        else
        {
            ImGui::BulletText("Click izq + arrastrar: Rotar camara");
            ImGui::BulletText("W/A/S/D: Mover camara");
            ImGui::BulletText("Q/E: Bajar/Subir camara");
            ImGui::BulletText("Click der + arrastrar: Trasladar objeto");
        }

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

        // Global white light controls
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Luz global:");
        if (ImGui::Checkbox("Activa luz global", &m_globalLightEnabled)) {}
        ImGui::ColorEdit3("Color luz global", &m_globalLightColor.x);
        ImGui::SliderFloat("Intensidad luz global", &m_globalLightIntensity, 0.0f, 5.0f);
        // Boton para resetear luces a configuracion por defecto
        if (ImGui::Button("Resetear luces"))
        {
            // Re-inicializa luces animadas y parametros globales
            initLights();
            m_globalLightEnabled = true;
            m_globalLightIntensity = 1.0f;
            m_globalLightColor = glm::vec3(1.0f);
        }
        ImGui::SameLine();
        // Boton para resetear posicion inicial de la camara
        if (ImGui::Button("Resetear camara"))
        {
            // Coloca camara sobre la mesa (si hay OBJ cargado)
            placeCameraOnTable();
            // Garantizar que las matrices se actualizan
            computeCameraVectors();
            updateViewMatrix();
        }

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

        // Controls for scene props: plate and axe
        if (m_plateLoaded)
        {
            ImGui::Separator();
            ImGui::Text("Plate (obj)");
            ImGui::DragFloat3("Plate Pos", &m_plateTranslation.x, 0.01f, -10.0f, 10.0f);
            ImGui::DragFloat3("Plate Scale", &m_plateScale.x, 0.005f, 0.001f, 10.0f);
            // rotation in degrees for easier editing
            glm::vec3 plateEulerDeg = glm::degrees(glm::eulerAngles(m_plateRotation));
            float plateRotArr[3] = { plateEulerDeg.x, plateEulerDeg.y, plateEulerDeg.z };
            if (ImGui::DragFloat3("Plate Rot (deg)", plateRotArr, 1.0f, -360.0f, 360.0f))
            {
                m_plateRotation = glm::quat(glm::radians(glm::vec3(plateRotArr[0], plateRotArr[1], plateRotArr[2])));
            }
        }

        if (m_axeLoaded)
        {
            ImGui::Separator();
            ImGui::Text("Axe (obj)");
            ImGui::DragFloat3("Axe Pos", &m_axeTranslation.x, 0.01f, -10.0f, 10.0f);
            ImGui::DragFloat3("Axe Scale", &m_axeScale.x, 0.005f, 0.001f, 10.0f);
            glm::vec3 axeEulerDeg = glm::degrees(glm::eulerAngles(m_axeRotation));
            float axeRotArr[3] = { axeEulerDeg.x, axeEulerDeg.y, axeEulerDeg.z };
            if (ImGui::DragFloat3("Axe Rot (deg)", axeRotArr, 1.0f, -360.0f, 360.0f))
            {
                m_axeRotation = glm::quat(glm::radians(glm::vec3(axeRotArr[0], axeRotArr[1], axeRotArr[2])));
            }
        }

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
        0.01f, 100.0f);

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

    // Upload global light uniforms
    glUniform1i(glGetUniformLocation(m_shaderProgram, "globalLightEnabled"), m_globalLightEnabled ? 1 : 0);
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "globalLightColor"), 1, glm::value_ptr(m_globalLightColor));
    // Use camera position as global light position so ambient follows the camera
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "globalLightPos"), 1, glm::value_ptr(m_cameraPos));
    glUniform1f(glGetUniformLocation(m_shaderProgram, "globalLightIntensity"), m_globalLightIntensity);
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

bool C3DViewer::setupSkybox()
{
    if (m_skyboxVAO == 0)
    {
        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &m_skyboxVAO);
        glGenBuffers(1, &m_skyboxVBO);
        glBindVertexArray(m_skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    if (m_skyboxShaderProgram == 0)
    {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &skyboxVertexShaderSrc, nullptr);
        glCompileShader(vertexShader);
        if (!checkCompileErrors(vertexShader, "SKYBOX_VERTEX")) return false;

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &skyboxFragmentShaderSrc, nullptr);
        glCompileShader(fragmentShader);
        if (!checkCompileErrors(fragmentShader, "SKYBOX_FRAGMENT")) return false;

        m_skyboxShaderProgram = glCreateProgram();
        glAttachShader(m_skyboxShaderProgram, vertexShader);
        glAttachShader(m_skyboxShaderProgram, fragmentShader);
        glLinkProgram(m_skyboxShaderProgram);
        if (!checkCompileErrors(m_skyboxShaderProgram, "PROGRAM")) return false;

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glUseProgram(m_skyboxShaderProgram);
        glUniform1i(glGetUniformLocation(m_skyboxShaderProgram, "skybox"), 0);
        glUseProgram(0);
    }

    if (m_skyboxTexture == 0)
    {
        
        std::string basePath = "src/room/";
        

        std::array<std::string, 6> faces = {
            basePath + "room/w3.jpg",
            basePath + "room/w6.jpg",
            basePath + "roof.jpg",
            basePath + "floor.jpg",
            basePath + "chim1.jpg",
            basePath + "room/w1.jpg"
        };

        m_skyboxTexture = loadSkyboxCubemap(faces);
        if (m_skyboxTexture == 0)
            return false;
    }

    return true;
}

namespace
{
    std::vector<unsigned char> resizeRGBA(const unsigned char* src, int srcW, int srcH, int dstW, int dstH)
    {
        std::vector<unsigned char> dst;
        if (!src || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
            return dst;

        dst.resize(static_cast<size_t>(dstW) * dstH * 4);
        for (int y = 0; y < dstH; ++y)
        {
            float v = (static_cast<float>(y) * srcH) / dstH;
            int y0 = static_cast<int>(std::floor(v));
            int y1 = std::min(y0 + 1, srcH - 1);
            float fy = v - y0;
            for (int x = 0; x < dstW; ++x)
            {
                float u = (static_cast<float>(x) * srcW) / dstW;
                int x0 = static_cast<int>(std::floor(u));
                int x1 = std::min(x0 + 1, srcW - 1);
                float fx = u - x0;
                for (int c = 0; c < 4; ++c)
                {
                    float c00 = src[(y0 * srcW + x0) * 4 + c];
                    float c10 = src[(y0 * srcW + x1) * 4 + c];
                    float c01 = src[(y1 * srcW + x0) * 4 + c];
                    float c11 = src[(y1 * srcW + x1) * 4 + c];
                    float c0 = c00 * (1.0f - fx) + c10 * fx;
                    float c1 = c01 * (1.0f - fx) + c11 * fx;
                    float cval = c0 * (1.0f - fy) + c1 * fy;
                    int out = static_cast<int>(cval + 0.5f);
                    dst[(y * dstW + x) * 4 + c] = static_cast<unsigned char>(std::min(255, std::max(0, out)));
                }
            }
        }

        return dst;
    }
}

bool C3DViewer::loadImageResized(const std::string& path, int targetWidth, int targetHeight, std::vector<unsigned char>& outPixels, int& outWidth, int& outHeight)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data)
    {
        std::cerr << "Warning: No se pudo cargar imagen de skybox: " << path
            << " -> " << stbi_failure_reason() << std::endl;
        return false;
    }

    int finalSize = (targetWidth > 0 && targetHeight > 0)
        ? std::min(targetWidth, targetHeight)
        : std::min(width, height);
    if (finalSize <= 0)
    {
        stbi_image_free(data);
        return false;
    }

    if (width == finalSize && height == finalSize)
    {
        outPixels.assign(data, data + (finalSize * finalSize * 4));
    }
    else
    {
        auto resized = resizeRGBA(data, width, height, finalSize, finalSize);
        if (resized.empty())
        {
            int xoff = std::max(0, (width - finalSize) / 2);
            int yoff = std::max(0, (height - finalSize) / 2);
            outPixels.resize(static_cast<size_t>(finalSize) * finalSize * 4);
            size_t rowBytes = static_cast<size_t>(finalSize) * 4;
            for (int r = 0; r < finalSize; ++r)
            {
                const unsigned char* src = data + ((r + yoff) * width + xoff) * 4;
                unsigned char* dst = outPixels.data() + r * rowBytes;
                std::memcpy(dst, src, rowBytes);
            }
        }
        else
        {
            outPixels = std::move(resized);
        }
    }

    stbi_image_free(data);
    outWidth = finalSize;
    outHeight = finalSize;
    return true;
}

GLuint C3DViewer::loadSkyboxCubemap(const std::array<std::string, 6>& faces)
{
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    GLint previousAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Fuerza que todas las caras se reescalen a 1500x1500
    const int forcedSize = 735;
    int targetSize = forcedSize;
    bool allFacesLoaded = true;

    for (unsigned int i = 0; i < faces.size(); ++i)
    {
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
        if (!loadImageResized(faces[i], forcedSize, forcedSize, pixels, width, height))
        {
            allFacesLoaded = false;
            break;
        }

        // loadImageResized devuelve las dimensiones finales en width/height;
        // comprobamos por seguridad que coincidan con el tamaño forzado
        if (width != targetSize || height != targetSize)
        {
            std::cerr << "Warning: Skybox face size mismatch en " << faces[i]
                << " (" << width << "x" << height << ") esperado " << targetSize << "x" << targetSize << std::endl;
            allFacesLoaded = false;
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA,
            targetSize, targetSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    if (!allFacesLoaded)
    {
        glDeleteTextures(1, &textureID);
        return 0;
    }

    return textureID;
}

void C3DViewer::renderSkybox()
{
    if (m_skyboxShaderProgram == 0 || m_skyboxVAO == 0 || m_skyboxTexture == 0)
        return;

    GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
    if (wasCullingEnabled)
    {
        glDisable(GL_CULL_FACE);
    }

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    glUseProgram(m_skyboxShaderProgram);
    glm::mat4 view = glm::mat4(glm::mat3(m_viewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

    glBindVertexArray(m_skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    if (wasCullingEnabled)
    {
        glEnable(GL_CULL_FACE);
    }
}
