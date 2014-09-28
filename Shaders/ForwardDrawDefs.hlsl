#define TILE_SIZE 8
#define MAX_LIGHTS_PER_TILE 32

Texture2D txAlphaMask : register(t2);
Texture2D txSpecularMap : register(t3);

SamplerState samLinear : register(s0);
SamplerState samPoint : register(s1);

struct LightNode {
	uint LightId;
};

StructuredBuffer<LightNode> Lights : register(t4);
StructuredBuffer<uint> LightsCountBuffer : register(t5);

struct PointLightProperties {
	float4 PositionAndRadius;
	float4 Color;
};
StructuredBuffer<PointLightProperties> PointLightsIn : register(t6);

cbuffer PerFrame : register(b0)
{
	matrix View;
	matrix Projection;
	vector Globals;
};

cbuffer PerSubset : register(b1)
{
	matrix World;
	vector MaterialProperties; // x - specular power
};

cbuffer GlobalProperties : register(b2)
{
	vector CameraPosition;
	vector GlobalLightDirInt; // xyz - direction; w - intensity
	vector GlobalLightColor;
};

#define PI 3.14159265f

float3 CalcLight(float3 toEye,
	float3 toLight,
	float intensity,
	float3 normal,
	float3 lightColor,
	float3 albedo,
	float3 specularColor,
	float specularPower)
{
	const float3 half_vec = normalize(toEye + toLight);
	const float nl = saturate(dot(normal, toLight));
	const float3 diffuse = albedo / PI;

	const float3 fresnel = specularColor + (1.0f - specularColor) * pow(1 - saturate(dot(toLight, half_vec)), 5);
	const float ndf = ((specularPower + 2) / (2 * PI)) * pow(saturate(dot(normal, half_vec)), specularPower);
	const float geom = 1.0f;
	const float3 specular = (fresnel * ndf * geom) / 4.0f;

	return intensity * nl * lightColor * PI * (diffuse + specular);
}

float3 CalcLightTriplanar(float3 toEye,
	float3 toLight,
	float intensity,
	float3 nx,
	float3 ny,
	float3 nz,
	float3 blend,
	float3 lightColor,
	float3 albedo,
	float3 specularColor,
	float specularPower)
{
	const float3 half_vec = normalize(toEye + toLight);
	float3 nl_vec = float3(saturate(dot(nx, toLight)),
							saturate(dot(ny, toLight)),
							saturate(dot(nz, toLight)));

	const float nl = dot(nl_vec, blend);

	float3 n_half_vec = float3(saturate(dot(nx, half_vec)),
		saturate(dot(ny, half_vec)),
		saturate(dot(nz, half_vec)));
	const float n_half = dot(n_half_vec, blend);

	const float3 diffuse = albedo / PI;
	const float3 fresnel = specularColor + (1.0f - specularColor) * pow(1 - saturate(dot(toLight, half_vec)), 5);
	const float ndf = ((specularPower + 2) / (2 * PI)) * pow(n_half, specularPower);
	const float geom = 1.0f;
	const float3 specular = (fresnel * ndf * geom) / 4.0f;

	return intensity * nl * lightColor * PI * (diffuse + specular);
}
