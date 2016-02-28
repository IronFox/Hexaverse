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
					input.position.y * length(refVector)*5.0 - Time,
					input.position.x * 0.5 + 0.5,
					input.position.y
					);
//	output.uv = float2(input.position.z * length(ObjectMatrix[2].xyz)*5.0, input.position.x * 0.5 + 0.5);
}

float4 psMain(V2P input) : SV_TARGET
{
	float2 uv = float2(input.uvw.x,abs(0.5 - input.uvw.y)*2.0);

	float b = ArrowTex.SampleBias(ArrowTexSampler,uv,-1.0);

	b *= 1.0 - smoothstep(0.4,0.9,abs(input.uvw.z*2.0 - 1.0));
	return Tint * b;
}


