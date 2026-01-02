#include "OBJLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <limits>
#include <glad.h>

OBJLoader::OBJLoader()
    : m_center(0.0f), m_scaleFactor(1.0f),
    m_minBounds(std::numeric_limits<float>::max()),
    m_maxBounds(std::numeric_limits<float>::lowest())
{
}

OBJLoader::~OBJLoader() {
    cleanupBuffers();
}

void OBJLoader::clear() {
    cleanupBuffers();
    m_subMeshes.clear();
    m_materials.clear();
    m_center = glm::vec3(0.0f);
    m_scaleFactor = glm::vec3(1.0f);
    m_minBounds = glm::vec3(std::numeric_limits<float>::max());
    m_maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
}

bool OBJLoader::loadMTL(const std::string& mtlPath) {
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        std::cerr << "Warning: No se pudo abrir el archivo MTL: " << mtlPath << std::endl;
        return false;
    }

    Material currentMaterial;
    std::string currentMaterialName;
    bool hasMaterial = false;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "newmtl") {
            if (hasMaterial) {
                m_materials[currentMaterialName] = currentMaterial;
            }
            iss >> currentMaterialName;
            currentMaterial = Material();
            currentMaterial.name = currentMaterialName;
            hasMaterial = true;
        }
        else if (prefix == "Kd") {
            iss >> currentMaterial.Kd.r >> currentMaterial.Kd.g >> currentMaterial.Kd.b;
        }
        else if (prefix == "Ka") {
            iss >> currentMaterial.Ka.r >> currentMaterial.Ka.g >> currentMaterial.Ka.b;
        }
        else if (prefix == "Ks") {
            iss >> currentMaterial.Ks.r >> currentMaterial.Ks.g >> currentMaterial.Ks.b;
        }
        else if (prefix == "map_Kd") {
            iss >> currentMaterial.map_Kd;
        }
    }

    if (hasMaterial) {
        m_materials[currentMaterialName] = currentMaterial;
    }

    file.close();
    return !m_materials.empty();
}

bool OBJLoader::load(const std::string& objPath) {
    clear();

    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::cout << "ERROR" << std::endl;
        std::cerr << "Error: No se pudo abrir el archivo OBJ: " << objPath << std::endl;
        return false;
    }
    std::cout << "NO ERROR" << std::endl;
    // Intentar cargar MTL
    size_t lastSlash = objPath.find_last_of("/\\");
    std::string directory = (lastSlash != std::string::npos) ? objPath.substr(0, lastSlash + 1) : "";

    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec3> tempNormals;
    std::vector<glm::vec2> tempTexCoords;

    SubMesh currentSubMesh;
    std::string currentMaterialName;
    bool hasCurrentSubMesh = false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "mtllib") {
            std::string mtlFile;
            iss >> mtlFile;
            loadMTL(directory + mtlFile);
        }
        else if (prefix == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            tempVertices.push_back(vertex);

            // Actualizar bounds
            m_minBounds = glm::min(m_minBounds, vertex);
            m_maxBounds = glm::max(m_maxBounds, vertex);
        }
        else if (prefix == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            tempNormals.push_back(normal);
        }
        else if (prefix == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            tempTexCoords.push_back(texCoord);
        }
        else if (prefix == "usemtl") {
            if (hasCurrentSubMesh) {
                m_subMeshes.push_back(currentSubMesh);
            }

            currentSubMesh = SubMesh();
            iss >> currentMaterialName;
            currentSubMesh.materialName = currentMaterialName;

            if (m_materials.find(currentMaterialName) != m_materials.end()) {
                currentSubMesh.material = m_materials[currentMaterialName];
            }
            else {
                currentSubMesh.material = Material();
                std::cerr << "Warning: Material '" << currentMaterialName << "' no encontrado, usando color gris por defecto" << std::endl;
            }

            hasCurrentSubMesh = true;
        }
        else if (prefix == "f") {
            if (!hasCurrentSubMesh) {
                currentSubMesh = SubMesh();
                currentSubMesh.material = Material();
                hasCurrentSubMesh = true;
            }

            std::vector<std::string> faceVertices;
            std::string vertex;
            while (iss >> vertex) {
                faceVertices.push_back(vertex);
            }

            // Triangular la cara (soporta triángulos y quads)
            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    size_t idx = (j == 0) ? 0 : i + j - 1;
                    std::string& fv = faceVertices[idx];

                    std::istringstream faceStream(fv);
                    std::string indexStr;
                    int indices[3] = { 0, 0, 0 }; // v, vt, vn
                    int count = 0;

                    while (std::getline(faceStream, indexStr, '/') && count < 3) {
                        if (!indexStr.empty()) {
                            indices[count] = std::stoi(indexStr);
                        }
                        count++;
                    }

                    int vIdx = indices[0] - 1;
                    int vtIdx = indices[1] - 1;
                    int vnIdx = indices[2] - 1;

                    if (vIdx >= 0 && vIdx < (int)tempVertices.size()) {
                        currentSubMesh.vertices.push_back(tempVertices[vIdx]);
                        currentSubMesh.indices.push_back(currentSubMesh.vertices.size() - 1);
                    }

                    if (vtIdx >= 0 && vtIdx < (int)tempTexCoords.size()) {
                        currentSubMesh.texCoords.push_back(tempTexCoords[vtIdx]);
                    }

                    if (vnIdx >= 0 && vnIdx < (int)tempNormals.size()) {
                        currentSubMesh.normals.push_back(tempNormals[vnIdx]);
                    }
                    else if (!currentSubMesh.vertices.empty()) {
                        currentSubMesh.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                }
            }
        }
    }

    if (hasCurrentSubMesh) {
        m_subMeshes.push_back(currentSubMesh);
    }

    file.close();

    if (m_subMeshes.empty()) {
        std::cerr << "Error: No se encontraron sub-mallados en el archivo OBJ" << std::endl;
        return false;
    }

    if (m_materials.empty()) {
        std::cout << "Mensaje: No se encontró archivo MTL o está vacío. Usando color gris por defecto (0.7, 0.7, 0.7)" << std::endl;
    }

    calculateNormalization();
    setupBuffers();

    return true;
}

