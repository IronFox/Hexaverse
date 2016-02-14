#include "common.inc.hlsl"
#include "shadow.hlsli"

cbuffer Tint
{
	float4 Tint;
};

cbuffer Time
{
	float Time;
};

Texture2D <float> SmokeTexture;
SamplerState SmokeSampler;


struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 normal : Normal;
	float4 color : COLOR;
	float3 world : World;
};


void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal;
	output.world = input.position;
	//input.position;
	output.position = TransformAndProject(output.world);
	output.color = input.color;
}

float4 psMain(V2P input) : SV_TARGET
{
	float2 uv = input.color.ga;
	float dist = sqr((uv - 0.5) * 2.0);
	float lengthOpacity = 1.0 - sqr(sqr((input.color.b-0.5) * 2.0));
	//if (uv.x > 0.25 && uv.x < 0.75)
	//	discard;
	float tex = SmokeTexture.Sample(SmokeSampler,float2((uv.x+uv.y)*0.5, input.color.b));
//	- Time*0.01);	//time does seem kind of stupid after thinking about for a while longer
	//tex = smoothstep(0.1,0.6,tex);
	tex = 0.1 + 0.9 * sqr(tex);
	float light = max(dot(Sun.xyz,input.normal),0.0)*0.75 + 0.25;
	return float4(lerp((float3)light, Tint.rgb, Tint.a),tex * sqr(sqr(sqr(sqr(1.0 - dist)))) * lengthOpacity);
}


