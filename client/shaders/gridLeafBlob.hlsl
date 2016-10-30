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
	float2 ext : Extent;
	float radius : Radius;
	float3 center : Center;
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
	output.ext = float2(u,v);
	output.world = input.center + WorldCamera[0].xyz * u * input.radius_windSensitivity.x + WorldCamera[1].xyz * v * input.radius_windSensitivity.x;
	output.position = TransformAndProject(output.world);
	output.radius = input.radius_windSensitivity.x;
	output.center = input.center;
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

float4 psMain(G2P input, out float outDepth: SV_Depth) : SV_TARGET
{
	float r2 = sqr(input.ext);
	clip(1.0 - r2);
	float r = sqrt(r2);
	float opacity = 1.0;//-smoothstep(0.95,1.0,r);
	
	float3 eye = WorldCamera[3].xyz;
	float3 center = input.center;

	float3 ray = input.world - eye;
	float3 d = eye-center;

	//(eye + ray * x - center)^2 = r^2
	//d = eye-center
	//(d+ray*x)^2 = r^2
	//d^2+2*x*ray*d+(ray*x)^2 = r^2
	//x^2+2*x*ray*d/ray2+d^2/ray2-r^2/ray2=0
	//p=2*ray*d/ray2
	//q=(d^2-r^2)/ray2
	//s = -p/2 +- SQRT((p/2)^2-q)

	float ray2 = sqr(ray);
	float _p = 2.0*dot(ray,d)/ray2;
	float _q = (sqr(d)-sqr(input.radius))/ray2;
	float sq2 = max(sqr(_p*0.5)-_q,0.0);
	float sq = sqrt(sq2);
	float s0 = -_p*0.5 + sq;
	float s1 = -_p*0.5 - sq;
	float x = min(s0,s1);
	float3 p = eye + ray*x;
	
	//float h = sqrt(max(0.0,1-r2));
	//float2 xy = input.ext;
	//if (sqr(xy) < 0.001)
	//	xy.x = 0.001;
	float3 normal = normalize(p - input.center);
	//xy.x * WorldCamera[0].xyz + xy.y * WorldCamera[1].xyz);
	
	//float3 normal = WorldCamera[2].xyz * h + outward * r;
	float diffuse = dot(normal,Sun.xyz);
	//float3 p = input.center + (WorldCamera[2].xyz * h + input.ext.x * WorldCamera[0].xyz + input.ext.y * WorldCamera[1].xyz) * input.radius;
	//input.world + WorldCamera[2].xyz * h * input.radius;
	float lit = Shade(p);
	//if (r > 0.95)
	//	lit = ShadeUnfiltered(p);;
	float light = diffuse > 0 ? lit*diffuse : 0.0;
	light = 0.6 * light + 0.4;


	float4 projected = TransformAndProject(p);
	outDepth = projected.z / projected.w;

	return float4(light*0.1,light*0.8,light*0.2,opacity);
	
}


