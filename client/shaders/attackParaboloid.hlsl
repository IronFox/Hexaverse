#include "common.inc.hlsl"
#include "light.hlsli"


Texture2D <float4> Texture;
SamplerState Sampler;

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
	float3 object : Object;
	//float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	output.object = input.position;
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color /** SunColor*/;

	float2 uv = output.object.xy;
	uv.y *= 512.0 / 886.0;
	uv *= 0.5;
	float x = length(uv);
	
}

float4 psMain(V2P input) : SV_TARGET
{
	//float2 uv = input.object.xy;
	//uv.y *= 512.0 / 886.0;
	//uv *= 0.5;
	//float s = 1.0 - Texture.Sample(Sampler,uv).r;
	//return float4(1.0,0.5,0.4,(s+0.35) * 0.4) * (1.0 - sqr(sqr(input.color.r)));

	float3 reflected = normalize(reflect(input.world - WorldCamera[3].xyz, input.normal));
	float diffuse = max(0.0, dot(input.normal, Sun.xyz))* (/*KeyColor.a * */input.color.a * /*Shade(input.world) **/ 0.8 + 0.2);
	float ambient = /*KeyColor.a * */input.color.a * 0.7 + 0.3;
	float toneDown = sqr(saturate(1.0 - input.object.z*2.0));
	ambient *= (1.0 - toneDown*0.5);
	float keyMatch = smoothstep(0.9,1.0,input.color.r - input.color.g - input.color.b);
	float3 color = (input.color.rgb * (diffuse*0.65 + ambient*0.35)/* * Color.a * input.color.a*/) * (1.0 - keyMatch);


	float fresnel = (((1.0 - abs(dot(WorldCamera[2].xyz,input.normal)))));

	return float4(color * Tint.rgb, fresnel * Tint.a);
}


