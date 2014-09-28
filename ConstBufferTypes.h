#pragma once

struct PerFrameBuffer
{
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Projection;
	DirectX::XMVECTOR Globals;
};

struct PerSubsetBuffer
{
	DirectX::XMMATRIX World;
	DirectX::XMFLOAT4 Properties; // x - specular power
};

struct CSPointLightProperties
{
	DirectX::XMFLOAT4 PositionAndRadius;
	DirectX::XMFLOAT4 Color;
};

struct CSCulledLight
{
	unsigned LightId;
};


