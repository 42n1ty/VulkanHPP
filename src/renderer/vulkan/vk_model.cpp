#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <limits>
#include <filesystem>

#include "vk_model.hpp"
#include "../../tools/assimp_glm_helpers.hpp"

namespace V {
  
  VulkanModel::VulkanModel(
    bool needFlip,
    vk::raii::PhysicalDevice& pDev,
    vk::raii::Device& lDev,
    VulkanSwapchain& sc,
    vk::raii::CommandPool& cmdPool,
    vk::raii::Queue& graphQ,
    vk::raii::DescriptorSetLayout& perFrameL,
    vk::raii::DescriptorSetLayout& perMatL,
    vk::raii::DescriptorPool& descPool
  ) {
    
    flipVertically = needFlip;
    
    m_pDev = &pDev;
    m_lDev = &lDev;
    m_sc = &sc;
    m_cmdPool = &cmdPool;
    m_graphQ = &graphQ;
    m_perFrameDescSetLayout = &perFrameL;
    m_perMatDescSetLayout = &perMatL;
    m_descPool = &descPool;
    
    m_normMatrix = glm::mat4(1.0f);
    m_baseTransform = glm::mat4(1.0f);
    m_pImporter = std::make_unique<Assimp::Importer>();
  }
  
  bool VulkanModel::load(const std::string& path) {
    m_pScene = m_pImporter->ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!m_pScene || m_pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_pScene->mRootNode) {
      Logger::error("ASSIMP: {}", m_pImporter->GetErrorString());
      m_isLoaded = false;
      return false;
    }
    m_dir = path.substr(0, path.find_last_of('/'));
    m_globInverseTransform = glm::inverse(AssimpToGlmMat4(m_pScene->mRootNode->mTransformation));
    
    m_minCoords = glm::vec3(std::numeric_limits<float>::max());
    m_maxCoords = glm::vec3(std::numeric_limits<float>::lowest());
    
    m_meshes.clear();
    m_texLoaded.clear();
    if(!processNode(m_pScene->mRootNode, m_pScene)) {
      m_isLoaded = false;
      return false;
    }
    
    if (m_meshes.empty()) {
      Logger::error("No meshes found in model: {}", path);
      m_isLoaded = false;
      return false;
    }
    
    calculateNormalization();
    m_isLoaded = true;
    
