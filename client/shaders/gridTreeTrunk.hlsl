#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer Time
{
	float	Time;
};

struct A2G
{
	float3 center : Center;
	float2 height_radius : Height_Radius;
};

struct G2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float2 ext : Extent;
	float radius : Radius;
	float3 center : Center;
	float3 up : Up;
//	float3 normal : Normal;
//	float4 atmosphere : Atmosphere;
};



void vsMain(A2G input, out A2G output)
{
	output = input;
}


G2P MakeVertex(A2G input, float3 up, float u, float v)
{
	G2P output;
	output.ext = float2(u,v);
	output.world = input.center + WorldCamera[0].xyz * u * input.height_radius.y + up * v * input.height_radius.x;
	output.position = TransformAndProject(output.world);
	output.radius = input.height_radius.y;
	output.center = input.center;
	output.up = up;
	return  output;
}

[maxvertexcount (6)]
void gsMain( point A2G input[1], inout TriangleStream<G2P> stream)
{
	G2P a,b,c,d;
	float3 up = normalize(input[0].center);
	a = MakeVertex(input, up, -1,-0.1);
	b = MakeVertex(input, up, 1,-0.1);
	c = MakeVertex(input, up, 1,1);
	d = MakeVertex(input, up, -1,1);
	stream.Append(a);
	stream.Append(b);
	stream.Append(c);

	stream.RestartStrip();

	stream.Append(a);
	stream.Append(c);
	stream.Append(d);

	stream.RestartStrip();
}

float4 psMain(G2P input, out float outDepth: SV_Depth) : SV_TARGET
{
	float3 eye = WorldCamera[3].xyz;
	float3 center = input.center;

	float3 ray = input.world - eye;


	float3 rayDUpRay=ray-dot(ray,input.up)*input.up;
	float3 dP = eye - center;
	float3 dPDUp = dP-dot(dP,input.up)*input.up;
	float A = sqr(rayDUpRay);
	float B = 2.0 * dot(rayDUpRay, dPDUp);
	float C = sqr(dPDUp) - sqr(input.radius);
	//(rayDUpRay + dPDUp)2 = r2
	float4 rs = (float4)0.0;
	outDepth = 0.0;
	if (A != 0.0)
	{
		float _p = B/A;
		float _q = C/A;

		float sq2 = max(sqr(_p*0.5)-_q,0.0);
		float sq = sqrt(sq2);
		float s0 = -_p*0.5 + sq;
		float s1 = -_p*0.5 - sq;
		float x = min(s0,s1);
		float3 p = eye + ray*x;

		float3 ot = p - input.center;
		float3 normal=normalize(ot-dot(ot,input.up)*input.up);


		float diffuse = dot(normal,Sun.xyz);
		float lit = Shade(p);
		float light = diffuse > 0 ? lit*diffuse : 0.0;
		light = 0.6 * light + 0.4;

		float4 projected = TransformAndProject(p);
		outDepth = projected.z / projected.w;
		rs = float4(light,light,light,1.0);
	}
	else
		clip(-1.0);


	return rs;
	
}


