#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
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
    void resize(int new_width, int new_height);
    bool setupShader();
    bool checkCompileErrors(GLuint shader, const char* type);
    void loadOBJFile();
    void renderOBJ();

    // Visualización de normales
    bool m_showNormals = false;
    bool m_showNormalsPerVertex = true; // nuevo: controlar ver/ocultar normales por vértice
    float m_normalLengthPercent = 0.05f; // porcentaje (0..1) de la diagonal world del bounding box
    glm::vec3 m_normalColor = glm::vec3(0.0f, 1.0f, 1.0f); // color editable
    GLuint m_normalVAO = 0;
    GLuint m_normalVBO = 0;
    GLuint m_normalShaderProgram = 0;
    std::vector<glm::vec3> m_normalLines;

    // para dibujar por sub-mesh (offsets/counts)
    std::vector<int> m_normalOffsets;
    std::vector<int> m_normalCounts;

    bool setupNormalShader();
    void generateNormalLines();
    void renderNormals();

    // Visualización de vértices
    bool m_showVertices = false;
    float m_vertexSize = 5.0f;
    glm::vec3 m_vertexColor = glm::vec3(1.0f, 0.0f, 0.0f); // Rojo por defecto
    GLuint m_vertexVAO = 0;
    GLuint m_vertexVBO = 0;
    GLuint m_vertexShaderProgram = 0;
    std::vector<glm::vec3> m_vertexPoints;

    // Ranges por sub-mesh para dibujar partes del buffer combinado
    std::vector<int> m_vertexOffsets;
    std::vector<int> m_vertexCounts;

    bool setupVertexShader();
    void generateVertexPoints();
    void renderVertices();

    // Depth test y Culling
    bool m_depthTestEnabled = true;               // habilita/deshabilita Z-buffer (GL_DEPTH_TEST)
    bool m_backfaceCullingEnabled = true;         // habilita/deshabilita back-face culling (GL_CULL_FACE)
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

    // Mostrar FPS (promedio últimos N segundos)
    bool m_showFPS = false;
    std::deque<double> m_frameTimestamps;   // timestamps de frames
    double m_fpsWindowSeconds = 5.0;        // ventana para promedio (segundos)
    double m_fpsAverage = 0.0;              // valor calculado del FPS

    // Antialiasing de líneas
    bool m_lineAntiAlias = false;

    // Color de fondo (editable)
    glm::vec3 m_backgroundColor = glm::vec3(0.15f, 0.15f, 0.2f);

    // OBJ Loader
    OBJLoader m_objLoader;
    bool m_objLoaded = false;
    int m_selectedSubMesh = -1;

    // Camera
    glm::vec3 m_cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_cameraTarget = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::vec3 m_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Matrices
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    glm::mat4 m_modelMatrix;

    // Transformaciones del objeto
    glm::vec3 m_objectTranslation = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::vec3 m_objectScale = glm::vec3(1.0f);
    glm::quat m_objectRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // Mouse tracking
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_isDragging = false;

    // Picking
    GLuint m_pickingFBO = 0;
    GLuint m_pickingTexture = 0;
    GLuint m_pickingDepthBuffer = 0;
    GLuint m_pickingShaderProgram = 0;

    void setupPickingFramebuffer();
    void renderForPicking();
    int performPicking(int mouseX, int mouseY);
    bool setupPickingShader();
    void assignPickingColors();

    // Bounding Box
    GLuint m_boundingBoxVAO = 0;
    GLuint m_boundingBoxVBO = 0;
    GLuint m_boundingBoxEBO = 0;
    GLuint m_boundingBoxShaderProgram = 0;
    glm::vec3 m_boundingBoxColor = glm::vec3(1.0f, 1.0f, 0.0f); // Amarillo por defecto

    // Relleno / Alambrado
    bool m_showFill = true;                     // mostrar relleno de triángulos
    bool m_showWireframe = false;               // mostrar alambrado
    glm::vec3 m_wireframeColor = glm::vec3(0.0f, 0.0f, 0.0f); // color del alambrado (negro por defecto)
    float m_wireframeLineWidth = 1.0f;          // grosor del alambrado

    // Parámetros para evitar z-fighting (se usan cuando se dibuja relleno)
    float m_fillPolygonOffsetFactor = 1.0f;
    float m_fillPolygonOffsetUnits = 1.0f;

    void setupBoundingBox();
    void renderBoundingBox(const SubMesh& subMesh, const glm::mat4& baseModel);
    bool setupBoundingBoxShader();
    void calculateSubMeshBounds(const SubMesh& subMesh, glm::vec3& minBounds, glm::vec3& maxBounds);

    // Shaders actualizados para lighting básico
    const char* vertexShaderSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        out vec3 FragPos;
        out vec3 Normal;
        
        void main() 
        {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";

    const char* fragmentShaderSrc = R"glsl(
        #version 330 core
        in vec3 FragPos;
        in vec3 Normal;
        
        out vec4 FragColor;
        
        uniform vec3 objectColor;
        uniform vec3 lightPos;
        uniform vec3 viewPos;
        uniform vec3 lightColor;
        
        void main() {
            // Ambient
            float ambientStrength = 0.3;
            vec3 ambient = ambientStrength * lightColor;
            
            // Diffuse
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;
            
            // Specular
            float specularStrength = 0.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
            vec3 specular = specularStrength * spec * lightColor;
            
            vec3 result = (ambient + diffuse + specular) * objectColor;
            FragColor = vec4(result, 1.0);
        }
    )glsl";

    // Shader para picking
    const char* pickingVertexShaderSrc = R"glsl(
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

    const char* pickingFragmentShaderSrc = R"glsl(
        #version 330 core
        out vec4 FragColor;
        
        uniform vec3 pickingColor;
        
        void main() {
            FragColor = vec4(pickingColor, 1.0);
        }
    )glsl";

    // Shader para Bounding Box
    const char* boundingBoxVertexShaderSrc = R"glsl(
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

    const char* boundingBoxFragmentShaderSrc = R"glsl(
        #version 330 core
        out vec4 FragColor;
        
        uniform vec3 boxColor;
        
        void main() {
            FragColor = vec4(boxColor, 1.0);
        }
    )glsl";

    // Shader para normales (agregar junto a los otros shaders)
    const char* normalVertexShaderSrc = R"glsl(
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

    const char* normalFragmentShaderSrc = R"glsl(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 normalColor;
    
    void main() {
        FragColor = vec4(normalColor, 1.0);
    }
)glsl";

    // Shader para vértices
    const char* vertexPointVertexShaderSrc = R"glsl(
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

    const char* vertexPointFragmentShaderSrc = R"glsl(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 vertexColor;
    
    void main() {
        // gl_PointCoord va de (0,0) a (1,1) sobre el punto rasterizado
        vec2 coord = gl_PointCoord - vec2(0.5);
        float dist = length(coord);
        // Smooth edge: alpha decrece cerca del borde para antialiasing
        float alpha = 1.0 - smoothstep(0.48, 0.5, dist);
        if (dist > 0.5) discard; // fuera del círculo
        FragColor = vec4(vertexColor, alpha);
    }
)glsl";
};