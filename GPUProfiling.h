#pragma once

struct GPUProfiling
{
#ifdef _DEBUG
	static const unsigned QUERIES_COUNT = 3;
#else
	static const unsigned QUERIES_COUNT = 5;
#endif
	unsigned FrameCounter;

	unsigned CurrentIndex;
	ReleaseGuard<ID3D11Query> FrameDisjoint[QUERIES_COUNT];
	
	ReleaseGuard<ID3D11Query> FrameBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> FrameEnd[QUERIES_COUNT];

	ReleaseGuard<ID3D11Query> ZPrepassBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> ZPrepassEnd[QUERIES_COUNT];

	ReleaseGuard<ID3D11Query> PolygonizeBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> PolygonizeEnd[QUERIES_COUNT];

	ReleaseGuard<ID3D11Query> TileLightsBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> TileLightsEnd[QUERIES_COUNT];

	ReleaseGuard<ID3D11Query> DrawBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> DrawEnd[QUERIES_COUNT];

	ReleaseGuard<ID3D11Query> PresentBegin[QUERIES_COUNT];
	ReleaseGuard<ID3D11Query> PresentEnd[QUERIES_COUNT];
};

extern GPUProfiling gGPUProfiling;