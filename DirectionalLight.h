#pragma once

struct DirectionalLight
{
	DirectionalLight(const DirectX::XMFLOAT4& props, const DirectX::XMFLOAT3& color)
		: Properties(props)
		, Color(color)
	{}

	DirectX::XMFLOAT4 Properties; // xyz - direction, w - intensity
	DirectX::XMFLOAT3 Color;
};
