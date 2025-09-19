struct VSInput {
    float2 inPos;
    float3 inClr;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 clr;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPos, 0.f, 1.f);
    output.clr = input.inClr;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target {
    return float4(inVert.clr, 1.f);
}