#include "common.inc.hlsl"


cbuffer VisualCode
{
	float4 VisualCode;
};
cbuffer Center
{
	float4 Center;
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


float2 Radial(float cellIndex)
{
	float2 rs;
	sincos(cellIndex / 6.0 * Pi * 2.0, rs.x, rs.y);
	rs.x = -rs.x;
	return rs;
}

float2 PushTriangle(inout TriangleStream<G2P> stream, float2 last, float cellIndex)
{
	float2 next = Radial(cellIndex);
	G2P output;
	output.uv = float3(0.5,0.5,0.0);
	output.position = Project(Center);
	stream.Append(output);
	output.uv = float3(last * 0.5 + 0.5,1.0);
	output.uv.y = 1.0 - output.uv.y;
	output.position = Project(Center + float4(last*CELL_SIZE,0,0));
	stream.Append(output);
	output.uv = float3(next * 0.5 + 0.5,1.0);
	output.uv.y = 1.0 - output.uv.y;
	output.position = Project(Center + float4(next*CELL_SIZE,0,0));
	stream.Append(output);
	stream.RestartStrip();

	return next;
}

void vsMain(out V2G output)
{
	output.position = Center;
}

[maxvertexcount (18)]
void gsMain(inout TriangleStream<G2P> stream, point V2G input[1])
{
	

//	float3	x = WorldCamera[0].xyz*CELL_SIZE,
	//		y = WorldCamera[1].xyz*CELL_SIZE;

	float2 last = Radial(0);
	last = PushTriangle(stream, last , 1);
	last = PushTriangle(stream, last , 2);
	last = PushTriangle(stream, last , 3);
	last = PushTriangle(stream, last , 4);
	last = PushTriangle(stream, last , 5);
	last = PushTriangle(stream, last , 0);
}



float4 psMain(G2P input) : SV_TARGET
{
	float4 s = Texture.Sample(Sampler,input.uv.xy);
	float edge = smoothstep(0.85,0.89,input.uv.z);

	s *= (1.0 - edge);
	s += edge * max(VisualCode.r, VisualCode.g);
	float3 color = s.rgb;
	//(float3)s;
	//s.a = 1.0;
	//color = (float3)s.a;
	//s.a = 1.0;

	color *= lerp((float3)1.0,float3(0,0,1),sqr(1.0 - input.uv.y)*0.5);

	//color *= s.a;




	color *= lerp((float3)1.0, float3(1.0, 0.5, 0.5),VisualCode.r);


	//color *= lerp((float3)1.0, float3(0.25,0.25,1.0),smoothstep(0.65,0.89,input.uv.z) * VisualCode.g );
	color *= lerp((float3)1.0, float3(0.25,0.25,1.0),(sqr(input.uv.y)+ sqr(1.0 - input.uv.y))* VisualCode.g );


	color = lerp(color,(float3)0.5,VisualCode.b);



	//sqr(sqr(sqr(sqr(min(1.0,1.05*input.uv.z)))));
	//s = 1.0 - (1.0 - s) * (1.0 - edge*0.5);
	//g *= 1.0 - edge*0.75;
	float a = smoothstep(0.0,0.05,1.0 - input.uv.z) * (1.0 - VisualCode.b);
	//if (input.uv.z > 0.8)
		//g = 1.0 - g;
	return float4(color,(0.75+s.a*0.25)*a);
	//float4(input.uv,0.0,1.0);
}


