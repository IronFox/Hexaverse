#include "common.inc.hlsl"


//cbuffer Tint
//{
//	float4 Tint;
//};


cbuffer BeamTopColor
{
	float4 BeamTopColor;
}

cbuffer BeamBottomColor
{
	float4 BeamBottomColor;
}

cbuffer Time
{
	float	Time;
};


struct A2V
{
	float2 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 object : Object;
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.object = float3(input.position.x,0.0,input.position.y);
	output.world = mul(ObjectMatrix,float4(output.object,1.0)).xyz;
	output.position = TransformAndProject(output.world);
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 c = lerp(BeamBottomColor,BeamTopColor,input.object.z);
	c.a *= 1.0 - sqr(abs(input.object.x))*0.9;
	
	c.a *= smoothstep(0.0,0.1,input.object.z) * smoothstep(0.0,0.1,1.0-input.object.z);

	c.a *= sin((input.object.z - Time/12.5)*16.0) * 0.25 + 0.75;
	c.a *= sin((input.object.z - Time/8.5)*8.0) * 0.25 + 0.75;
	c.rgb *= c.a;
	//c.a = 
	return c;
}