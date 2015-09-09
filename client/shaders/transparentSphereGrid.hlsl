#include "common.inc.hlsl"



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
	float step = max(abs(ddx(input.sourceOpacity)), abs(ddy(input.sourceOpacity)));
	float edge = (smoothstep(0.93 - step, 0.93 + step, abs(input.sourceOpacity))) / (1.0 + 10.0 * step * step);

	//float o = input.sourceOpacity
	float3 color = float3(0.65,0.65,1.0);
	color += edge;
	color *= min(1.0,10.0*(1.0 - abs(input.sourceOpacity)));
	return float4(color,0.3 + 0.7 * edge);
	//float4(0.7, 1.3, 0.8, smoothstep(0.8, 0.9, input.sourceOpacity));;
}


