#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>

namespace V {
  
  inline glm::mat4 AssimpToGlmMat4(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    
    return to;
  }
  
  inline glm::vec3 AssimpToGlmVec3(const aiVector3D& from) {
    return glm::vec3(from.x, from.y, from.z);
  }
  
  inline glm::quat AssimpToGlmQuat(const aiQuaternion& from) {
    return glm::quat(from.w, from.x, from.y, from.z);
  }
  
}; //V