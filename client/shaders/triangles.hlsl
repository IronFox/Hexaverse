#include "common.inc.hlsl"


cbuffer Tint
{
	float4 Tint;
}

struct A2V
{
	float2 position : POSITION;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position,0.0));
	//float4(input.position*2.0 - 1.0,0.0,1.0);
	output.color = input.color;
}


float4 psMain(V2P input) : SV_TARGET
{
	return float4(input.color.rgb,input.color.a * input.color.a) * Tint;
}


