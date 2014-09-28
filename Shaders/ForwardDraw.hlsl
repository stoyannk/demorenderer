#include "ForwardDrawDefs.hlsl"

Texture2D txDiffuse : register(t0);
Texture2D txNormal : register(t1);

struct VS_INPUT
{
    float4 Pos : POSITION;
	float2 Tex : TEXCOORD0;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float3 Bitangent : BITANGENT;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float3 Normal : TEXCOORD1;
	float3 Tangent : TEXCOORD2;
	float3 Bitangent : TEXCOORD3;
	float4 WorldPosition : TEXCOORD4;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;

	output.Pos = mul(input.Pos, World);
	output.WorldPosition = output.Pos / output.Pos.w;

    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

	output.Tex = input.Tex;

	// We only allow uniform scaling, so it's ok to transform by the World
	// instead of the inverse-transpose
	output.Normal = mul(float4(input.Normal, 0), World).xyz;
	output.Tangent = mul(float4(input.Tangent, 0), World).xyz;
	output.Bitangent = mul(float4(input.Bitangent, 0), World).xyz;

    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	static const float3 ambient = float3(0.0f, 0.0f, 0.0f);
	static const float specularIntensity = 1;

	float alpha = 1;
    if(g_HasAlphaMask)
	{
		alpha = txAlphaMask.Sample(samLinear, input.Tex);
	}

	float2 uv = input.Pos.xy / Globals.xy;
	float3 albedo = txDiffuse.Sample(samLinear, input.Tex).xyz;

	float3 normal = normalize(input.Normal);
	float3 tangent = normalize(input.Tangent);
	float3 bitangent = normalize(input.Bitangent);
	float3x3 tbn = float3x3(tangent, bitangent, normal);

	if (g_HasNormal)
	{
		normal = normalize(txNormal.Sample(samLinear, input.Tex).xyz * 2.0f - 1.0f);
		normal = mul(normal, tbn);
	}
	// I don't have specular colors for all subsets, so settle with this
	float3 specColor = float3(0.02f, 0.02f, 0.02f);
	if (g_HasSpecularMap)
	{
		specColor = txSpecularMap.Sample(samLinear, input.Tex).xyz;
	}

	const float3 toEye = normalize(CameraPosition.xyz - input.WorldPosition.xyz);
	
	float3 finalColor = 0;
	// Global directional light
	{
		finalColor += CalcLight(toEye,
			-normalize(GlobalLightDirInt.xyz),
			GlobalLightDirInt.w,
			normal,
			GlobalLightColor.xyz,
			albedo,
			specColor,
			MaterialProperties.x);
	}

	// Point Lights
	const float2 groupCoord = floor(input.Pos.xy / float2(TILE_SIZE, TILE_SIZE));
	const uint groupId = (uint(groupCoord.y) * (Globals.x / TILE_SIZE)) + uint(groupCoord.x);
	const uint lightsCount = LightsCountBuffer[groupId];
	for (int lid = 0; lid < lightsCount; ++lid) {
		PointLightProperties light = PointLightsIn[Lights[groupId*MAX_LIGHTS_PER_TILE + lid].LightId];
		float3 toLight = light.PositionAndRadius.xyz - input.WorldPosition.xyz;
		const float toLightLen = length(toLight);
		const float attenuation = saturate(1 - toLightLen / light.PositionAndRadius.w);
		if (attenuation > 0) {
			toLight /= toLightLen;
			finalColor += CalcLight(toEye,
				toLight,
				attenuation,
				normal,
				light.Color.xyz,
				albedo,
				specColor,
				MaterialProperties.x);
		}
	}
	finalColor += ambient;

	return float4(finalColor, alpha);
}
