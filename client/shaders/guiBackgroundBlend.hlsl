#include "common.inc.hlsl"


cbuffer BlurOpacity
{
	float BlurOpacity;
}

Texture2D<float4> BackgroundTexture;
SamplerState BackgroundSampler;


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
	output.position = float4(input.position * 2.0 - 1.0, 0.0, 1.0);
	output.uv = input.position;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 blurred = BackgroundTexture.Sample(BackgroundSampler,float2(input.uv.x,1.0 - input.uv.y));
	return float4(blurred.rgb,BlurOpacity);
}


