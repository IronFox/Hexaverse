#include "common.inc.hlsl"



struct A2V
{
	float2 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
	float2 uv : UV;
};

Texture2D <float4> SplashTexture;
SamplerState SplashSampler;


//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.position = float4(input.position,0,1);
	output.uv  = input.position * 0.5 + 0.5;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 tex = SplashTexture.Sample(SplashSampler,input.uv);
	//tex.rgb *= 1.0 - pow(1.0 - input.uv.y,2.0);
	return tex;
}


