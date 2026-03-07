#pragma once
#include <string>
#include <fstream>
#include <iomanip>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <array>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "OBJLoader.h"
#include <vector>
#include <deque>

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
    glm::vec3 getCameraMovementDirection() const;
    void resize(int new_width, int new_height);
    bool setupShader();
    bool checkCompileErrors(GLuint shader, const char* type);
    void loadOBJFile();
    void renderOBJ();
    void placeCameraOnTable();
    bool loadSceneProps();
    void updateScenePropsPlacement();
    glm::vec3 getNormalizedMinBounds(const OBJLoader& loader, const glm::vec3& scale) const;
    glm::vec3 getNormalizedMaxBounds(const OBJLoader& loader, const glm::vec3& scale) const;

    // guardar OBJ + MTL con transformaciones aplicadas a vertices/normales
    void saveOBJFile();
    bool saveOBJWithMTL(const std::string& objPath);

    // (Normal and vertex overlay visualization removed)

    struct LightSettings
    {
        glm::vec3 ambient = glm::vec3(0.2f);
        glm::vec3 diffuse = glm::vec3(1.0f);
        glm::vec3 specular = glm::vec3(1.0f);
        float orbitRadius = 2.5f;
        float orbitHeight = 1.0f;
        float orbitSpeed = 0.5f;
        float orbitAngle = 0.0f;
        float phaseOffset = 0.0f;
        bool enabled = true;
        bool attenuationEnabled = true;
        int shadingModel = 0;
        glm::vec3 position = glm::vec3(0.0f);
    };

    static constexpr int MAX_LIGHTS = 3;
    std::array<LightSettings, MAX_LIGHTS> m_lights;
    float m_lightAnimationSpeed = 1.0f;
    double m_lastLightUpdateTime = 0.0;
    float m_lightGlobalAngle = 0.0f;

    // Global white light above scene
    bool m_globalLightEnabled = true;
    float m_globalLightIntensity = 1.0f;
    glm::vec3 m_globalLightColor = glm::vec3(1.0f);
    glm::vec3 m_globalLightPosition = glm::vec3(0.0f, 3.0f, 0.0f);

    bool setupLightVisualization();
    bool setupLightSphereMesh();
    bool setupLightShader();
    void initLights();
    void updateLightAnimation(double deltaTime);
    void uploadLightUniforms();
    void renderLightIndicators();

    bool setupSkybox();
    GLuint loadSkyboxCubemap(const std::array<std::string, 6>& faces);
    bool loadImageResized(const std::string& path, int targetWidth, int targetHeight,
        std::vector<unsigned char>& outPixels, int& outWidth, int& outHeight);
    void renderSkybox();

    // Depth test y Culling
    bool m_depthTestEnabled = true;               
    bool m_backfaceCullingEnabled = true;         
    GLenum m_cullFaceMode = GL_BACK;

    static void keyCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos);