void OBJLoader::calculateNormalization() {
    m_center = (m_minBounds + m_maxBounds) * 0.5f;
    glm::vec3 size = m_maxBounds - m_minBounds;
    float maxSize = std::max({ size.x, size.y, size.z });
    m_scaleFactor = glm::vec3(1.0f / maxSize);
}

void OBJLoader::setupBuffers() {
    for (auto& subMesh : m_subMeshes) {
        glGenVertexArrays(1, &subMesh.VAO);
        glBindVertexArray(subMesh.VAO);

        // Vertices
        glGenBuffers(1, &subMesh.VBO_vertices);
        glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_vertices);
        glBufferData(GL_ARRAY_BUFFER, subMesh.vertices.size() * sizeof(glm::vec3),
            subMesh.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // Normals
        if (!subMesh.normals.empty()) {
            glGenBuffers(1, &subMesh.VBO_normals);
            glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_normals);
            glBufferData(GL_ARRAY_BUFFER, subMesh.normals.size() * sizeof(glm::vec3),
                subMesh.normals.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(1);
        }

        // TexCoords
        if (!subMesh.texCoords.empty()) {
            glGenBuffers(1, &subMesh.VBO_texCoords);
            glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_texCoords);
            glBufferData(GL_ARRAY_BUFFER, subMesh.texCoords.size() * sizeof(glm::vec2),
                subMesh.texCoords.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glEnableVertexAttribArray(2);
        }

        // Indices
        glGenBuffers(1, &subMesh.EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, subMesh.indices.size() * sizeof(unsigned int),
            subMesh.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }
}

void OBJLoader::cleanupBuffers() {
    for (auto& subMesh : m_subMeshes) {
        if (subMesh.VAO) glDeleteVertexArrays(1, &subMesh.VAO);
        if (subMesh.VBO_vertices) glDeleteBuffers(1, &subMesh.VBO_vertices);
        if (subMesh.VBO_normals) glDeleteBuffers(1, &subMesh.VBO_normals);
        if (subMesh.VBO_texCoords) glDeleteBuffers(1, &subMesh.VBO_texCoords);
        if (subMesh.EBO) glDeleteBuffers(1, &subMesh.EBO);
    }
}