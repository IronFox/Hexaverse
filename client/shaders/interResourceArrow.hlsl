#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer Tint
{
	float4 Tint;
};

Texture2D<float> ArrowTex;
SamplerState ArrowTexSampler;



struct A2V
{
	float3 position : POSITION;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 uvw : UVW;
	//float height : Height;
};


cbuffer Time
{
	float	Time;
};


//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
//	output.normal = normalize(mul(ObjectMatrix, float4(input.normal,0.0) ).xyz);
	output.world = mul(ObjectMatrix,float4(input.position,1.0)).xyz;
	output.position = TransformAndProject(output.world);
	float3 refVector = float3(ObjectMatrix[0].y,ObjectMatrix[1].y,ObjectMatrix[2].y);
	//float3 refVector = ObjectMatrix[1].xyz;
	output.uvw = float3(
					input.position.y * length(refVector)*10.0,
					input.position.x * 0.5 + 0.5,
					input.position.y
					);
//	output.uv = float2(input.position.z * length(ObjectMatrix[2].xyz)*5.0, input.position.x * 0.5 + 0.5);
}

float4 psMain(V2P input) : SV_TARGET
{
	float2 uv0 = float2(input.uvw.x - Time,abs(0.25 - input.uvw.y)*2.0);
	float2 uv1 = float2(-input.uvw.x - Time,abs(0.75 - input.uvw.y)*2.0);
	float b0 = ArrowTex.SampleBias(ArrowTexSampler,uv0,-1.0);
	float b1 = ArrowTex.SampleBias(ArrowTexSampler,uv1,-1.0);
	float b = lerp(b0,b1,smoothstep(0.45,0.55,input.uvw.y));
	
	b = b * 0.5 + 0.5;
	b *= smoothstep(0.0,0.05,abs(input.uvw.y -0.5));
	//b *= 1.0 - smoothstep(0.4,0.9,sqr(input.uvw.z*2.0 - 1.0));
	return Tint * b;
}


