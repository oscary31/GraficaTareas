#include "OBJLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <limits>
#include <glad.h>
#include <unordered_map>

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

// Cargar un archivo MTL 
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

        // Define un nuevo material
        if (prefix == "newmtl") {

            if (hasMaterial) {
                m_materials[currentMaterialName] = currentMaterial;
            }
            iss >> currentMaterialName;
            currentMaterial = Material();
            currentMaterial.name = currentMaterialName;
            hasMaterial = true;
        }
        // Color difuso (Diffuse color)
        else if (prefix == "Kd") {
            iss >> currentMaterial.Kd.r >> currentMaterial.Kd.g >> currentMaterial.Kd.b;
        }
        // Color ambiental (Ambient color)
        else if (prefix == "Ka") {
            iss >> currentMaterial.Ka.r >> currentMaterial.Ka.g >> currentMaterial.Ka.b;
        }
        // Color especular (Specular color)
        else if (prefix == "Ks") {
            iss >> currentMaterial.Ks.r >> currentMaterial.Ks.g >> currentMaterial.Ks.b;
        }
        // Textura difusa
        else if (prefix == "map_Kd") {
            iss >> currentMaterial.map_Kd;
        }
    }

    // Guarda el ultimo material procesado
    if (hasMaterial) {
        m_materials[currentMaterialName] = currentMaterial;
    }

    file.close();
    return !m_materials.empty();
}

