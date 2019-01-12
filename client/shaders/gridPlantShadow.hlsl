#include "common.inc.hlsl"


cbuffer Time
{
	float	Time;
};

struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 colorRGB_wind : COLOR;
	float3 instPosition : InstancePosition;
	float3 instUp : InstanceUp;
	float3 instDir : InstanceDir;
	float  instScale : InstanceScale;
};

struct V2P
{
	float4 position : SV_POSITION;
};


void vsMain(A2V input, out V2P output)
{
	float3x3 system;
	system[0] = input.instDir;
	system[2] = input.instUp;
	system[1] = cross(input.instUp,input.instDir);
	float3 world = mul(input.position,system)*input.instScale + input.instPosition;
	output.position = TransformAndProject(world);
}

void psMain()
{
	
}

