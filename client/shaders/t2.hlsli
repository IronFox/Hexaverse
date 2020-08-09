#include "shadow.hlsli"

float3 t2Color(float4 inputColor, float diffuse, float ambient)
{
	float3 c = inputColor.rgb*inputColor.rgb * 0.15;// *float3(0.5,0.2,0.2);
	c * (diffuse*0.65 + ambient*0.35) * SunColor.rgb/* * Color.a * input.color.a*/;
	return c;
}

float3 t2Fresnel(float3 color, float4 keyColor, float fresnel)
{
	return lerp(color,keyColor.rgb,fresnel*0.5);
}

float3 t2KeyColor(float3 color, float4 keyColor, float keyMatch)
{
	return color + (keyColor.rgb+0.1) * keyMatch*1.1;
}