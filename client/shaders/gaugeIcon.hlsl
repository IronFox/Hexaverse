#include "common.inc.hlsl"


//cbuffer Center
//{
//	float4 Center;
//};
cbuffer Color
{
	float4 Color;
};


Texture2D <float4> Texture;
SamplerState Sampler;



struct G2P
{
	float4 position : SV_POSITION;
	float3 uv : UV;
};

struct V2G
{
	float4 position : SV_POSITION;
};


void vsMain(out V2G output)
{
	output.position = (float4)0.0;
}

float4 Transform(float3 w)
{
	float4 p = float4(w,1.0);
	p = mul(ObjectMatrix,p);

	return TransformAndProject(p.xyz);
}

[maxvertexcount (6)]
void gsMain(inout TriangleStream<G2P> stream, point V2G input[1])
{
	

	float3	x = WorldCamera[0].xyz *Color.a ,
			y = WorldCamera[1].xyz * Color.a;


	G2P output;
	output.uv = float3(0.0,0.0,0.0);
	output.position = Transform(float3(0,0,0));
	stream.Append(output);
	output.uv = float3(1.0,0.0,0.0);
	output.position = Transform(x);
	stream.Append(output);
	output.uv = float3(1.0,1.0,0.0);
	output.position = Transform(x + y);
	stream.Append(output);
	stream.RestartStrip();

	output.uv = float3(0.0,0.0,0.0);
	output.position = Transform((float3)0);
	stream.Append(output);
	output.uv = float3(1.0,1.0,0.0);
	output.position = Transform(x + y);
	stream.Append(output);
	output.uv = float3(0.0,1.0,0.0);
	output.position = Transform(y);
	stream.Append(output);
	stream.RestartStrip();

}



float4 psMain(G2P input) : SV_TARGET
{
	//float4 s = Texture.Sample(Sampler,input.uv.xy);
	//return float4(Color.r,Color.g,Color.b,s.r);

	float4 s = Texture.Sample(Sampler,input.uv.xy);
	float4 low = Texture.SampleBias(Sampler,input.uv.xy, 1.0);
	low.r = smoothstep(0.3,0.5,low.r);
	float3 color = Color.rgb;
	color = lerp((float3)0.0,color,s.r);
	return float4(color,max(low.r,s.r));
}


