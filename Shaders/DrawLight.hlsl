cbuffer PerFrame : register(b0)
{
	matrix View;
	matrix Projection;
	vector Globals;
};

cbuffer PerSubset : register(b1)
{
	matrix World;
	vector Properties;
};

struct VS_Light
{
	float4 Pos : POSITION;
	float2 Tex : TEXCOORD0;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float3 Bitangent : BITANGENT;
};

struct PS_Light
{
	float4 Pos : SV_POSITION;
};

PS_Light VSLight(VS_Light input)
{
	PS_Light output = (PS_Light)0;

	output.Pos = mul(input.Pos, World);
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);

	return output;
}

float4 PSLight(PS_Light input) : SV_Target
{
	return float4(1, 0, 0, 1);
}
