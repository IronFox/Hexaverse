#include "common.inc.hlsl"


struct A2V
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
};




void vsMain(A2V input, out V2P output)
{
	float3 world = input.position.xyz;
	output.position = TransformAndProject(world);

}



float4 psMain() : SV_TARGET
{
	return (float4)1.0;
}


