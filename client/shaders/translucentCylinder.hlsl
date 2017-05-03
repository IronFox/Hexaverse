#include "common.inc.hlsl"
#include "shadow.hlsli"


cbuffer Tint
{
	float4 Tint;
};

cbuffer CutOffConfig
{
	float4 CutOffConfig;
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
	float3 init : Init;
	float3 object : Object;
	float height : Height;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.init = input.position;
	output.object = input.position;
	output.object.xy *= lerp(CutOffConfig.x, CutOffConfig.y,input.position.z);
	output.object.z *= CutOffConfig.z;
	output.world = mul(ObjectMatrix,float4(output.object,1.0)).xyz;

	//adjust normal here to inclination etc
	float3 normal = input.normal;
	normal.z = (CutOffConfig.x - CutOffConfig.y) / CutOffConfig.z;
	normal.xy = normalize(normal.xy)* sqrt(1.0 - sqr(normal.z));

	output.normal = (mul(ObjectMatrix,float4(normal,0.0)).xyz);
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color /** SunColor*/;
	output.height = length(output.world);
}

float4 psMain(V2P input) : SV_TARGET
{
	const float fadeRange = 0.25;
	float clipFade = (CutOffConfig.w - input.height) / fadeRange;
	clip(clipFade);
	clipFade = min(clipFade, 1.0);

	//return float4(input.normal, 1.0);
	//clip(CutOffConfig.w - input.height);

	//return float4((float3)Shade(input.world),1.0);
	float3 normal = normalize(input.normal);
	float3 reflected = normalize(reflect(input.world - WorldCamera[3].xyz, normal));
	float diffuse = max(0.0, dot(normal, Sun.xyz))* (/*KeyColor.a * */input.color.a * /*Shade(input.world) **/ 0.8 + 0.2);
	float ambient = /*KeyColor.a * */input.color.a * 0.7 + 0.3;
	//float specular = pow(max(0.0, dot(reflected, normalize((float3)1))),4);
	//color = ;
	//float toneDown = sqr(saturate(1.0 - input.object.z*2.0));
	//ambient *= (1.0 - toneDown*0.5);
//	float keyMatch = smoothstep(0.9,1.0,input.color.r - input.color.g - input.color.b);
	float3 color = (input.color.rgb * (diffuse*0.65 + ambient*0.35)/* * Color.a * input.color.a*/);


	float fresnel = (((1.0 - abs(dot(WorldCamera[2].xyz,normal)))));

	//fresnel *= 1.0 - abs(input.init.z-0.5)*2.0;
	//color *= 1.0 - abs(input.init.z-0.5);

	//color = lerp(color,Tint.rgb*input.color.rgb,(0.25 + 0.75 * fresnel) * Tint.a);

	return float4(color * Tint.rgb, fresnel * Tint.a * clipFade);
}


