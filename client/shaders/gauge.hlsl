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
	//float3 world : World;
	float height : Height;
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	float3 world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	//output.world = world;
	output.height = input.position.z;
	//input.position;
	output.position = TransformAndProject(world);
	output.color = input.color;
}

float4 psMain(V2P input) : SV_TARGET
{
	float d = dot(input.normal,normalize(float3(1,1,1)));
	float h = sin(d*3.0)*0.5+0.5;

	//float step = fmod(input.height-2.0,10.0)/10.0;
	//step = sqr(sqr((step)));
	//h *= 0.8 + 0.4 * smoothstep(0.0,0.5,step) * smoothstep(0.0,0.5,1.0 - step);
	return float4((h*0.5 + 0.5) * input.color.rgb, input.color.a);
}


