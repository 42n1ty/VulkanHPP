struct VSInput {
    float3 inPos;
    float3 inClr;
    float2 inTexCoord;
};

struct UniformBuf {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuf> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 clr;
    float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPos, 1.f))));
    output.clr = input.inClr;
    output.texCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target {
    return texture.Sample(inVert.texCoord);
}