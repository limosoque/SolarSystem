cbuffer CBPerObject : register(b0)
{
    float4x4 World;
    float4x4 ViewProj;
    float4 BaseColor;
};

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
};

struct PS_IN
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 Color : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;

    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.ClipPos = mul(worldPos, ViewProj);
    output.WorldPos = worldPos.xyz;

    // Transform normal to world space (no non-uniform scale here, so no inverse-transpose needed)
    output.Normal = normalize(mul(input.Normal, (float3x3) World));
    output.Color = input.Color * BaseColor;

    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    // Sun is at world origin — use it as the point light
    static const float3 LightPos = float3(0.0f, 0.0f, 0.0f);
    static const float3 LightColor = float3(1.0f, 0.9f, 0.8f);
    static const float Ambient = 0.15f;

    float3 N = normalize(input.Normal);
    float3 L = normalize(LightPos - input.WorldPos);

    // Diffuse
    float NdotL = saturate(dot(N, L));
    float dist = length(LightPos - input.WorldPos);
    float atten = 1.0f / (1.0f + 0.01f * dist * dist); // quadratic falloff
    float3 diffuse = LightColor * NdotL * atten;

    // Simple specular (Blinn-Phong)
    // Camera position is not in CB; skip full specular, do a cheap rim instead.
    float3 rim = pow(1.0f - saturate(dot(N, L)), 3.0f) * 0.15f;

    float3 albedo = input.Color.rgb;
    float3 finalRGB = albedo * (Ambient + diffuse) + rim;

    return float4(finalRGB, input.Color.a);
}
