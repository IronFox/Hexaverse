#include "common.inc.hlsl"


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



void vsMain(in A2V input, out V2P output)
{
	output.position = mul(ObjectMatrix,float4(input.position,0,1));
	output.position.xy = output.position.xy * 2.0 - 1.0;
	output.uv = input.position;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 s = Texture.Sample(Sampler,input.uv);
	return s;
}


