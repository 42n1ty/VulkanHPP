#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "vk_mesh.hpp"
#include "vk_texture.hpp"
#include "vk_material.hpp"

#include <map>


namespace V {
  
  class VulkanSwapchain;
  
  struct BoneInfo {
    glm::mat4 boneOffset;
    glm::mat4 finalTransform;
  };
  
  class VulkanModel {
  public:
  
    VulkanModel(
      bool needFlip,
      vk::raii::PhysicalDevice& pDev,
      vk::raii::Device& lDev,
      VulkanSwapchain& sc,
      vk::raii::CommandPool& cmdPool,
      vk::raii::Queue& graphQ,
      vk::raii::DescriptorSetLayout& perFrameL,
      vk::raii::DescriptorSetLayout& perMatL,
      vk::raii::DescriptorPool& descPool
    );
    
    bool load(const std::string& path);
    
    void draw(vk::raii::CommandBuffer& cmdBuf);
    
    vk::raii::PipelineLayout& getPipLayout();
    bool isLoaded() const { return m_isLoaded; };
    
    const glm::mat4& getNormMatrix() const { return m_normMatrix; };
    const std::vector<glm::mat4> getBoneTransforms() { return m_finalBoneMatrices; };
    void setBaseRotation(float angleDegrees, const glm::vec3& axis);
    
    bool hasAnims() const { return m_pScene && m_pScene->HasAnimations(); }
    void updAnim(float dT);
    void setAnim(uint32_t animIndex);
     
    bool flipVertically = true;
    
  private:
    bool processNode(aiNode * node, const aiScene * scene);
    bool processMesh(aiMesh * mesh, const aiScene * scene);
    
    std::shared_ptr<VulkanTexture> loadTexture(aiMaterial* mat, aiTextureType type);
    
    void calculateNormalization();
    
    void loadBones(const aiMesh* pMesh, std::vector<Vertex>& vertices);
    const aiNodeAnim* findNodeAnim(const aiAnimation* pAnim, const std::string& nodeName);
    void readNodeHierarchy(float animTime, const aiNode* pNode, const glm::mat4& parentTransform);
    
    uint32_t findScaling(float animTime, const aiNodeAnim* pNodeAnim);
    uint32_t findRotation(float animTime, const aiNodeAnim* pNodeAnim);
    uint32_t findPosition(float animTime, const aiNodeAnim* pNodeAnim);
    void calcInterpolatedScaling(glm::vec3& out, float animTime, const aiNodeAnim* pNodeAnim);
    void calcInterpolatedRotation(glm::quat& out, float animTime, const aiNodeAnim* pNodeAnim);
    void calcInterpolatedPosition(glm::vec3& out, float animTime, const aiNodeAnim* pNodeAnim);
    
    //====================================================================================================
    
    vk::raii::PhysicalDevice* m_pDev{nullptr};
    vk::raii::Device* m_lDev{nullptr};
    VulkanSwapchain* m_sc{nullptr};
    vk::raii::CommandPool* m_cmdPool{nullptr};
    vk::raii::Queue* m_graphQ{nullptr};
    vk::raii::DescriptorSetLayout* m_perFrameDescSetLayout;
    vk::raii::DescriptorSetLayout* m_perMatDescSetLayout;
    vk::raii::DescriptorPool* m_descPool;
    
    std::vector<std::unique_ptr<VulkanMesh>> m_meshes;
    std::vector<std::shared_ptr<VulkanTexture>> m_texLoaded;
    std::vector<std::unique_ptr<VulkanMaterial>> m_materials;
    std::vector<uint32_t> m_meshToMat;
    std::string m_dir;
    glm::mat4 m_normMatrix;
    glm::mat4 m_baseTransform;
    glm::vec3 m_minCoords;
    glm::vec3 m_maxCoords;
    bool m_isLoaded = false;
    
    // anim
    std::unique_ptr<Assimp::Importer> m_pImporter;
    const aiScene* m_pScene = nullptr;
    std::map<std::string, uint32_t> m_boneMapping;
    uint32_t m_numBones{0};
    std::vector<BoneInfo> m_boneInfo;
    std::vector<glm::mat4> m_finalBoneMatrices;
    glm::mat4 m_globInverseTransform;
    float m_animTime = 0.f;
    uint32_t m_curAnim = 0;
    
  };
  
}; //V