protected:
    int width = 1280;
    int height = 720;
    GLFWwindow* m_window = nullptr;
    GLuint m_shaderProgram = 0;
    double lastTime = 0.0;
    bool mouseButtonsDown[3] = { false, false, false };

    // Mostrar FPS (promedio ultimos N segundos)
    bool m_showFPS = false;
    std::deque<double> m_frameTimestamps;   // timestamps de frames
    double m_fpsWindowSeconds = 5.0;        // ventana para promedio (segundos)
    double m_fpsAverage = 0.0;              // valor calculado del FPS

    // Antialiasing de lineas
    bool m_lineAntiAlias = false;

    // Color de fondo 
    glm::vec3 m_backgroundColor = glm::vec3(0.15f, 0.15f, 0.2f);

    // OBJ Loader
    OBJLoader m_objLoader;
    bool m_objLoaded = false;
    OBJLoader m_stoveLoader;
    OBJLoader m_howlLoader;
    OBJLoader m_jugLoader;
    bool m_stoveLoaded = false;
    bool m_howlLoaded = false;
    bool m_jugLoaded = false;

    // Camera
    glm::vec3 m_cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_cameraTarget = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::vec3 m_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    // Camera - FPS style control
    glm::vec3 m_cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Camara estilo FPS
    float m_cameraYaw = -90.0f;          // grados, rumbo inicial hacia -Z
    float m_cameraPitch = 0.0f;          // grados
    float m_cameraSpeed = 0.03f;         // distancia por pulsacion/step
    float m_mouseSensitivity = 0.1f;     // grados por pixel de raton
    bool m_mouseLookEnabled = true;      // permitir mirar libremente con el mouse
    int m_cameraMovementMode = 0;        // 0 = FPS, 1 = GOD

    // Helpers para la camara
    void computeCameraVectors();
    void updateViewMatrix();

    // Matrices
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    glm::mat4 m_modelMatrix;

    // Transformaciones del objeto
    glm::vec3 m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::vec3 m_objectScale = glm::vec3(1.0f);
    glm::quat m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 m_stoveTranslation = glm::vec3(0.0f);
    glm::vec3 m_stoveScale = glm::vec3(0.18f);
    glm::quat m_stoveRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 m_howlTranslation = glm::vec3(0.0f);
    glm::vec3 m_howlScale = glm::vec3(0.12f);
    glm::quat m_howlRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 m_jugTranslation = glm::vec3(0.0f);
    glm::vec3 m_jugScale = glm::vec3(0.092f);
    glm::quat m_jugRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // Mouse tracking
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_hasMousePosition = false;
    bool m_isDragging = false;

    // Relleno / Alambrado (simplificado: siempre renderizamos relleno)
    // Nota: las opciones de wireframe/bounding box se eliminaron.

    // Parametros para evitar z-fighting (se usan cuando se dibuja relleno)
    float m_fillPolygonOffsetFactor = 1.0f;
    float m_fillPolygonOffsetUnits = 1.0f;



    // Shaders actualizados para lighting basico
    const char* vertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoord;
        
        void main() 
        {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            TexCoord = aTexCoord;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";

    const char* fragmentShaderSrc = R"glsl(
        #version 330 core
        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoord;
        
        out vec4 FragColor;
        
        const int MAX_LIGHTS = 3;
        
        uniform vec3 viewPos;
        uniform vec3 lightPos[MAX_LIGHTS];
        uniform vec3 lightAmbient[MAX_LIGHTS];
        uniform vec3 lightDiffuse[MAX_LIGHTS];
        uniform vec3 lightSpecular[MAX_LIGHTS];
        uniform int lightEnabled[MAX_LIGHTS];
        uniform int lightShadingModel[MAX_LIGHTS];
        uniform int lightUseAttenuation[MAX_LIGHTS];
        uniform vec3 materialKa;
        uniform vec3 materialKd;
        uniform vec3 materialKs;
        uniform sampler2D texAmbient;
        uniform sampler2D texDiffuse;
        uniform sampler2D texSpecular;
        uniform int hasAmbientMap;
        uniform int hasDiffuseMap;
        uniform int hasSpecularMap;
        // Global scene light
        uniform int globalLightEnabled;
        uniform vec3 globalLightColor;
        uniform vec3 globalLightPos;
        uniform float globalLightIntensity;
        
        void main() {
            vec3 ambientColor = (hasAmbientMap == 1) ? texture(texAmbient, TexCoord).rgb : materialKa;
            vec3 diffuseColor = (hasDiffuseMap == 1) ? texture(texDiffuse, TexCoord).rgb : materialKd;
            vec3 specularColor = (hasSpecularMap == 1) ? texture(texSpecular, TexCoord).rgb : materialKs;

            vec3 norm = normalize(Normal);
            vec3 flatNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 result = vec3(0.0);

            for (int i = 0; i < MAX_LIGHTS; ++i)
            {
                if (lightEnabled[i] == 0) continue;

                vec3 lightDir = normalize(lightPos[i] - FragPos);
                vec3 usedNormal = (lightShadingModel[i] == 2) ? flatNormal : norm;
                float diff = max(dot(usedNormal, lightDir), 0.0);

                float attenuation = 1.0;
                if (lightUseAttenuation[i] == 1)
                {
                    float dist = length(lightPos[i] - FragPos);
                    float constant = 1.0;
                    float linear = 0.22;
                    float quadratic = 0.20;
                    attenuation = 1.0 / (constant + linear * dist + quadratic * dist * dist);
                }

                vec3 ambient = lightAmbient[i] * ambientColor;
                vec3 diffuse = diff * lightDiffuse[i] * diffuseColor;

                float spec = 0.0;
                if (lightShadingModel[i] == 1)
                {
                    vec3 halfway = normalize(lightDir + viewDir);
                    spec = pow(max(dot(usedNormal, halfway), 0.0), 32.0);
                }
                else
                {
                    vec3 reflectDir = reflect(-lightDir, usedNormal);
                    spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                }

                vec3 specular = spec * lightSpecular[i] * specularColor;
                result += attenuation * (ambient + diffuse + specular);
            }

            // Global white light contribution (simple Phong)
            if (globalLightEnabled == 1)
            {
                vec3 gLightDir = normalize(globalLightPos - FragPos);
                vec3 gAmbient = 0.1 * globalLightColor * ambientColor * globalLightIntensity;
                float gDiff = max(dot(norm, gLightDir), 0.0);
                vec3 gDiffuse = gDiff * globalLightColor * diffuseColor * globalLightIntensity;
                vec3 gReflectDir = reflect(-gLightDir, norm);
                float gSpec = pow(max(dot(viewDir, gReflectDir), 0.0), 32.0);
                vec3 gSpecular = gSpec * globalLightColor * specularColor * globalLightIntensity;
                result += (gAmbient + gDiffuse + gSpecular);
            }

            FragColor = vec4(result, 1.0);
        }
    )glsl";

    // Shaders para indicadores de luz
    const char* lightIndicatorVertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main()
        {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";

    const char* lightIndicatorFragmentShaderSrc = R"glsl(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 lightColor;
        void main()
        {
            FragColor = vec4(lightColor, 1.0);
        }
    )glsl";

    const char* skyboxVertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        out vec3 TexCoords;

        uniform mat4 view;
        uniform mat4 projection;

        void main()
        {
            TexCoords = aPos;
            vec4 pos = projection * view * vec4(aPos, 1.0);
            gl_Position = pos.xyww;
        }
    )glsl";

    const char* skyboxFragmentShaderSrc = R"glsl(
        #version 330 core
        in vec3 TexCoords;
        out vec4 FragColor;

        uniform samplerCube skybox;

        void main()
        {
            FragColor = texture(skybox, TexCoords);
        }
    )glsl";

    GLuint m_lightShaderProgram = 0;
    GLuint m_lightSphereVAO = 0;
    GLuint m_lightSphereVBO = 0;
    GLuint m_lightSphereEBO = 0;
    GLsizei m_lightSphereIndexCount = 0;

    GLuint m_skyboxShaderProgram = 0;
    GLuint m_skyboxVAO = 0;
    GLuint m_skyboxVBO = 0;
    GLuint m_skyboxTexture = 0;

    
    // (Normal and vertex shaders removed)
};