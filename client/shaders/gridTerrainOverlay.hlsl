#include "common.inc.hlsl"
//#include "shadow.hlsli"


struct A2V
{
	float3 position : POSITION;
	float opacity : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float opacity : COLOR;
};




void vsMain(A2V input, out V2P output)
{
	output.world = input.position;
	output.position = TransformAndProject(output.world);
	output.opacity = input.opacity;
}



float4 psMain(V2P input) : SV_TARGET
{
	return float4(0,0,0,smoothstep(0.1,0.3, input.opacity) * 0.45);
}


