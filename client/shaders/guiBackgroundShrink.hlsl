#include "common.inc.hlsl"


cbuffer PixelScale
{
	float2 PixelScale;
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
	float4 accum = (float4)0;
	for (int x = -1; x < 3; x++)
		for (int y = -1; y < 3; y++)
		{
			float2 uv = input.uv + PixelScale * float2(x,-y);
			uv.y = 1.0 - uv.y;
			accum += BackgroundTexture.Sample(BackgroundSampler,uv);
		}
	return accum / 16.0;
}


