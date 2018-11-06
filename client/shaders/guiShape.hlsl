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


float ExteriorEdge(float2 c, float2 b, float2 d)
{
	return dot(c - b, d) / length(d);
}

float InteriorEdge(float2 c, float2 b, float2 sep, float2 d0, float2 d1)
{
	c-= b;
	float i = dot(c,sep);
	if (i < 0)
		return dot(c,d0) / length(d0);
	return dot(c,d1) / length(d1);
}


static const float R = 50.0;	//hex radius


float GetHexDist(V2P input)
{
	
	static const float Z = 0.5 * R;
	static const float H = 0.86603 * R;
	
	float2 c = fmod(input.position.xy,float2(Z*6 ,H*2));
	if (c.y > H)
		c.y = H*2 - c.y;
	
	float dist = 0;
	int sector = (int)floor(c.x / Z);
	switch (sector)
	{
		case 0:
			dist = ExteriorEdge(c,float2(Z,0), float2(H,Z));
			break;
		case 1:
			dist = InteriorEdge(c,float2(Z,0), float2(H,-Z), float2(H,Z), float2(0,1));
			break;
		case 2:
			dist = InteriorEdge(c,float2(Z+R,0), float2(-H,-Z), float2(-H,Z), float2(0,1));
			break;
		case 3:
			dist = ExteriorEdge(c,float2(Z+R,0),float2(-H,Z));
			break;
		case 4:
			dist = InteriorEdge(c,float2(2*R,H), float2(H,Z), float2(-H,Z), float2(0,1));
			break;
		case 5:
			dist = InteriorEdge(c,float2(3*R,H), float2(-H,Z), float2(H,Z), float2(0,1));
			break;
	}
	
	//dist /= R;
	return dist;
}

float4 psMain(V2P input) : SV_TARGET
{
	float4 rs = lerp(BottomColor,TopColor,input.uv.y);
	
	float dist = GetHexDist(input);
	float hexDark = (1.0 - smoothstep(2,6,abs(dist)));	//for R=50
	//float hexDark = (1.0 - smoothstep(1,4,abs(dist))*rs.a);		//for R=12
	float2 normal = float2(ddx(hexDark),-ddy(hexDark));
	
	float2 delta = MouseAt-normal*0.5 - input.world;
	
	float mouseProximity = (1.0 / (1.0 + 50.0 * dot(delta,delta)));
	float hexDarken = hexDark * (0.45 + 0.55 * mouseProximity);
	
	float2 uv = input.position.xy * Viewport.xy;
	uv += Viewport.zw;
	
	
	
	float4 tex = BackgroundTexture.Sample(BackgroundSampler,uv);
	
	rs.a *= smoothstep(0.0,1.0, 1.0 - abs(input.world.x - ShapeCenter) / FadeStretch);
//	(0.5 - abs(0.5 - input.uv.x)*0.5)*2.0);
	rs.rgb *= 0.8 + 0.3 * mouseProximity;
	
	rs.rgb *= 1.0 - 0.05 * hexDarken;
	
	rs.a = saturate(rs.a);
	//rs.rgb *= max(1.0,rs.a);
	
	tex.rgb *= (1.0 - rs.a) * (1.0 - rs.a);
	rs.rgb = lerp(tex.rgb,rs.rgb,rs.a);
	//rs.rgb = tex.rgb * (1.0 - rs.a) + rs.rgb;
	rs.a = 1.0;
	return rs;
	//return float4(input.uv,1,1);
}


