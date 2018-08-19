#include "common.inc.hlsl"


cbuffer ShadowColor
{
	float4 ShadowColor;
}
cbuffer MouseAt
{
	float2 MouseAt;
}


struct A2V
{
	float4 position_uv : POSITION;
};


struct V2P
{
	float4 position : SV_POSITION;	
	float2 uv : UV;
};



void vsMain(in A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position_uv.xy,0));
	output.uv = input.position_uv.zw;
}

float4 psMain(V2P input) : SV_TARGET
{
	//return lerp(BottomBackgroundColor,TopBackgroundColor,input.uv.y);
	//return float4(input.uv,1,1);
	return ShadowColor;
}


