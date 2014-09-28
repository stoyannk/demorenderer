#pragma once

#include <Utilities/Random.h>

struct PointLight
{
	PointLight(float x, float y, float z, float r)
		: Position(x, y, z)
		, Radius(r)
	{}
	PointLight(float x, float y, float z, float r, float red, float green, float blue)
		: Position(x, y, z)
		, Radius(r)
		, Color(red, green, blue)
	{}

	DirectX::XMFLOAT3 Position;
	float Radius;
	DirectX::XMFLOAT3 Color;
};

struct MovingLight : public PointLight
{
	MovingLight(float x, float y, float z, float r)
		: PointLight(x, y, z, r)
		, LifeTime(0)
	{}

	MovingLight(float x, float y, float z, float r, float red, float green, float blue)
		: PointLight(x, y, z, r, red, green, blue)
		, LifeTime(0)
	{}

	void Update(float dt)
	{
		static const float SPEED = 100.0f;
		Position.x += Direction.x*dt*SPEED;
		Position.y += Direction.y*dt*SPEED;
		Position.z += Direction.z*dt*SPEED;

		LifeTime += dt;
	}

	DirectX::XMFLOAT3 Direction;
	float LifeTime;
};
