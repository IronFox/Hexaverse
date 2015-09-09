deprecated

#include "common.inc.hlsl"



cbuffer Config
{
	float4x4 Config;
};

struct A2V
{
	float4 position : POSITION;
};

struct V2G
{
	float4 position : POSITION;
};

struct G2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 normal : Normal;
	float2 opacity : Opacity;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2G output)
{
	output.position = (input.position);
}


void Put(inout TriangleStream<G2P> stream, float4 p, float3 n, float h,float opacity)
{
	G2P output = (G2P)0;
	output.normal = n;
	output.world = p.xyz * h;
	output.position = TransformAndProject(output.world);
	output.opacity = float2(p.w, opacity);
	stream.Append(output);

}

[maxvertexcount(9)]
void gsMain(inout TriangleStream<G2P> stream, triangle V2G input[3])
{
	//G2P output = (G2P)0;

	Put(stream, input[0].position, input[0].position.xyz, Config[0].y, 1);
	Put(stream, input[1].position, input[0].position.xyz, Config[0].y, 1);
	Put(stream, input[2].position, input[0].position.xyz, Config[0].y, 1);
	stream.RestartStrip();

	float3 n2 = normalize(cross(input[2].position.xyz - input[1].position.xyz, input[0].position.xyz));
	Put(stream, input[2].position, n2, Config[0].y, 1);
	Put(stream, input[1].position, n2, Config[0].y, 1);
	Put(stream, input[2].position, n2, Config[0].x, 0);
	stream.RestartStrip();

	Put(stream, input[1].position, n2, Config[0].y, 1);
	Put(stream, input[1].position, n2, Config[0].x, 0);
	Put(stream, input[2].position, n2, Config[0].x, 0);
	stream.RestartStrip();
}




float4 psMain(G2P input) : SV_TARGET
{
	float opacity = (input.opacity.x*input.opacity.x * 0.25 + 0.75) * (input.opacity.y * 0.5 + 0.5);
	return float4(opacity * Config[2].xyz * (max(0.0, dot(input.normal, normalize((float3)1)))*0.5 + 0.5), Config[2].a);
	//float4(0.7, 1.3, 0.8, smoothstep(0.8, 0.9, input.sourceOpacity));;
}


