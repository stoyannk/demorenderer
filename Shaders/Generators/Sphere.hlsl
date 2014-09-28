#include "PolygonizerCommon.hlsl"

#define NORM_DELTA 0.01

static const float3 InitialCoords = float3(-2.0, -2.0, -2.0);
static const float Step = 0.25;
float sceneDistance(float3 position)
{
	float4 sphere = float4(0, 0, 0, 1 + sin(Time.x));

	return length(sphere.xyz - position) - sphere.w;
}

uint2 textureIndices(float3 position, float sceneDist)
{
	return makeTextureIndicesPack(2, 0, 0,
								3, 1, 1,
								1);
}
