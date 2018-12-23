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
cbuffer MouseAt_FadeRange
{
	float2 	MouseAt;
	float	ShapeCenter, FadeStretch;
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
	//delta.y *= Viewport.x / Viewport.y;
	float rand0 = fmod(input.position.x*13.92374 + input.position.y*17.239478,1.0);
	float rand2 = fmod(input.position.x*54.169148 + input.position.y*25.784247,1.0);
	float rand3 = fmod(input.position.x*36.702146 + input.position.y*89.742514,1.0);
	rs.a *= smoothstep(0.0,1.0, 1.0 - abs(input.world.x - ShapeCenter) / FadeStretch * (0.7+0.1*(rand0+rand2+rand3)));
//	(0.5 - abs(0.5 - input.uv.x)*0.5)*2.0);

	float rand = fmod(input.position.x*18.5902374 + input.position.y*11.951932,1.0);
	float rand1 = fmod(input.position.x*14.23987 + input.position.y*14.569871,1.0);
	float rand4 = fmod(input.position.x*38.106577 + input.position.y*89.32154,1.0);

	float lightDist = dot(delta,delta);
	lightDist *= (rand1 * 0.4 + 0.8);
	float distAlpha = (1.0 / (1.0 + 50.0 * lightDist));
	distAlpha *= ((rand+rand4) * 0.1 + 0.8);
	rs.rgb *= 0.8 + 0.3 * distAlpha;
	
	rs.a = saturate(rs.a);
	rs.rgb *= max(1.0,rs.a);
	
	tex.rgb *= (1.0 - rs.a) * (1.0 - rs.a);
	rs.rgb = lerp(tex.rgb,rs.rgb,rs.a);
	//rs.rgb = tex.rgb * (1.0 - rs.a) + rs.rgb;
	rs.a = 1.0;
	return rs;
	//return float4(input.uv,1,1);
}


