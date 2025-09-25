struct VSInput {
    float2 inPos;
    float3 inClr;
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
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPos, 0.f, 1.f))));
    output.clr = input.inClr;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target {
    return float4(inVert.clr, 1.f);
}