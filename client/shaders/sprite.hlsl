#include "common.inc.hlsl"


cbuffer Time
{
	float	Time;
};

Texture2D <float> Texture;
SamplerState Sampler;


struct A2G
{
	float3 position : Position;
	float2 size : Size;
	float3 color : Color;
	float3 pulse : Pulse;
};



struct G2P
{
	float4 position : SV_POSITION;
	float2 uv : UV;
	float3 color : Color;
	float3 pulse : Pulse;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2G input, out A2G output)
{
	output = input;
}



[maxvertexcount (6)]
void gsMain(inout TriangleStream<G2P> stream, point A2G input[1])
{
	G2P output;
	output.color = input[0].color;
	output.pulse = input[0].pulse;

	float size = 0.02;
	float3	x = WorldCamera[0].xyz*input[0].size.x,
			y = WorldCamera[1].xyz*input[0].size.y;

	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].position - x - y);
	stream.Append(output);
	output.uv = float2(1,0);
	output.position = TransformAndProject(input[0].position + x - y);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].position + x + y);
	stream.Append(output);
	stream.RestartStrip();


	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].position - x - y);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].position + x + y);
	stream.Append(output);
	output.uv = float2(0,1);
	output.position = TransformAndProject(input[0].position - x + y);
	stream.Append(output);
	stream.RestartStrip();
}


float4 psMain(G2P input) : SV_TARGET
{
	float texel = Texture.SampleBias(Sampler,input.uv,0.5);
	float4 result;
	result.rgb = input.color * lerp(input.pulse.x,input.pulse.y,0.5 + 0.5 * sin(input.pulse.z*Time ) );
	result.a = texel;
	float step = max(abs(ddx(result.a)), abs(ddy(result.a)));
	float cutoff = 0.4;
	#ifdef CLIPPED
		clip(result.a-cutoff);
		result.a = 1.0;
	#else
		float opacity = (smoothstep(max(0.0,cutoff-step*2.0), cutoff, result.a));// / (1.0 + 10.0 * step * step);
		float nonBlack = (smoothstep(max(0.0,0.5-step), min(1.0,0.5+step), result.a));// / (1.0 + 10.0 * step * step);
		float bright = 1.0 - (smoothstep(0.65-step, 0.65+step, result.a)) / (1.0 + 10.0 * step * step);
		result.rgb = (0.75 * result.rgb + 0.25 * bright) * nonBlack;
		result.a = opacity;
	#endif
	return result;
}


