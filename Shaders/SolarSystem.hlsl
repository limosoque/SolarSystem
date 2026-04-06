cbuffer CBPerObject : register(b0)
{
    float4x4 World;
    float4x4 ViewProj;
    float4 BaseColor;
    float4 OrbitParams;
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
    if (OrbitParams.z > 0.5f)
    {
        float3 center = float3(World[3][0], World[3][1], World[3][2]);
        float dist = distance(input.WorldPos, center);
        
        float radius = OrbitParams.x;
        float thickness = OrbitParams.y;
        float distToLine = abs(dist - radius);
        
        if (distToLine > thickness)
            discard;
        
        return input.Color;
    }
    
    return input.Color;
}
