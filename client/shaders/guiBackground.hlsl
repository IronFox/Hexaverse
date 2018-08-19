#include "common.inc.hlsl"


cbuffer BottomBackgroundColor
{
	float4 BottomBackgroundColor;
}
cbuffer TopBackgroundColor
{
	float4 TopBackgroundColor;
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
	float2 world : World;	
	float2 uv : UV;
};



void vsMain(in A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position_uv.xy,0));
	output.world = input.position_uv.xy;
	output.uv = input.position_uv.zw;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 rs = lerp(BottomBackgroundColor,TopBackgroundColor,input.uv.y);
	float2 delta = MouseAt - input.world;
	rs.rgb *= 0.8 + 0.3 * (1.0 / (1.0 + 50.0 * dot(delta,delta)));
	return rs;
	
	//return float4(input.uv,1,1);
}


