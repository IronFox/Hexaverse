#include "common.inc.hlsl"


Texture1D LineTex;
SamplerState LineSampler;


struct A2V
{
	float4 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 sourcePosition : SourcePosition;
	float sourceOpacity : SourceOpacity;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.position = TransformAndProject(mul(ObjectMatrix,float4(input.position.xyz, 1.0)).xyz);
	output.sourcePosition = input.position.xyz;
	output.sourceOpacity = input.position.w;
}


float4 psMain(V2P input) : SV_TARGET
{
	float4 s = LineTex.Sample(LineSampler, input.sourceOpacity);
	clip(s.a-0.01);
	return float4(s.rgb,1.0);
	//float4(0.7, 1.3, 0.8, smoothstep(0.8, 0.9, input.sourceOpacity));;
}


