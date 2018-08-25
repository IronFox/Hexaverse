#include "common.inc.hlsl"


cbuffer TopColor
{
	float4 TopColor;
}
cbuffer BottomColor
{
	float4 BottomColor;
}
cbuffer Viewport
{
	float4 Viewport;
}
cbuffer MouseAt
{
	float2 MouseAt;
}

Texture2D<float4> BackgroundTexture;
SamplerState BackgroundSampler;


struct A2V
{
	float4 position_uv : POSITION;
};


struct V2P
{
	float4 position : SV_POSITION;	
	float2 world : World;
	float2 uv : UV;
};



void vsMain(in A2V input, out V2P output)
{
	output.position = TransformAndProject(float3(input.position_uv.xy,0));
	output.world = input.position_uv.xy;
	output.uv = input.position_uv.zw;
}

float4 psMain(V2P input) : SV_TARGET
{
	float2 uv = input.position.xy * Viewport.xy;
	uv += Viewport.zw;
	float4 tex = BackgroundTexture.Sample(BackgroundSampler,uv);
	
	float4 rs = lerp(BottomColor,TopColor,input.uv.y);
	float2 delta = MouseAt - input.world;
	rs.a *= smoothstep(0.0,1.0,(0.5 - abs(0.5 - input.uv.x)*0.5)*2.0);
	rs.rgb *= 0.8 + 0.3 * (1.0 / (1.0 + 50.0 * dot(delta,delta)));
	
	rs.a = saturate(rs.a);
	rs.rgb *= max(1.0,rs.a);
	
	tex.rgb *= (1.0 - rs.a) * (1.0 - rs.a);
	rs.rgb = lerp(tex.rgb,rs.rgb,rs.a);
	//rs.rgb = tex.rgb * (1.0 - rs.a) + rs.rgb;
	rs.a = 1.0;
	return rs;
	//return float4(input.uv,1,1);
}


