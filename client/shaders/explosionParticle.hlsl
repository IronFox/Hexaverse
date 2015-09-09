#include "common.inc.hlsl"



Texture2D <float4> Texture;
SamplerState Sampler;


struct A2V
{
	float4 positionSize : PositionSize;
	float4 color : Color;
};


struct V2G
{
	float4 positionSize : PositionSize;
	float4 color : Color;
};

struct G2P
{
	float4 position : SV_POSITION;
	float2 uv : UV;
	float4 color : Color;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2G output)
{
	output.positionSize = input.positionSize;
	output.color = input.color;
}



[maxvertexcount (6)]
void gsMain(inout TriangleStream<G2P> stream, point V2G input[1])
{
	G2P output;
	output.color = input[0].color;

	float size = 0.02;
	float3	x = WorldCamera[0].xyz*input[0].positionSize.w,
			y = WorldCamera[1].xyz*input[0].positionSize.w;

	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].positionSize.xyz - x - y);
	stream.Append(output);
	output.uv = float2(1,0);
	output.position = TransformAndProject(input[0].positionSize.xyz + x - y);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].positionSize.xyz + x + y);
	stream.Append(output);
	stream.RestartStrip();


	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].positionSize.xyz - x - y);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].positionSize.xyz + x + y);
	stream.Append(output);
	output.uv = float2(0,1);
	output.position = TransformAndProject(input[0].positionSize.xyz - x + y);
	stream.Append(output);
	stream.RestartStrip();
}


float4 psMain(G2P input) : SV_TARGET
{
	float4 texel = Texture.Sample(Sampler,input.uv);
	return float4(input.color.rgb,input.color.a * texel.a);
	return texel * input.color;
}


