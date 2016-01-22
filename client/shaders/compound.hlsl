#include "common.inc.hlsl"
#include "shadow.hlsli"

/*
this shader renders compound schematic graphics over structures.
Tint defines the color to use for the compound.
Depth test is disabled, as is depth write. Alpha blend should be enabled
*/


cbuffer Tint
{
	float4 Tint;
};

cbuffer WirelessRange
{
	float4 WirelessRange;
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
	float distance : Distance;
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color;
	output.distance = distance(WorldCamera[3].xyz,output.world);
}

float4 psMain(V2P input, float4 screenSpace : SV_Position) : SV_TARGET
{
	float z0 = (screenSpace.x + screenSpace.y);
	float step = max(abs(ddx(z0)),abs(ddy(z0)));
	float z = fmod(z0,20.0);
	float a = smoothstep(5.0 - step,5.0+step,z);
	float b = 1.0 - smoothstep(15.0 - step,15.0+step,z);
	
	float dist = sqr(input.world - WirelessRange.xyz) / (WirelessRange.w);	//.w already squared
	step = max(abs(ddx(dist)),abs(ddy(dist)));
	float opacity = 1.0 - 0.5 * smoothstep(1.0-step,1.0+step,dist);
	float3 tint = Tint.rgb * opacity;
	float stripe = a * b;
	stripe = 0.5 + 0.5 * stripe;
	
	return float4(tint,stripe * Tint.a * opacity);	//if this works we move on
}


