#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer Time
{
	float	Time;
};

struct A2G
{
	float3 center : Center;
	float2 radius_windSensitivity : Radius_WindSensitivity;
};

struct G2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float2 texcoords : Texcoords;
//	float3 normal : Normal;
//	float4 atmosphere : Atmosphere;
};



void vsMain(A2G input, out A2G output)
{
	output = input;
}


G2P MakeVertex(A2G input, float u, float v)
{
	G2P output;
	output.texcoords = float2(u,v)*0.5 + 0.5;
	output.world = input.center + WorldCamera[0].xyz * u * input.radius_windSensitivity.x + WorldCamera[1].xyz * v * input.radius_windSensitivity.y;
	output.position = TransformAndProject(output.world);
	return  output;
}

[maxvertexcount (6)]
void gsMain( point A2G input[1], inout TriangleStream<G2P> stream)
{
	G2P a,b,c,d;

	a = MakeVertex(input, -1,-1);
	b = MakeVertex(input, 1,-1);
	c = MakeVertex(input, 1,1);
	d = MakeVertex(input, -1,1);
	stream.Append(a);
	stream.Append(b);
	stream.Append(c);

	stream.RestartStrip();

	stream.Append(a);
	stream.Append(c);
	stream.Append(d);

	stream.RestartStrip();
}

float4 psMain(G2P input) : SV_TARGET
{
	return (float4)1.0;
	
}


