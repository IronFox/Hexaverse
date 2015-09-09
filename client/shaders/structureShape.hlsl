/*
Shader used to render the normal surfaces of structures (buildings)
*/

#include "common.inc.hlsl"
#include "shadow.hlsli"


cbuffer KeyColor
{
	float4 KeyColor;
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
	float4 atmosphere : Atmosphere;
	//float height : Height;
};



void vsMain(A2V input, out V2P output)
{
	output.normal = normalize(mul(ObjectMatrix,float4(input.normal,0.0)).xyz);
	output.object = input.position;
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color ;
	output.atmosphere = CalculatePositionFog(output.world);

}

float4 psMain(V2P input) : SV_TARGET
{
	float3 reflected = normalize(reflect(input.world - WorldCamera[3].xyz, input.normal));
	float diffuse = max(0.0, dot(input.normal, Sun.xyz))* (KeyColor.a * input.color.a * Shade(input.world) * 0.8 + 0.2);
	float ambient = KeyColor.a * input.color.a * 0.7 + 0.3;
	float toneDown = sqr(saturate(1.0 - input.object.z*2.0));
	ambient *= (1.0 - toneDown*0.5);
	float keyMatch = smoothstep(0.9,1.0,input.color.r - input.color.g - input.color.b);
	float3 color = input.color.rgb * (diffuse*0.65 + ambient*0.35) * SunColor.rgb/* * Color.a * input.color.a*/;

	float fresnel = (((1.0 - abs(dot(WorldCamera[2].xyz,input.normal)))));

	color = lerp(color,Tint.rgb*input.color.rgb,(0.25 + 0.75 * fresnel) * Tint.a);

	color.rgb *= 1.0 - keyMatch;

	color.rgb = lerp(color.rgb,input.atmosphere.rgb,input.atmosphere.a);


	//color.rgb = lerp(color.rgb, KeyColor.rgb, keyMatch);

	color.rgb += KeyColor.rgb * keyMatch;


	return float4(color, 1.0);
}

	//float3 reflected = normalize(reflect(input.world - WorldCamera[3].xyz, input.normal));
	//float diffuse = max(0.0, dot(input.normal, Sun.xyz))* (KeyColor.a * input.color.a * Shade(input.world) * 0.8 + 0.2);
	//float ambient = KeyColor.a * input.color.a * 0.7 + 0.3;
	//float toneDown = sqr(saturate(1.0 - input.object.z*2.0));
	//ambient *= (1.0 - toneDown*0.5);
	//float keyMatch = smoothstep(0.9,1.0,input.color.r - input.color.g - input.color.b);
	//keyMatch = 1.0;
	//float3 color = lerp(input.color.rgb * (diffuse*0.65 + ambient*0.35) * SunColor.rgb/* * Color.a * input.color.a*/, KeyColor.rgb, keyMatch);


	//float fresnel = (((1.0 - abs(dot(WorldCamera[2].xyz,input.normal)))));


	//color = lerp(color,Tint.rgb*input.color.rgb,(0.25 + 0.75 * fresnel) * Tint.a);

	//return float4(color, 1.0);



