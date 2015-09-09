#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer Tint
{
	float4 Tint;
};

cbuffer Progression
{
	float4 Progression;
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
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal;
	output.world = input.position;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color;
}

float4 psMain(V2P input) : SV_TARGET
{
//lerp(float3(0.4,1.0,0.5), Tint.rgb, Tint.a)
	float white = sqr(sqr(sqr(input.color.g)));
	float progression = Progression.x - input.color.b;
	if (progression > 0.0)
		progression = 1.0 - smoothstep(0.0,Progression.z,progression);
	else
		progression = 1.0 - smoothstep(0.0,Progression.y,-progression);
	//progression = input.color.b;
	return float4(lerp((float3)white, Tint.rgb, Tint.a), sqr(input.color.r) * progression);
}


