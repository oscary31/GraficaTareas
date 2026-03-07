#pragma once

#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct Material {
    std::string name;
    glm::vec3 Kd;  
    glm::vec3 Ka;  
    glm::vec3 Ks;  
    std::string map_Ka;
    std::string map_Kd;  
    std::string map_Ks;

    unsigned int ambientTexture;
    unsigned int diffuseTexture;
    unsigned int specularTexture;

    Material()
        : Kd(0.7f, 0.7f, 0.7f), Ka(0.0f), Ks(0.0f),
        ambientTexture(0), diffuseTexture(0), specularTexture(0) {}
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

    // Transformacion por submesh
    glm::vec3 translation;

    SubMesh() : VAO(0), VBO_vertices(0), VBO_normals(0), VBO_texCoords(0), EBO(0),
        translation(0.0f) {
    }
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
    glm::vec3 getMinBounds() const { return m_minBounds; }
    glm::vec3 getMaxBounds() const { return m_maxBounds; }

    void setupBuffers();
    void cleanupBuffers();
    void calculateVertexNormals();

private:
    bool loadMTL(const std::string& mtlPath);
    void calculateNormalization();
    unsigned int loadTexture2D(const std::string& texturePath);
    void loadMaterialTextures(Material& material, const std::string& mtlDirectory);
    void applyTextureFallbacks(Material& material);
    void cleanupTextures();

    std::vector<SubMesh> m_subMeshes;
    std::map<std::string, Material> m_materials;
    std::unordered_map<std::string, unsigned int> m_textureCache;

    glm::vec3 m_center;
    glm::vec3 m_scaleFactor;
    glm::vec3 m_minBounds;
    glm::vec3 m_maxBounds;
};