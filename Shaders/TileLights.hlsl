#define MAX_LIGHTS_PER_TILE 32
#define GROUP_SIZE 8

struct PointLightProperties {
	float4 PositionAndRadius;
	float4 Color;
};

struct LightNode {
	uint LightId;
};

cbuffer PerFrame : register(b0)
{
	matrix View;
	matrix Projection;
	vector Globals;
};

cbuffer TilingData : register(b1)
{
	matrix InvProjection;
	uint LightsCount;
};

// Input
StructuredBuffer<PointLightProperties> PointLightsIn : register(t0);
#ifdef MULTISAMPLING
Texture2DMS<float> txDepth : register(t1);
#else
Texture2D txDepth : register(t1);
#endif

// Output
RWStructuredBuffer<LightNode> LightsBufferOut : register(u0);
RWStructuredBuffer<uint> LightsCountBufferOut : register(u1);

// Shared
groupshared uint LightsCountGroup;
groupshared uint LightIdsGroup[MAX_LIGHTS_PER_TILE];
groupshared uint zMinInt;
groupshared uint zMaxInt;

float3 projectionToView(float3 position) {
	position.xy /= Globals.xy;
	position.xy = (position.xy - 0.5f) * 2.0f;
	position.y = -position.y;
	float4 r = mul(float4(position, 1.0f), InvProjection);
	r /= r.w;
	return r.xyz;
}

// NOTE: Makes a plane from 3 points with the first being
// implicitly 0, 0, 0
float4 makePlaneEquation(float3 v1, float3 v2) {
	float3 n = normalize(cross(v1, v2));

	return float4(n, 0);
}

// Is a sphere in the negative half-space
bool SphereBehindPlane(float4 plane, float4 sphere) {
	return dot(plane, float4(sphere.xyz, 1.0)) < -sphere.w;
}

// A sphere intersects or is in a frustum
bool SphereInFrustum(float4 frustum[6], float4 sphere) {
	for (int i = 0; i < 6; ++i) {
		if (SphereBehindPlane(frustum[i], sphere)) {
			return false;
		}
	}
	return true;
}

//#define DEBUG_SPHERES

#ifdef DEBUG_SPHERES
bool rayIntersectsSphere(float3 origin, float3 direction, float4 sphere) {
	float a = dot(direction, direction);
	float3 centToOr = origin - sphere.xyz;
	float b = 2 * dot(centToOr, direction);
	float c = dot(centToOr, centToOr) - (sphere.w * sphere.w);

	return (b*b - 4 * a * c) >= 0;
}
#endif

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSTileLights(
	uint3 gid : SV_GroupID, 
	uint3 tid : SV_GroupThreadID,
	uint3 dtid : SV_DispatchThreadID,
	uint localTid : SV_GroupIndex) {

	uint groupIndex = gid.y * (Globals.x / GROUP_SIZE) + gid.x;

	// Init shared variables
	int myTidMinusOne = (tid.y * GROUP_SIZE + tid.x) - 1;
	if (tid.x == 0 && tid.y == 0)
	{
		LightsCountGroup = 0;
		zMinInt = 0xFFFFFFFF;
		zMaxInt = 0;
	}
	else if ((tid.y * GROUP_SIZE + tid.x) < 33)
	{
		LightIdsGroup[myTidMinusOne] = 0xFFFFFFFF;
	}
	GroupMemoryBarrierWithGroupSync();

#ifdef DEBUG_SPHERES
	float3 rayDir;
#endif
	// build frustum
	float4 frustum[6];
	{
		float3 vertex[4];
		vertex[0] = projectionToView(float3(GROUP_SIZE * gid.x, GROUP_SIZE * gid.y, 1.0f));
		vertex[1] = projectionToView(float3(GROUP_SIZE * (gid.x + 1), GROUP_SIZE * gid.y, 1.0f));
		vertex[2] = projectionToView(float3(GROUP_SIZE * (gid.x + 1), GROUP_SIZE * (gid.y + 1), 1.0f));
		vertex[3] = projectionToView(float3(GROUP_SIZE * gid.x, GROUP_SIZE * (gid.y + 1), 1.0f));

		for (int pe = 0; pe < 4; ++pe) {
			frustum[pe] = -makePlaneEquation(vertex[pe], vertex[(pe + 1) % 4]);
		}

#ifdef DEBUG_SPHERES
		rayDir = normalize(vertex[0] + vertex[1] + vertex[2] + vertex[3]);
#endif
	}
	
#ifdef MULTISAMPLING
	uint widthD, heightD, samplesCnt;
	txDepth.GetDimensions(widthD, heightD, samplesCnt);

	for (int samp = 0; samp < samplesCnt; ++samp)
#endif
	{
#ifdef MULTISAMPLING
		float depth = txDepth.Load(int2(dtid.x, dtid.y), samp).r;
#else
		float depth = txDepth.Load(uint3(dtid.x, dtid.y, 0)).r;
#endif
		float3 viewPosition = projectionToView(float3(dtid.x, dtid.y, depth));
		uint zInt = asuint(viewPosition.z);

		if (depth != 1.0f)
		{
			InterlockedMin(zMinInt, zInt);
			InterlockedMax(zMaxInt, zInt);
		}
	}
	GroupMemoryBarrierWithGroupSync();
	
	const float minZ = asfloat(zMinInt);
	const float maxZ = asfloat(zMaxInt);
	
	frustum[4] = float4(0, 0, 1, minZ);
	frustum[5] = float4(0, 0, -1, maxZ);
	/*frustum[4] = float4(0, 0, 1, 0);
	frustum[5] = float4(0, 0, -1, 5000);*/

	// Count
	for (uint lightId = localTid; lightId < LightsCount; lightId += GROUP_SIZE*GROUP_SIZE) {
		PointLightProperties light = PointLightsIn[lightId];

		float4 lightPosInView = mul(float4(light.PositionAndRadius.xyz, 1.0), View);

#ifdef DEBUG_SPHERES
		if (rayIntersectsSphere(float3(0, 0, 0), rayDir, float4(lightPosInView.xyz, light.PositionAndRadius.w))) {
#else
		if (SphereInFrustum(frustum, float4(lightPosInView.xyz, light.PositionAndRadius.w))) {
#endif
			uint slot;
			InterlockedAdd(LightsCountGroup, 1, slot);

			if (slot >= MAX_LIGHTS_PER_TILE)
				break;

			LightIdsGroup[slot] = lightId;
		}
	}

	GroupMemoryBarrierWithGroupSync();
	
	// Update global memory
	if (tid.x == 0 && tid.y == 0)
	{
		LightsCountBufferOut[groupIndex] = min(LightsCountGroup, MAX_LIGHTS_PER_TILE);
	}
	else if ((tid.y * GROUP_SIZE + tid.x) < 33)
	{
		LightsBufferOut[(groupIndex * MAX_LIGHTS_PER_TILE) + myTidMinusOne].LightId = LightIdsGroup[myTidMinusOne];
	}
	AllMemoryBarrierWithGroupSync();
}
