/*
Shader used to render the accessibility mesh around the selected vehicle. Uses triangles only.
Texcoord0.x is used to indicate an internal edge (0=center of cell, 1=outer most edge pixel)
TexCoord0.y is used to indicate a hard edge to either no cell at all or one of increased round number (0=center of cell/internal edge, 1=outer most edge pixel on a hard edge)
Typically TexCoord0.y <= TexCoord0.x
*/

#include "common.inc.hlsl"
//#include "shadow.hlsli"
#include "light.hlsli"



struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 edge : TEXCOORD0;
	float4 color: COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 normal : Normal;
	float4 color : COLOR;
	float3 world : World;
	//float4 atmosphere : Atmosphere;
	float2 edge : Edge;
};



void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal;
	output.world = input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color ;
	//output.atmosphere = CalculatePositionFog(output.world);
	output.edge = input.edge;
}

float4 psMain(V2P input) : SV_TARGET
{
	float stepX = max(abs(ddx(input.edge.x)), abs(ddy(input.edge.x)));
	float step2 = max(abs(ddx(input.edge.y)), abs(ddy(input.edge.y)));
	float hardEdgeOpacity = smoothstep(0.9 - step2, 0.9 + step2, abs(input.edge.y));
	float whiteEdgeOpacity2 = smoothstep(0.8 - step2, 0.8 + step2, abs(input.edge.y));

	//float edgeX = smoothstep(0.85 - stepX, 0.85 + stepX, abs(input.edge.x));

	//float c= 1.0 - hardEdgeOpacity;
	
	float diffuse = max(0.0, dot(input.normal, Sun.xyz))* 0.5 + 0.5;
	
	float3 rgb = input.color.rgb;
	float3 light = rgb * diffuse;
	
	float a = input.color.a;// - whiteEdgeOpacity2;
	a *= whiteEdgeOpacity2*2.0 + 1.0;
	//a = smoothstep(0.1,0.2,a);
	clip(a-0.0001);
	
	light *= (1.0 - whiteEdgeOpacity2)*0.5 + 0.5;
	light += hardEdgeOpacity*0.5;
	
	//light = (float3)whiteEdgeOpacity2;
	//a = 1.0;

	return float4(light, a);
}
