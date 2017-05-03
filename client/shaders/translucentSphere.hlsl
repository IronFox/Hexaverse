#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer CutOffConfig
{
	float4 CutOffConfig;
};

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
	float height : Height;
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
	output.height = length(output.world);
}

float4 psMain(V2P input) : SV_TARGET
{
	//clip(CutOffConfig.w - input.height);
	//clip(input.height - CutOffConfig.z);
	const float fadeRange = 0.25;
	float clipFade = (CutOffConfig.w - input.height+ fadeRange)/ fadeRange;
	clipFade = min(clipFade,(input.height - CutOffConfig.z+ fadeRange)/ fadeRange);
	clip(clipFade);
	clipFade = min(clipFade, 1.0);
	////return (float4)(1);
	//float d = abs(1.0  - abs(dot(input.normal, normalize(input.world))));
	//d = pow(d, 1000);
	//return float4(1, 1, 1, 0.2 + 0.8 * d);


	//return float4((float3)Shade(input.world),1.0);
	float3 reflected = normalize(reflect(input.world - WorldCamera[3].xyz, input.normal));
	float diffuse = max(0.0, dot(input.normal, Sun.xyz))* (/*KeyColor.a * */input.color.a * /*Shade(input.world) **/ 0.8 + 0.2);
	float ambient = /*KeyColor.a * */input.color.a * 0.7 + 0.3;
	//float specular = pow(max(0.0, dot(reflected, normalize((float3)1))),4);
	//color = ;
	//float toneDown = sqr(saturate(1.0 - input.object.z*2.0));
	//ambient *= (1.0 - toneDown*0.5);
	//float keyMatch = smoothstep(0.9,1.0,input.color.r - input.color.g - input.color.b);
	float3 color = (input.color.rgb * (diffuse*0.65 + ambient*0.35)/* * Color.a * input.color.a*/);// *(1.0 - keyMatch);

	float fresnel = (((1.0 - abs(dot(WorldCamera[2].xyz,input.normal)))));

	//color = lerp(color,Tint.rgb*input.color.rgb,(0.25 + 0.75 * fresnel) * Tint.a);
	//return float4(input.normal, 1.0);


	return float4(color * Tint.rgb, fresnel * Tint.a * clipFade);
}


