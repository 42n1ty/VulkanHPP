struct VSInput {
    float3 inPos;
    float3 inClr;
    float2 inTexCoord;
};

struct CameraData {
    float4x4 view;
    float4x4 proj;
}

struct ObjectData {
    float4x4 model;
};

ConstantBuffer<CameraData> camera : register(b0);
ConstantBuffer<ObjectData> object : register(b2);

struct VSOutput {
    float4 pos : SV_Position;
    float3 clr;
    float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(camera.proj, mul(camera.view, mul(object.model, float4(input.inPos, 1.f))));
    output.clr = input.inClr;
    output.texCoord = input.inTexCoord;
    return output;
}

Texture2D texture : register(t1);
SamplerState sampler : register(s1);

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target {
    return texture.Sample(sampler, inVert.texCoord);
}