// Funcion principal que carga un archivo OBJ
bool OBJLoader::load(const std::string& objPath) {
    clear();

    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo OBJ: " << objPath << std::endl;
        return false;
    }

    // Extrae el directorio del archivo OBJ para buscar el MTL
    size_t lastSlash = objPath.find_last_of("/\\");
    std::string directory = (lastSlash != std::string::npos) ? objPath.substr(0, lastSlash + 1) : "";

    // Vectores temporales para almacenar los datos del OBJ
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec3> tempNormals;
    std::vector<glm::vec2> tempTexCoords;

    // Submesh actual que se esta construyendo
    SubMesh currentSubMesh;
    std::string currentMaterialName;
    bool hasCurrentSubMesh = false;

    // Mapa para evitar duplicar vertices identicos
    std::unordered_map<std::string, unsigned int> vertexIndexMap;
    vertexIndexMap.clear();

    // Flag para detectar si el OBJ fue exportado por esta aplicacion
    bool isBakedOBJ = false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Detecta comentario de exportacion para evitar normalizacion posterior
        if (line.find("Exportado por OBJ Viewer") != std::string::npos) {
            isBakedOBJ = true;
        }

        if (line[0] == '#') continue; // Ignora comentarios

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        // Referencia al archivo MTL
        if (prefix == "mtllib") {
            std::string mtlFile;
            iss >> mtlFile;
            loadMTL(directory + mtlFile);
        }
        // Vertice (x, y, z)
        else if (prefix == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            tempVertices.push_back(vertex);

            // Actualiza el bounding box
            m_minBounds = glm::min(m_minBounds, vertex);
            m_maxBounds = glm::max(m_maxBounds, vertex);
        }
        // Normal de vertice
        else if (prefix == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            tempNormals.push_back(normal);
        }
        // Coordenadas de textura
        else if (prefix == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            tempTexCoords.push_back(texCoord);
        }
        // Cambio de material (crea un nuevo submesh)
        else if (prefix == "usemtl") {
            // Guarda el submesh anterior si existe
            if (hasCurrentSubMesh) {
                m_subMeshes.push_back(currentSubMesh);
            }

            // Inicia un nuevo submesh
            currentSubMesh = SubMesh();
            iss >> currentMaterialName;
            currentSubMesh.materialName = currentMaterialName;

            // Busca y asigna el material correspondiente
            if (m_materials.find(currentMaterialName) != m_materials.end()) {
                currentSubMesh.material = m_materials[currentMaterialName];
                std::cout << "Material '" << currentMaterialName << "' asignado al sub-mesh" << std::endl;
            }
            else {
                currentSubMesh.material = Material();
                std::cerr << "Material '" << currentMaterialName << "' no encontrado, usando color gris por defecto" << std::endl;
            }

            hasCurrentSubMesh = true;
        }
        
        else if (prefix == "f") {
            // Si no hay submesh activo, crea uno por defecto
            if (!hasCurrentSubMesh) {
                currentSubMesh = SubMesh();
                currentSubMesh.material = Material();
                hasCurrentSubMesh = true;
            }

            // Lee todos los vertices de la cara
            std::vector<std::string> faceVertices;
            std::string vertex;
            while (iss >> vertex) {
                faceVertices.push_back(vertex);
            }

            // Triangula la cara
            // Usar el algoritmo de abanico (fan triangulation)
            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                unsigned int triangleIndices[3] = { 0, static_cast<unsigned int>(i), static_cast<unsigned int>(i + 1) };

                // Procesar cada vertice del triangulo
                for (size_t j = 0; j < 3; ++j) {
                    std::string& fv = faceVertices[triangleIndices[j]];

                    // Verifica si este vertice ya fue procesado
                    auto it = vertexIndexMap.find(fv);
                    if (it != vertexIndexMap.end()) {
                        
                        currentSubMesh.indices.push_back(it->second);
                    }
                    else {
                        // Nuevo vertice, parsea los indices v/vt/vn
                        std::istringstream faceStream(fv);
                        std::string indexStr;
                        int indices[3] = { 0, 0, 0 };
                        int count = 0;

                        while (std::getline(faceStream, indexStr, '/') && count < 3) {
                            if (!indexStr.empty()) {
                                indices[count] = std::stoi(indexStr);
                            }
                            count++;
                        }

                        int vIdx = indices[0] - 1;  // Indice de vertice (OBJ usa indices base 1)
                        int vtIdx = indices[1] - 1; // Indice de textura
                        int vnIdx = indices[2] - 1; // Indice de normal

                        // Agrega el vertice si es valido
                        if (vIdx >= 0 && vIdx < (int)tempVertices.size()) {
                            currentSubMesh.vertices.push_back(tempVertices[vIdx]);
                        }

                        // Agrega coordenada de textura o valor por defecto
                        if (vtIdx >= 0 && vtIdx < (int)tempTexCoords.size()) {
                            currentSubMesh.texCoords.push_back(tempTexCoords[vtIdx]);
                        }
                        else {
                            currentSubMesh.texCoords.push_back(glm::vec2(0.0f));
                        }

                        // Agrega normal o placeholder (se calculara despues si no existe)
                        if (vnIdx >= 0 && vnIdx < (int)tempNormals.size()) {
                            currentSubMesh.normals.push_back(tempNormals[vnIdx]);
                        }
                        else {
                            // Placeholder - calculateVertexNormals() las calculara
                            currentSubMesh.normals.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
                        }

                        // Registra el nuevo vertice en el mapa de indices
                        unsigned int newIndex = currentSubMesh.vertices.size() - 1;
                        currentSubMesh.indices.push_back(newIndex);
                        vertexIndexMap[fv] = newIndex;
                    }
                }
            }
        }
    }

    // Guarda el ultimo submesh
    if (hasCurrentSubMesh) {
        m_subMeshes.push_back(currentSubMesh);
    }

    file.close();

    // Verifica que se hayan cargado submeshes
    if (m_subMeshes.empty()) {
        std::cerr << "Error: No se encontraron sub-mallados en el archivo OBJ" << std::endl;
        return false;
    }

    // Muestra informacion de carga
    std::cout << "INFO: OBJ cargado con " << m_subMeshes.size() << " sub-mallados" << std::endl;
    for (size_t i = 0; i < m_subMeshes.size(); ++i) {
        std::cout << "  SubMesh " << i << ": "
            << m_subMeshes[i].vertices.size() << " vertices, "
            << m_subMeshes[i].indices.size() << " indices, "
            << m_subMeshes[i].normals.size() << " normals, "
            << m_subMeshes[i].texCoords.size() << " texCoords" << std::endl;
    }

    if (m_materials.empty()) {
        std::cout << "Mensaje: No se encontro archivo MTL o esta vacio. Usando color gris por defecto (0.7, 0.7, 0.7)" << std::endl;
    }

    // Si el OBJ fue exportado por la aplicacion, no recalcula normalizacion
    if (!isBakedOBJ) {
        calculateNormalization();
    }
    else {
        std::cout << "INFO: OBJ marcado como exportado por la aplicacion -> se omite calculateNormalization()\n";
        // m_center y m_scaleFactor ya estan en valores neutrales desde clear()
    }

    // Calcula/ajusta normales si es necesario y prepara buffers
    calculateVertexNormals();
    setupBuffers();

    return true;
}

// Calcula el centro y factor de escala para normalizar el modelo
void OBJLoader::calculateNormalization() {
    // Centro del bounding box
    m_center = (m_minBounds + m_maxBounds) * 0.5f;

    // Dimensiones del bounding box
    glm::vec3 size = m_maxBounds - m_minBounds;

    // Encuentra la dimension mas grande
    float maxSize = std::max({ size.x, size.y, size.z });

    // Factor de escala para normalizar a rango [-0.5, 0.5]
    m_scaleFactor = glm::vec3(1.0f / maxSize);
}

