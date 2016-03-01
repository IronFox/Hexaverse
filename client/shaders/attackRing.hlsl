/*
Shader used to render the normal surfaces of all vehicles
*/

#include "common.inc.hlsl"
#include "shadow.hlsli"

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
	float3 world : World;
};



void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	output.position = TransformAndProject(output.world);
	float4 atmosphere = CalculatePositionFog(output.world);
	output.color.rgb = lerp(input.color.rgb * Tint.rgb,atmosphere.rgb,atmosphere.a);
	output.color.a = input.color.a * Tint.a;
}

float4 psMain(V2P input) : SV_TARGET
{
	return input.color;
}


