#include "common.inc.hlsl"

struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.position = TransformAndProject(mul(ObjectMatrix, float4(input.position,1.0)).xyz);
	//output.position.z = output.position.w * 0.5;	//middle of defined space, defo less than 1.0. should be visible
}

void psMain()
{
	
}


