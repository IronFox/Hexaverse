#include "common.inc.hlsl"


cbuffer TopColor
{
	float4 TopColor;
}
cbuffer BottomColor
{
	float4 BottomColor;
}
cbuffer Viewport
{
	float4 Viewport;
}
cbuffer MouseAt_FadeRange
{
	float2 	MouseAt;
	float	ShapeCenter, FadeStretch;
}



struct A2V
{
	float2 position : POSITION;
	float4 normal_uv : NORMAL;
};


struct V2P
{
	float4 position : SV_POSITION;	
	float2 world : World;
	float4 normal_uv : NORMAL;
};



void vsMain(in A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position,0));
	output.world = input.position;
	output.normal_uv = input.normal_uv;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 rs = lerp(BottomColor,TopColor,input.normal_uv.z);
	float b = dot(input.normal_uv.xy,normalize(float2(-1,1)));
	float c = saturate(b*1000.0);
	return rs * float4(c,c,c,abs(b));
}


