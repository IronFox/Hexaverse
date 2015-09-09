#include "common.inc.hlsl"



struct A2V
{
	float3 position : POSITION;
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
	output.position = TransformAndProject(input.position);
	output.color = input.color;
}


float4 psMain(V2P input) : SV_TARGET
{
	return input.color;
}


