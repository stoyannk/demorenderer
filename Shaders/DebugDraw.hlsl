#define TILE_SIZE 8
#define MAX_LIGHTS_PER_TILE 32

struct LightNode {
	uint LightId;
};

StructuredBuffer<LightNode> Lights : register(t0);
StructuredBuffer<uint> LightsCountBuffer : register(t1);

cbuffer PerFrame : register(b0)
{
	matrix View;
	matrix Projection;
	vector Globals;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;

	// Clean up inaccuracies
	input.Pos.xy = sign(input.Pos.xy);
	output.Pos = float4(input.Pos.xy, 0, 1);

	return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
	float2 groupId = floor(input.Pos.xy / float2(TILE_SIZE, TILE_SIZE));

	const uint lightsCount = LightsCountBuffer[(uint(groupId.y) * (Globals.x / TILE_SIZE)) + uint(groupId.x)];

	float3 color;
	if (lightsCount <= 12) {
		color = float3(0, float(lightsCount) / 12, 0);
	}
	else if (lightsCount > 12 && lightsCount <= 24)
	{
		color = float3(0, 0, float(lightsCount) / 24);
	}
	else if (lightsCount > 24 && lightsCount < MAX_LIGHTS_PER_TILE)
	{
		color = float3(float(lightsCount) / MAX_LIGHTS_PER_TILE, 0, 0);
	}
	else
	{
		color = float3(1, 1, 1);
	}

	return float4(color, 1);
}
