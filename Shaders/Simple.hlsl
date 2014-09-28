cbuffer PerFrameBuffer : register(b0)
{
	matrix View;
	matrix Projection;
}

struct VS_INPUT
{
	float4 Position : POSITION;
	float3 Normal : NORMAL;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;

	output.Position = mul(input.Position, View);
	output.Position = mul(output.Position, Projection);

	output.Normal = input.Normal;

	return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
	static const float3 LighDirection = float3(0, -1, 0);
	
	float nl = saturate(dot(-LighDirection, input.Normal));

	const float3 color = float3(1, 0, 0) * nl;

	return float4(color, 1);
}
