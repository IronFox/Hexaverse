#include "common.inc.hlsl"
#include "shadow.hlsli"


cbuffer Time
{
	float	Time;
};

struct A2V
{
	float4 position3_edge : POSITION;
	float4 normal3_opacity : NORMAL;
	float emp : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float4 position3_edge : PTransfer;
	float4 normal3_opacity : NTransfer;
	float2 emp_fresnel : ETransfer;
	
};



void vsMain(A2V input, out V2P output)
{
	output.position3_edge = input.position3_edge;
	output.normal3_opacity = input.normal3_opacity;
	output.emp_fresnel.x = input.emp;
	output.emp_fresnel.y = dot(normalize(WorldCamera[3].xyz-output.position3_edge.xyz),input.normal3_opacity.xyz);
	output.position = TransformAndProject(output.position3_edge.xyz);
}

float4 psMain(V2P input) : SV_TARGET
{
	float edge = 1.0-input.position3_edge.w;
	float step = max(abs(ddx(edge)), abs(ddy(edge)));

	float centerGridOpacity = (1.0 - smoothstep(0.04 - step, 0.04 + step, abs(edge))) / (1.0 + 10.0 * step * step);
	float outerGridOpacity = (1.0 - smoothstep(0.04, 0.11 + step*2.0, abs(edge))) / (1.0 + 10.0 * step * step);

	

	float4 rs;// = float4((float3)input.position3_edge.w,input.normal3_opacity.w);
	//rs.a = 0.25 + 0.75 * rs.a;

	//rs.rgb = (float3)0.0;
	//lerp(rs.rgb,(float3)(0.5),outerGridOpacity);//outer grid is 0.5 grey
	rs.rgb = (float3)centerGridOpacity;
	//(centerGridOpacity)*0.75 + 0.25;	//center grid is black
	//float red = input.emp_fresnel.x * (sin(Time*8.f)*0.25 + 0.75);
	//rs.gb *= (1.0-red) * 0.75 +0.25;
	//rs.a = outerGridOpacity*0.5 + centerGridOpacity*0.5;
	rs.a = outerGridOpacity * 0.75;
	//rs.a *= sqr(max(0.0,input.emp_fresnel.y));
	//smoothstep(0.4,0.6,input.emp_fresnel.y);
	rs.a *= 0.25 + 0.75 * input.normal3_opacity.w;
	rs.a *= 1.0 - input.emp_fresnel.x;	//emp lets grid break off entirely
	//
	return rs;
}

