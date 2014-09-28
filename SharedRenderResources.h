#pragma once

#define MAX_LIGHTS_PER_TILE 32
#define LIGHTS_TILE_SIZE 8
#define MAX_LIGHTS_IN_SCENE 1000

struct SharedRenderResources
{
	ReleaseGuard<ID3D11UnorderedAccessView> LightsCulledUAV;
	ReleaseGuard<ID3D11ShaderResourceView> LightsCulledSRV;
	ReleaseGuard<ID3D11Buffer> LightsCulledBuffer;

	ReleaseGuard<ID3D11UnorderedAccessView> LightsCulledCountUAV;
	ReleaseGuard<ID3D11ShaderResourceView> LightsCulledCountSRV;
	ReleaseGuard<ID3D11Buffer> LightsCulledCountBuffer;

	ReleaseGuard<ID3D11Buffer> PointLightsBuffer;
	ReleaseGuard<ID3D11ShaderResourceView> PointLightsSRV;
};

extern SharedRenderResources* gSharedRenderResources;