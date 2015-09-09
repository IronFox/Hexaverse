#include "common.inc.hlsl"


cbuffer Color
{
	float4 Color;
};

Texture2D <float4> Texture;
SamplerState Sampler;


struct A2V
{
	float2 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
	float2 uv : UV;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	float3 world = mul(ObjectMatrix,float4(input.position,0.0,1.0)).xyz;
	//input.position;
	output.position = TransformAndProject(world);
	output.uv = input.position * 0.5 + 0.5;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 texel = Texture.Sample(Sampler,input.uv);
	//return float4(input.uv,0,1);
	return texel * Color;

}


