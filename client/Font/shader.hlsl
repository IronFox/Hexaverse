#include "../shaders/common.inc.hlsl"

//FONT SHADER


cbuffer Color
{
	float4		Color;
};

Texture2D <float4>	FontAtlas;
SamplerState		FontSampler;

struct A2V
{
	float4 texcoords : Texcoords;
	float4 attributes : Attributes;
};




struct V2G
{
	float4	ll_p : AP,	//lower left	projected position
			lr_p : BP,	//lower right
			ur_p : CP,	//upper right
			ul_p : DP;	//upper left
	float2	ll_c : AC,	//texcoords
			lr_c : BC,
			ur_c : CC,
			ul_c : DC;
};


struct G2P
{
	float2	texcoords : Texcoords;
	float4	position : SV_POSITION;
};



float4 Transform(float x, float y)
{
	float4 p = float4(x,y,0.0,1.0);
	p = mul(ObjectMatrix,p);

	return TransformAndProject(p.xyz);
}

void vsMain(A2V input, out V2G output)
{
	//input.texcoords.yw = 1.0 - input.texcoords.yw;
	output.ll_c = input.texcoords.xy;
	output.lr_c = input.texcoords.zy;
	output.ur_c = input.texcoords.zw;
	output.ul_c = input.texcoords.xw;

	output.ll_p = Transform(input.attributes.z,input.attributes.w);
	output.lr_p = Transform(input.attributes.z + input.attributes.x,input.attributes.w);
	output.ur_p = Transform(input.attributes.z + input.attributes.x,input.attributes.w + input.attributes.y);
	output.ul_p = Transform(input.attributes.z,input.attributes.w + input.attributes.y);
}


[maxvertexcount (6)]
void gsMain( point V2G input[1], inout TriangleStream<G2P> stream)
{
	G2P output;


	output.texcoords = input[0].ll_c;
	output.position = input[0].ll_p;
	stream.Append(output);

	output.texcoords = input[0].lr_c;
	output.position = input[0].lr_p;
	stream.Append(output);

	output.texcoords = input[0].ur_c;
	output.position = input[0].ur_p;
	stream.Append(output);

	stream.RestartStrip();

	output.texcoords = input[0].ll_c;
	output.position = input[0].ll_p;
	stream.Append(output);

	output.texcoords = input[0].ur_c;
	output.position = input[0].ur_p;
	stream.Append(output);

	output.texcoords = input[0].ul_c;
	output.position = input[0].ul_p;
	stream.Append(output);

	stream.RestartStrip();
}



float4 psMain(G2P input) : SV_TARGET
{
	//return float4(input.texcoords,0.0,1.0);
		//float4((float3)1,0.2);
	return FontAtlas.Sample(FontSampler,input.texcoords) * Color;
}
