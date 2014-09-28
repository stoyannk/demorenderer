cbuffer PerFramePolygonizer : register(b1) // Use 1 as 0 is always the global per-frame buffer
{
	vector Time; //x - time since start in secs, y - time since last frames
};

Texture2D randomTexture : register(t2);

SamplerState samLinear : register(s0);

#define RANDOM_TEXTURE_SZ 256.0f

// Same packing as in Lengyel's Voxel terrain (figure 5.8)
uint2 makeTextureIndicesPack(
	uint Txz, uint Tpy, uint Tny,
	uint Uxz, uint Upy, uint Uny,
	float blend)
{
	uint2 result;
	result.x = (Txz & 0xFF) << 24 |
		(Uxz & 0xFF) << 16 |
		(uint(blend * 255) & 0xFF) << 8;
	result.y = (Tpy & 0xFF) << 24 |
		(Tny & 0xFF) << 16 |
		(Upy & 0xFF) << 8 |
		(Uny & 0xFF);

	return result;
}

// Credit to iq in https://www.shadertoy.com/view/4sfGzS
float fastNoise(float3 position) {
	float3 p = floor(position);
	float3 f = frac(position);
	f = f*f*(3.0 - 2.0*f);

	float2 uv = (p.xy + float2(37.0, 17.0)*p.z) + f.xy;
	float2 rg = randomTexture.SampleLevel(samLinear, (uv + 0.5) / RANDOM_TEXTURE_SZ, 0).yx;
	return lerp(rg.x, rg.y, f.z);
}

// SDFs
float planeDist(float4 plane, float3 position) {
	return dot(plane, float4(position, 1));
}

// sphere: xyz - pos, w - radius
float sphereDist(float4 sphere, float3 position) {
	return length(sphere.xyz - position) - sphere.w;
}

float boxDist(float3 boxExtents, float3 boxPos, float3 position) {
	float3 newPos = position - boxPos;
	float3 d = abs(newPos) - boxExtents;
	float3 dm = float3(max(d.x, 0), max(d.y, 0), max(d.z, 0));

	float dl = length(dm);

	return min(max(d.x, max(d.y, d.z)), 0.f) + dl;
}

float cylinderDist(float2 h, float3 position) {
	float2 d = abs(float2(length(position.xz), position.y)) - h;
	return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float unionDist(float3 d1, float3 d2) {
	return min(d1, d2);
}

float subDist(float3 d1, float3 d2) {
	return max(-d1, d2);
}

float intersectDist(float3 d1, float3 d2) {
	return max(d1, d2);
}

float3 makeRepeat(float3 position, float3 rep) {
	return fmod(position, rep) - 0.5*rep;
}

