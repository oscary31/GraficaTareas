#pragma once

#include <vector>
#include <map>
#include <string>
#include <glm.hpp>

struct Material {
    std::string name;
    glm::vec3 Kd;  // Diffuse color
    glm::vec3 Ka;  // Ambient color
    glm::vec3 Ks;  // Specular color
    std::string map_Kd;  // Diffuse texture (ignorado en este proyecto)

    Material() : Kd(0.7f, 0.7f, 0.7f), Ka(0.0f), Ks(0.0f) {}
};

struct SubMesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;
    std::string materialName;
    Material material;

    // OpenGL buffers
    unsigned int VAO;
    unsigned int VBO_vertices;
    unsigned int VBO_normals;
    unsigned int VBO_texCoords;
    unsigned int EBO;

    SubMesh() : VAO(0), VBO_vertices(0), VBO_normals(0), VBO_texCoords(0), EBO(0) {}
};

class OBJLoader {
public:
    OBJLoader();
    ~OBJLoader();

    bool load(const std::string& objPath);
    void clear();

    const std::vector<SubMesh>& getSubMeshes() const { return m_subMeshes; }
    glm::vec3 getCenter() const { return m_center; }
    glm::vec3 getScaleFactor() const { return m_scaleFactor; }

    void setupBuffers();
    void cleanupBuffers();

private:
    bool loadMTL(const std::string& mtlPath);
    void calculateNormalization();

    std::vector<SubMesh> m_subMeshes;
    std::map<std::string, Material> m_materials;

    glm::vec3 m_center;
    glm::vec3 m_scaleFactor;
    glm::vec3 m_minBounds;
    glm::vec3 m_maxBounds;
};