    Logger::info("Model loaded from: {}", path);
    return true;
  }
  
  void VulkanModel::draw(vk::raii::CommandBuffer& cmdBuf) {
     for (size_t i = 0; i < m_meshes.size(); ++i) {
      
      const auto& mesh = m_meshes[i];
      uint32_t matIdx = m_meshToMat[i];
      const auto& material = m_materials[matIdx];
      
      material->bind(cmdBuf);

      mesh->bind(cmdBuf);

      cmdBuf.drawIndexed(mesh->getIndexCount(), 1, 0, 0, 0);
    }
  }
  
  vk::raii::PipelineLayout& VulkanModel::getPipLayout() {
    if(m_materials.empty()) {
      Logger::error("Model has no materials, cannot get pipeline layout");
    }
    
    return m_materials[0]->getPipLayout();
  }
  
  void VulkanModel::setBaseRotation(float angleDegrees, const glm::vec3& axis) {
    m_baseTransform = glm::rotate(glm::mat4(1.f), glm::radians(angleDegrees), axis);
  }

  bool VulkanModel::processNode(aiNode * node, const aiScene * scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
      aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
      if(!(processMesh(mesh, scene))) {
        return false;
      }
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
      if(!processNode(node->mChildren[i], scene)) return false;
    }
    
    return true;
  }

  bool VulkanModel::processMesh(aiMesh * mesh, const aiScene * scene) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    //vertices==================================================
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
      Vertex vertex;
      vertex.pos = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
      
      if (mesh->HasNormals()) {
        vertex.clr = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
      }
      
      if (mesh->mTextureCoords[0]) {
        vertex.texCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
      } else {
        vertex.texCoord = glm::vec2(0.0f, 0.0f);
      }
      vertices.emplace_back(vertex);
    }
    //vertices==================================================
    
    loadBones(mesh, vertices);
    
    //indices==================================================
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for (unsigned int j = 0; j < face.mNumIndices; j++) {
        indices.emplace_back(face.mIndices[j]);
      }
    }
    //indices==================================================
    
    //materials==================================================
    uint32_t materialIndex = 0;
    if (mesh->mMaterialIndex >= 0) {
      aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

      // TODO: materials cache

      auto texture = loadTexture(assimpMaterial, aiTextureType_DIFFUSE);
      if (!texture) {
        Logger::warn("Mesh {} has no diffuse texture, skipping material creation for now.", mesh->mName.C_Str());
      }

      VulkanPplConfig materialConfig{};
      materialConfig.shaderPath = "../../assets/shaders/shader.spv";

      auto newMaterial = std::make_unique<VulkanMaterial>();
      vk::Format depthFormat;
      findDepthFormat(depthFormat, *m_pDev);
      
      if (!newMaterial->init(
        materialConfig, texture,
        *m_lDev, *m_sc,
        *m_perFrameDescSetLayout, *m_perMatDescSetLayout,
        *m_descPool, depthFormat
      )) {
        Logger::error("Failed to create material for mesh {}", mesh->mName.C_Str());
        return false;
      }

      m_materials.push_back(std::move(newMaterial));
      materialIndex = m_materials.size() - 1;
    }
    
    m_meshToMat.push_back(materialIndex);
    //materials==================================================
    
    for (const auto& vertex : vertices) {
      m_minCoords = glm::min(m_minCoords, vertex.pos);
      m_maxCoords = glm::max(m_maxCoords, vertex.pos);
    }
    
    //mesh==================================================
    auto vkMesh = std::make_unique<VulkanMesh>();
    if(!vkMesh->init(vertices, indices, *m_pDev, *m_lDev, *m_cmdPool, *m_graphQ)) {
      Logger::error("Failed to init vulkan mesh");
      return false;
    }
      m_meshes.emplace_back(std::move(vkMesh));
    //mesh==================================================
    
    // if (!textures.empty()) {
    //   Logger::debug(fmt::format("Mesh '{}' has textures:", mesh->mName.C_Str()));
    //   for (const auto& tex : textures) {
    //     Logger::debug(fmt::format("  -> Type: '{}', Path: '{}', ID: {}", tex.s_type, tex.s_path, tex.s_id));
    //   }
    // } else {
    //   Logger::warn(fmt::format("Mesh '{}' has material, but NO textures were loaded for it.", mesh->mName.C_Str()));
    // }
    
    Logger::info("Processed mesh: {}\t- Vertices: {}, Indices: {}"/*, Textures: {}"*/, mesh->mName.C_Str(), vertices.size(), indices.size()/*, m_texLoaded.size()*/);
    
    return true;
  }

  std::shared_ptr<VulkanTexture> VulkanModel::loadTexture(aiMaterial* mat, aiTextureType type) {
    
    if(mat->GetTextureCount(type) > 0) {
      aiString str;
      mat->GetTexture(type, 0, &str);
      bool skip = false;
      for(uint32_t j = 0; j < m_texLoaded.size(); ++j) {
        if(std::strcmp(m_texLoaded[j]->s_path.data(), str.C_Str()) == 0) {
          m_texLoaded.emplace_back(m_texLoaded[j]);
          skip = true;
          break;
        }
      }
      
      if(!skip) {
        std::filesystem::path texturePath(str.C_Str());
        std::string fName = texturePath.filename().string();
        std::filesystem::path finalPath = std::filesystem::path(m_dir) / ".." / "textures" / fName;
        if (!std::filesystem::exists(finalPath)) {
          Logger::warn("Texture not found at default path: {}. Trying alongside fbx...", finalPath.string());
          finalPath = std::filesystem::path(m_dir) / fName;
        }
        std::string path = finalPath.string();

        Logger::info("Attempting to load texture from: {}", path.data());
        auto newTex = std::make_shared<VulkanTexture>();
        if(newTex->init(path, *m_pDev, *m_lDev, *m_cmdPool, *m_graphQ)) {
          return newTex;
        }
      }
    }
    
    Logger::error("Failed to create vulkan texture");
    return nullptr;
  }
  
  void VulkanModel::calculateNormalization() {
    glm::vec3 center = (m_minCoords + m_maxCoords) * 0.5f;
    glm::vec3 size = m_maxCoords - m_minCoords;
    float maxDim = glm::max(glm::max(size.x, size.y), size.z);

    if (maxDim == 0.0f) {
      m_normMatrix = glm::mat4(1.0f);
      return;
    }

    float scaleFactor = 2.0f / maxDim;
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), -center);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor));
    m_normMatrix = scale * translate;

    Logger::info("Model normalized: center({}, {}, {}), scale_factor: {}", center.x, center.y, center.z, scaleFactor);
  }
  
  void VulkanModel::updAnim(float dT) {
    if(m_pScene && m_pScene->HasAnimations()) {
      float ticksPerSec = m_pScene->mAnimations[m_curAnim]->mTicksPerSecond != 0 ? m_pScene->mAnimations[m_curAnim]->mTicksPerSecond : 25.f;
      float timeInTicks = dT * ticksPerSec;
      float animDur = m_pScene->mAnimations[m_curAnim]->mDuration;
      
      m_animTime = fmod(m_animTime + timeInTicks, animDur);
      
      readNodeHierarchy(m_animTime, m_pScene->mRootNode, glm::mat4(1.f));
      
      m_finalBoneMatrices.resize(m_numBones);
      for(uint32_t i = 0; i < m_numBones; ++i) {
        m_finalBoneMatrices[i] = m_boneInfo[i].finalTransform;
      }
    }
  }
  
  void VulkanModel::setAnim(uint32_t animIndex) {
    if(m_pScene && animIndex < m_pScene->mNumAnimations) {
      m_curAnim = animIndex;
      m_animTime = 0.f;
    }
  }
  
  void VulkanModel::loadBones(const aiMesh* pMesh, std::vector<Vertex>& vertices) {
    for(uint32_t i = 0; i < pMesh->mNumBones; ++i) {
      uint32_t boneIndex = 0;
      std::string boneName(pMesh->mBones[i]->mName.data);
      
      if(m_boneMapping.find(boneName) == m_boneMapping.end()) {
        boneIndex = m_numBones;
        m_numBones++;
        
        // Logger::debug("New bone found: '{}', assigning index {}. Resizing m_boneInfo to {}.", boneName, boneIndex, m_numBones);
        if (m_numBones > MAX_BONES) {
          Logger::error("FATAL: Number of bones ({}) exceeds MAX_BONES ({}). Increase MAX_BONES in opengl_model.hpp and animated.vert shader.",
                                    m_numBones, MAX_BONES);
          return; 
        }
        
        m_boneInfo.resize(m_numBones);
        m_boneInfo[boneIndex].boneOffset = AssimpToGlmMat4(pMesh->mBones[i]->mOffsetMatrix);
        m_boneMapping[boneName] = boneIndex;
      } else {
        boneIndex = m_boneMapping[boneName];
      }
      
      for(uint32_t j = 0; j < pMesh->mBones[i]->mNumWeights; ++j) {
        uint32_t vertexID = pMesh->mBones[i]->mWeights[j].mVertexId;
        float weight = pMesh->mBones[i]->mWeights[j].mWeight;
        
        for(int k = 0; k < MAX_BONES_PER_VERTEX; ++k) {
          if(vertices[vertexID].weights[k] == 0.f) {
            vertices[vertexID].boneIDs[k] = boneIndex;
            vertices[vertexID].weights[k] = weight;
            break;
          }
        }
      }
    }
    
    // normalize weights
    for(size_t i = 0; i < vertices.size(); ++i) {
      float totalWeight = 0.0f;
      for(int j = 0; j < MAX_BONES_PER_VERTEX; ++j) {
        totalWeight += vertices[i].weights[j];
      }
      
      if (totalWeight > 0.0f) {
        for(int j = 0; j < MAX_BONES_PER_VERTEX; ++j) {
          vertices[i].weights[j] /= totalWeight;
        }
      }
    }
  }
  
  void VulkanModel::readNodeHierarchy(float animTime, const aiNode* pNode, const glm::mat4& parentTransform) {
    std::string nodeName(pNode->mName.data);
    const aiAnimation* pAnim = m_pScene->mAnimations[m_curAnim];
    glm::mat4 nodeTransform = AssimpToGlmMat4(pNode->mTransformation);
    
    const aiNodeAnim* pNodeAnim = findNodeAnim(pAnim, nodeName);
    
    if(pNodeAnim) {
      glm::vec3 scaling;
      calcInterpolatedScaling(scaling, animTime, pNodeAnim);
      glm::mat4 scalingM = glm::scale(glm::mat4(1.f), scaling);
      
      glm::quat rotatQ;
      calcInterpolatedRotation(rotatQ, animTime, pNodeAnim);
      glm::mat4 rotationM = glm::toMat4(rotatQ);
      
      glm::vec3 posit;
      calcInterpolatedPosition(posit, animTime, pNodeAnim);
      glm::mat4 positionM = glm::translate(glm::mat4(1.f), posit);
      
      nodeTransform = positionM * rotationM * scalingM;
    }
    
    glm::mat4 globTransform = parentTransform * nodeTransform;
    
    if(m_boneMapping.find(nodeName) != m_boneMapping.end()) {
      uint32_t boneInd = m_boneMapping[nodeName];
      m_boneInfo[boneInd].finalTransform = m_globInverseTransform * globTransform * m_boneInfo[boneInd].boneOffset;
    }
    
    for(uint32_t i = 0; i < pNode->mNumChildren; ++i) {
      readNodeHierarchy(animTime, pNode->mChildren[i], globTransform);
    }
  }
  
  const aiNodeAnim* VulkanModel::findNodeAnim(const aiAnimation* pAnim, const std::string& nodeName) {
    for(uint32_t i = 0; i < pAnim->mNumChannels; ++i) {
      const aiNodeAnim* pNodeAnim = pAnim->mChannels[i];
      if(std::string(pNodeAnim->mNodeName.data) == nodeName) {
        return pNodeAnim;
      }
    }
    
    return nullptr;
  }
  
  uint32_t VulkanModel::findScaling(float animTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
      if (animTime < static_cast<float>(pNodeAnim->mScalingKeys[i + 1].mTime)) {
        return i;
      }
    }
    return 0;
  }
  
  void VulkanModel::calcInterpolatedScaling(glm::vec3& out, float animTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumScalingKeys == 1) {
      out = {pNodeAnim->mScalingKeys[0].mValue.x, pNodeAnim->mScalingKeys[0].mValue.y, pNodeAnim->mScalingKeys[0].mValue.z};
      return;
    }

    unsigned int scalingIndex = findScaling(animTime, pNodeAnim);
    unsigned int nextScalingIndex = scalingIndex + 1;
    float deltaTime = (float)(pNodeAnim->mScalingKeys[nextScalingIndex].mTime - pNodeAnim->mScalingKeys[scalingIndex].mTime);
    float factor = (animTime - (float)pNodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;
    
    const aiVector3D& start = pNodeAnim->mScalingKeys[scalingIndex].mValue;
    const aiVector3D& end = pNodeAnim->mScalingKeys[nextScalingIndex].mValue;
    aiVector3D delta = end - start;
    aiVector3D final = start + factor * delta;

    out = {final.x, final.y, final.z};
  }
  
  uint32_t VulkanModel::findRotation(float animTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
      if (animTime < static_cast<float>(pNodeAnim->mRotationKeys[i + 1].mTime)) {
        return i;
      }
    }
    return 0;
  }
  
  void VulkanModel::calcInterpolatedRotation(glm::quat& out, float animTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumRotationKeys == 1) {
      const auto& val = pNodeAnim->mRotationKeys[0].mValue;
      out = glm::quat(val.w, val.x, val.y, val.z);
      return;
    }

    unsigned int rotationIndex = findRotation(animTime, pNodeAnim);
    unsigned int nextRotationIndex = rotationIndex + 1;
    float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotationIndex].mTime - pNodeAnim->mRotationKeys[rotationIndex].mTime);
    float factor = (animTime - (float)pNodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;

    const aiQuaternion& start = pNodeAnim->mRotationKeys[rotationIndex].mValue;
    const aiQuaternion& end = pNodeAnim->mRotationKeys[nextRotationIndex].mValue;
    aiQuaternion final;
    aiQuaternion::Interpolate(final, start, end, factor);

    out = glm::quat(final.w, final.x, final.y, final.z);
  }
  
  uint32_t VulkanModel::findPosition(float animTime, const aiNodeAnim* pNodeAnim) {
    for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
      if (animTime < static_cast<float>(pNodeAnim->mPositionKeys[i + 1].mTime)) {
        return i;
      }
    }
    return 0;
  }
  
  void VulkanModel::calcInterpolatedPosition(glm::vec3& out, float animTime, const aiNodeAnim* pNodeAnim) {
    if (pNodeAnim->mNumPositionKeys == 1) {
      out = {pNodeAnim->mPositionKeys[0].mValue.x, pNodeAnim->mPositionKeys[0].mValue.y, pNodeAnim->mPositionKeys[0].mValue.z};
      return;
    }
    
    unsigned int positionIndex = findPosition(animTime, pNodeAnim);
    unsigned int nextPositionIndex = positionIndex + 1;
    float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPositionIndex].mTime - pNodeAnim->mPositionKeys[positionIndex].mTime);
    float factor = (animTime - (float)pNodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;

    const aiVector3D& start = pNodeAnim->mPositionKeys[positionIndex].mValue;
    const aiVector3D& end = pNodeAnim->mPositionKeys[nextPositionIndex].mValue;
    aiVector3D delta = end - start;
    aiVector3D final = start + factor * delta;
    
    out = {final.x, final.y, final.z};
  }
  
}; //V