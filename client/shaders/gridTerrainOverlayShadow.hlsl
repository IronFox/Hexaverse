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
};




void vsMain(A2V input, out V2P output)
{
	output.position = TransformAndProject(input.position);
}



float4 psMain() : SV_TARGET
{
	return (float4)1;
}


