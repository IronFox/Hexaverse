#include "common.inc.hlsl"



struct A2V
{
	float2 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
};


cbuffer Color
{
	float4 Color;
};


void vsMain(A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position,0.0));
}

float4 psMain(V2P input): SV_TARGET
{
	return Color;
}

