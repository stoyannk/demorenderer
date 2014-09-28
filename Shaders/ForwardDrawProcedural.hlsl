#include "ForwardDrawDefs.hlsl"

Texture2DArray txDiffuse : register(t0);
Texture2DArray txNormal : register(t1);

// Generated meshes
struct VS_INPUT_GEN
{
	float4 Pos : POSITION;
	float3 Normal : NORMAL;
	uint2 TextureIndices : TEXCOORD0;
};

struct PS_INPUT_GEN
{
	float4 Pos : SV_POSITION;
	float3 Normal : TEXCOORD0;
	float4 WorldPosition : TEXCOORD1;
	
	uint2 TextureIndices : TEXCOORD2;
	float TexBlend : TEXCOORD3;
	float3 Tangent1 : TEXCOORD4;
	float3 Tangent2 : TEXCOORD5;

	float3 ObjectPosition : TEXCOORD6;
	float3 ObjectNormal : TEXCOORD7;
};

#define TBN_EPSILON 0.03125f

PS_INPUT_GEN VS_Gen(VS_INPUT_GEN input)
{
	PS_INPUT_GEN output = (PS_INPUT_GEN)0;

	output.ObjectPosition = input.Pos.xyz;
	output.ObjectNormal = input.Normal;

	output.Pos = mul(input.Pos, World);
	output.WorldPosition = output.Pos / output.Pos.w;
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);

	// We only allow uniform scaling, so it's ok to transform by the World
	// instead of the inverse-transpose
	output.Normal = mul(float4(input.Normal, 0), World).xyz;

	// Calculations from Lengyel's Voxel terrain (5.5.2)
	const float t1denom = max(sqrt(output.Normal.x * output.Normal.x + output.Normal.y * output.Normal.y), TBN_EPSILON);
	output.Tangent1 = float3(output.Normal.y / t1denom, -output.Normal.x / t1denom, 0);

	const float t2denom = max(sqrt(output.Normal.x * output.Normal.x + output.Normal.z * output.Normal.z), TBN_EPSILON);
	output.Tangent2 = float3(-output.Normal.z / t2denom, 0, output.Normal.x / t2denom);

	output.TextureIndices = input.TextureIndices;
	output.TexBlend = ((input.TextureIndices[0] >> 8) & 0xFF) / 255.f;

	return output;
}

float4 PS_Gen(PS_INPUT_GEN input) : SV_Target
{
	static const float3 ambient = float3(0.0f, 0.0f, 0.0f);
	static const float specularIntensity = 1;

	float3 normal = normalize(input.Normal);
	float3 objectNormal = normalize(input.ObjectNormal);

	// Triplanar projection
	const float delta = 0.5f;
	const float m = 4.f;

	float3 blend = saturate(abs(objectNormal) - delta);
	blend = pow(blend, m);
	blend /= dot(blend, float3(1.0f, 1.0f, 1.0f));

	// Txz, Uxz
	float2 material1 = float2(input.TextureIndices[0] >> 24, (input.TextureIndices[0] >> 16) & 0xFF);
	// Tpy, Tny, Upy, Uny
	float4 material2 = float4(input.TextureIndices[1] >> 24
		, (input.TextureIndices[1] >> 16) & 0xFF
		, (input.TextureIndices[1] >> 8) & 0xFF
		, (input.TextureIndices[1]) & 0xFF);
	
	const float3 flip = float3(objectNormal.x < 0.0, objectNormal.y < 0.0, objectNormal.z >= 0.0);
	const float2 zindex = lerp(material2.xz, material2.yw, flip.y);
	const float3 p = input.ObjectPosition;
	
	const float3 s = lerp(p.zxx, -p.zxx, flip.xzy);
	const float3 t = p.yyz;
	// Sample the diffuse color
	const float3 cx0 = txDiffuse.Sample(samLinear, float3(float2(s.x, t.x), material1.x)).xyz;
	const float3 cz0 = txDiffuse.Sample(samLinear, float3(float2(s.y, t.y), material1.x)).xyz;
	const float3 cy0 = txDiffuse.Sample(samLinear, float3(float2(s.z, t.z), zindex.x)).xyz;

	const float3 cx1 = txDiffuse.Sample(samLinear, float3(float2(s.x, t.x), material1.y)).xyz;
	const float3 cz1 = txDiffuse.Sample(samLinear, float3(float2(s.y, t.y), material1.y)).xyz;
	const float3 cy1 = txDiffuse.Sample(samLinear, float3(float2(s.z, t.z), zindex.y)).xyz;

	float3 albedo = lerp((blend.x * cx0 + blend.y * cy0 + blend.z * cz0), (blend.x * cx1 + blend.y * cy1 + blend.z * cz1), input.TexBlend);

	// Fetch and calculate normals
	const float3 nx0 = txNormal.Sample(samLinear, float3(float2(s.x, t.x), material1.x)).xyz;
	const float3 nz0 = txNormal.Sample(samLinear, float3(float2(s.y, t.y), material1.x)).xyz;
	const float3 ny0 = txNormal.Sample(samLinear, float3(float2(s.z, t.z), zindex.x)).xyz;
	const float3 nx1 = txNormal.Sample(samLinear, float3(float2(s.x, t.x), material1.y)).xyz;
	const float3 nz1 = txNormal.Sample(samLinear, float3(float2(s.y, t.y), material1.y)).xyz;
	const float3 ny1 = txNormal.Sample(samLinear, float3(float2(s.z, t.z), zindex.y)).xyz;

	// Calculate tangent & bitangent
	const float3 tangent1 = normalize(input.Tangent1);
	const float3 tangent2 = normalize(input.Tangent2);

	const float3 bitangent1 = normalize(cross(tangent1, normal));
	const float3 bitangent2 = normalize(cross(tangent2, normal));

	float3 nx = normalize(lerp(nx0, nx1, input.TexBlend) * 2.0f - 1.0f);
	float3 ny = normalize(lerp(ny0, ny1, input.TexBlend) * 2.0f - 1.0f);
	float3 nz = normalize(lerp(nz0, nz1, input.TexBlend) * 2.0f - 1.0f);

	const float3x3 tbn1 = float3x3(tangent1, bitangent1, normal);
	const float3x3 tbn2 = float3x3(tangent2, bitangent2, normal);
	
	nx = mul(nx, tbn1);
	ny = mul(ny, tbn1);
	nz = mul(nz, tbn2);
	// End triplanar projection

	float3 specColor = float3(0.02f, 0.02f, 0.02f);
	const float specularPower = MaterialProperties.x;
	const float3 toEye = normalize(CameraPosition.xyz - input.WorldPosition.xyz);

	float3 finalColor = 0;
	// Global directional light
	{
		finalColor += CalcLightTriplanar(toEye,
			-normalize(GlobalLightDirInt.xyz),
			GlobalLightDirInt.w,
			nx,
			ny,
			nz,
			blend,
			GlobalLightColor.xyz,
			albedo,
			specColor,
			specularPower);
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
			finalColor += CalcLightTriplanar(toEye,
				toLight,
				attenuation,
				nx,
				ny,
				nz,
				blend,
				light.Color.xyz,
				albedo,
				specColor,
				specularPower);
		}
	}
	finalColor += ambient;

	return float4(finalColor, 1);
}