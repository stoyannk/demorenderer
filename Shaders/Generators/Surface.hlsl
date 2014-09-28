#include "PolygonizerCommon.hlsl"

#define NORM_DELTA 0.1

static const float3 InitialCoords = float3(-4, -4, -4);
static const float Step = 0.1;

float sceneDistance(float3 position)
{
	float terrain;
	{
		const float4 sphere = float4(0, 0, 0, 4.5);
		float ctx = 2 + cos(Time.x* 0.25);
		terrain = sphereDist(sphere, position)
			+ fastNoise(position * 0.52 * ctx) * 2.5
			+ (fastNoise(position * 2.03) / 2)
			+ (fastNoise(position * 2.05) / 4);
	}
	return terrain;
}

uint2 textureIndices(float3 position, float sceneDist)
{
	return makeTextureIndicesPack(2, 0, 0,
								3, 1, 1,
								1);
}