// Configura los buffers de OpenGL (VAO, VBO, EBO) para cada submesh
void OBJLoader::setupBuffers() {
    for (auto& subMesh : m_subMeshes) {

        // Verifica consistencia de datos (mismo numero de vertices, normales y texcoords)
        if (subMesh.normals.size() != subMesh.vertices.size()) {
            std::cerr << "Warning: Mismatch entre vertices y normales, ajustando..." << std::endl;
            subMesh.normals.resize(subMesh.vertices.size(), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        if (subMesh.texCoords.size() != subMesh.vertices.size()) {
            subMesh.texCoords.resize(subMesh.vertices.size(), glm::vec2(0.0f));
        }

        // Crea y vincula el VAO (Vertex Array Object)
        glGenVertexArrays(1, &subMesh.VAO);
        glBindVertexArray(subMesh.VAO);

        // VBO para vertices (location = 0)
        glGenBuffers(1, &subMesh.VBO_vertices);
        glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_vertices);
        glBufferData(GL_ARRAY_BUFFER, subMesh.vertices.size() * sizeof(glm::vec3),
            subMesh.vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // VBO para normales (location = 1)
        if (!subMesh.normals.empty()) {
            glGenBuffers(1, &subMesh.VBO_normals);
            glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_normals);
            glBufferData(GL_ARRAY_BUFFER, subMesh.normals.size() * sizeof(glm::vec3),
                subMesh.normals.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(1);
        }

        // VBO para coordenadas de textura (location = 2)
        if (!subMesh.texCoords.empty()) {
            glGenBuffers(1, &subMesh.VBO_texCoords);
            glBindBuffer(GL_ARRAY_BUFFER, subMesh.VBO_texCoords);
            glBufferData(GL_ARRAY_BUFFER, subMesh.texCoords.size() * sizeof(glm::vec2),
                subMesh.texCoords.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glEnableVertexAttribArray(2);
        }

        // EBO para indices (Element Buffer Object)
        glGenBuffers(1, &subMesh.EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, subMesh.indices.size() * sizeof(unsigned int),
            subMesh.indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0); // Desvincular VAO
    }
}

// Libera los buffers de OpenGL
void OBJLoader::cleanupBuffers() {
    for (auto& subMesh : m_subMeshes) {
        if (subMesh.VAO) glDeleteVertexArrays(1, &subMesh.VAO);
        if (subMesh.VBO_vertices) glDeleteBuffers(1, &subMesh.VBO_vertices);
        if (subMesh.VBO_normals) glDeleteBuffers(1, &subMesh.VBO_normals);
        if (subMesh.VBO_texCoords) glDeleteBuffers(1, &subMesh.VBO_texCoords);
        if (subMesh.EBO) glDeleteBuffers(1, &subMesh.EBO);
    }
}

// Calcula normales de vertices a partir de las normales de las caras
void OBJLoader::calculateVertexNormals() {
    for (auto& subMesh : m_subMeshes) {
        // Verifica si ya tiene normales validas
        bool hasNormals = !subMesh.normals.empty();
        for (const auto& n : subMesh.normals) {
            if (glm::length(n) < 0.001f) {
                hasNormals = false;
                break;
            }
        }

        // Si ya tiene normales validas, omite el calculo
        if (hasNormals) continue;

        std::cout << "Calculando normales para un sub-mesh..." << std::endl;

        // Reinicia el vector de normales
        subMesh.normals.clear();
        subMesh.normals.resize(subMesh.vertices.size(), glm::vec3(0.0f));

        // Recorre todos los triangulos (grupos de 3 indices)
        for (size_t i = 0; i < subMesh.indices.size(); i += 3) {
            unsigned int i0 = subMesh.indices[i];
            unsigned int i1 = subMesh.indices[i + 1];
            unsigned int i2 = subMesh.indices[i + 2];

            // Verifica que los indices sean validos
            if (i0 >= subMesh.vertices.size() ||
                i1 >= subMesh.vertices.size() ||
                i2 >= subMesh.vertices.size()) {
                continue;
            }

            // Obtiene los tres vertices del triangulo
            glm::vec3 v0 = subMesh.vertices[i0];
            glm::vec3 v1 = subMesh.vertices[i1];
            glm::vec3 v2 = subMesh.vertices[i2];

            // Calcula la normal del triangulo usando producto cruz
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            // Acumula la normal en cada vertice del triangulo
            // Esto genera un promedio ponderado por area
            subMesh.normals[i0] += faceNormal;
            subMesh.normals[i1] += faceNormal;
            subMesh.normals[i2] += faceNormal;

            std::cout << "Face normal: (" << faceNormal.x << ", " << faceNormal.y << ", " << faceNormal.z << ")" << std::endl;
        }

        // Normaliza todas las normales acumuladas
        for (auto& normal : subMesh.normals) {
            if (glm::length(normal) > 0.001f) {
                normal = glm::normalize(normal);
            }
            else {
                // Si la normal es cero, usa un valor por defecto (apuntando hacia arriba)
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        std::cout << "Normales calculadas para un sub-mesh: " << subMesh.normals.size() << " normales" << std::endl;
    }
}