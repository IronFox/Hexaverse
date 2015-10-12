#include "common.inc.hlsl"

#include "shadow.hlsli"




struct A2V
{
	float2 position : POSITION;
};
struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	float4 pos = float4(input.position,0.5,1.0);
	float4 world = mul(ObjectMatrix,pos);
	
	output.world = world.xyz / world.w;


	//output.world = mul(ObjectMatrix,float4(input.position,0.0,1.0)).xyz;
	output.position = TransformAndProject(output.world);
}


float4 psMain(V2P input) : SV_TARGET
{
	//return (float4)1.0;

	float4 c = CalculateDirectionFog(input.world);// + float4(0,0.5,0,0.5);
	return float4(c.rgb * c.a,c.a);


	//float radial = saturate(input.radial*(1.0 + r0) - r0);
	//float3 color = float3(0.7,0.8,1.0)*sqr(sqr((1.0 - radial)));
	//
	////color *= Light(input.world,input.normal);
	//color *=  max(0.0,dot(Sun,input.normal)) * 0.5 + 0.5;


	//return float4(color,0.75);
}


