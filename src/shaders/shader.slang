struct VSInput {
    float3 inPos;
    float3 inClr;
    float2 inTexCoord;
    
    int4 inBoneIDs;
    float4 inWeights;
};

struct CameraData {
    float4x4 view;
    float4x4 proj;
}

struct ObjectData {
    float4x4 model;
};

struct BoneData {
    float4x4 bones[100];
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraData> camera;
[[vk::binding(2, 0)]] ConstantBuffer<ObjectData> object;
[[vk::binding(3, 0)]] ConstantBuffer<BoneData> bones;

struct VSOutput {
    float4 pos : SV_Position;
    float3 clr;
    float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    //anim==========
    float4x4 boneTransform = (float4x4)0;
    boneTransform += bones.bones[input.inBoneIDs.x] * input.inWeights.x;
    boneTransform += bones.bones[input.inBoneIDs.y] * input.inWeights.y;
    boneTransform += bones.bones[input.inBoneIDs.z] * input.inWeights.z;
    boneTransform += bones.bones[input.inBoneIDs.w] * input.inWeights.w;
    
    if(input.inWeights.x == 0 && input.inWeights.y == 0 && input.inWeights.z == 0 && input.inWeights.w == 0) {
      boneTransform = (float4x4)1;
    }
    //anim==========
    output.pos = mul(camera.proj, mul(camera.view, mul(object.model, mul(boneTransform, float4(input.inPos, 1.f)))));
    output.clr = input.inClr;
    output.texCoord = input.inTexCoord;
    return output;
}

// [[vk::binding(0, 1)]] Texture2D texture;
// [[vk::binding(0, 1)]] SamplerState sampler;
[[vk::binding(0, 1)]] Sampler2D sampler;

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target {
    return sampler.Sample(inVert.texCoord);
}