#include "common.inc.hlsl"


cbuffer KeyColor
{
	float4 KeyColor;
};
cbuffer Tint
{
	float4 Tint;
};


struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 normal : Normal;
	float4 color : COLOR;
	float3 world : World;
	float distance : Distance;
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color;
	output.distance = distance(WorldCamera[3].xyz,output.world);
}

float4 psMain(V2P input) : SV_TARGET
{
	return float4(Tint.rgb,1.0 - smoothstep(40.0, 45.0, input.distance));
}


