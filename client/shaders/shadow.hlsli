#include "light.hlsli"

#define CASCADING_SHADOW

#ifdef CASCADING_SHADOW
	cbuffer ShadowMatrix0
	{
		float4x4 ShadowMatrix0;
	};
	cbuffer ShadowMatrix1
	{
		float4x4 ShadowMatrix1;
	};
	Texture2D <float> ShadowMap0,ShadowMap1;

#else
	cbuffer ShadowMatrix
	{
		float4x4 ShadowMatrix;
	};
	Texture2D <float> ShadowMap;
#endif



SamplerState ShadowSampler;



#ifdef CASCADING_SHADOW
	static const float ShadowMapSize = 1024.0;
#else
	static const float ShadowMapSize = 4096.0;
#endif

static const float m_fTexelSize = 1.0 / ShadowMapSize;

static const float Tolerance = 0.0007;

float ShadeSample(float3 projCoords, float3 duvdist_dx, float3 duvdist_dy, Texture2D <float> shadowMap, float tolerance)
{
	if (Sun.w < 0.5)
		return 1.0;
//	return float(shadowMap.SampleLevel( ShadowSampler, (root + float2(x,y)) *m_fTexelSize,0.0) >= depthBase + ((ddist_duv.x * (float)x) + (ddist_duv.y * (float)y)) * m_fTexelSize) * impact;

	//Invert texture Jacobian and use chain rule to compute ddist/du and ddist/dv
	// |ddist/du| = |du/dx du/dy|-T * |ddist/dx|
	// |ddist/dv| |dv/dx dv/dy| |ddist/dy|
	//Multiply ddist/dx and ddist/dy by inverse transpose of Jacobian
	float div = ((duvdist_dx.x * duvdist_dy.y) - (duvdist_dx.y * duvdist_dy.x) );
	if (abs(div) < 0.000000001)
	{
		float dist = projCoords.z - Tolerance;// - (ddist_duv.x * (float) x) - (ddist_duv.y * (float) y);
		float s = shadowMap.SampleLevel( ShadowSampler,
								projCoords.xy,0.0);
		return float(s >= dist);
	}
	else
	{
		float invDet = 1.0 / div;
		//Top row of 2x2
		float2 ddist_duv;
		ddist_duv.x = duvdist_dy.y * duvdist_dx.z ; // invJtrans[0][0] * ddist_dx
		ddist_duv.x -= duvdist_dx.y * duvdist_dy.z ; // invJtrans[0][1] * ddist_dy
		//Bottom row of 2x2
		ddist_duv.y = duvdist_dx.x * duvdist_dy.z ; // invJtrans[1][1] * ddist_dy
		ddist_duv.y -= duvdist_dy.x * duvdist_dx.z ; // invJtrans[1][0] * ddist_dx
		ddist_duv *= invDet;

		float fPercentLit = 0.0;

	
		float2 texel0 = projCoords.xy * ShadowMapSize;
		//float2 texel = texel0;// + 0.5;
		float2 texel = texel0-0.5;
		float2 texelBase = floor(texel);
		float2 inter = texel - texelBase;

		static const int Range = SHADOW_FILTER_QUALITY;
			//6;

		//return 0.5 + 10.0*ddist_duv.x;

		float rootDist = float(Range/2-1);
		float2 root = texelBase-rootDist;
		float depthBase = projCoords.z - tolerance - ((ddist_duv.x * (rootDist + inter.x)) + (ddist_duv.y * (rootDist + inter.y))) * m_fTexelSize;

		//return abs(depthBase + ((ddist_duv.x * (rootDist + inter.x)) + (ddist_duv.y * (rootDist + inter.y))) * m_fTexelSize - (projCoords.z - Tolerance))*1000.0;


		float rs = 0.0;
		//float lightMatrix[16], lightMatrix2[12];


		//return float(ShadowMap.SampleLevel( ShadowSampler, (texelBase) *m_fTexelSize,0.0) >= depthBase);
		
		{
			for( int x = 0; x < Range; ++x )
			{
				float impactX = x == 0 ? 1.0 - inter.x : (x == Range-1? inter.x : 1.0);
				for( int y = 0; y < Range; ++y ) 
				{
					float impactY = y == 0 ? 1.0 - inter.y : (y == Range-1? inter.y : 1.0);
					float impact = impactX * impactY;
					float2 uv = (root + float2(x, y)) *m_fTexelSize;
					rs += float(shadowMap.SampleLevel( ShadowSampler, uv,0.0) >= depthBase + ((ddist_duv.x * (float)x) + (ddist_duv.y * (float)y)) * m_fTexelSize) * impact;
				}
			}
		}
	

		return rs / ((Range-1)*(Range-1));
	}
}

float UnfilteredShadeSample(float3 projCoords, Texture2D <float> shadowMap, float tolerance)
{
//	return float(shadowMap.SampleLevel( ShadowSampler, (root + float2(x,y)) *m_fTexelSize,0.0) >= depthBase + ((ddist_duv.x * (float)x) + (ddist_duv.y * (float)y)) * m_fTexelSize) * impact;
	return float(shadowMap.SampleLevel( ShadowSampler, projCoords.xy,0.0) >= projCoords.z - tolerance);
}

float Shade(float3 world)
{
	#if SHADOW_FILTER_QUALITY == 0
		return 1.0;
	#else

	#ifdef CASCADING_SHADOW
		float4 projected1 = mul(ShadowMatrix1,float4(world,1.0));
		float4 projected = mul(ShadowMatrix0,float4(world,1.0));
		projected.xyz /= projected.w;
		float2 xy = abs(projected.xy);


		#if SHADOW_FILTER_QUALITY > 1
			float3 duvdist_dx = ddx(projected.xyz);
			float3 duvdist_dy = ddy(projected.xyz);

			float3 duvdist1_dx = ddx(projected1.xyz);
			float3 duvdist1_dy = ddy(projected1.xyz);

			if (any(xy > 0.95))
			{
				projected1.xyz /= projected1.w;
				if (any(abs(projected1.xy)>1.0))
					return 1.0;
				projected1.y = -projected1.y;
				projected1.xy = projected1.xy * 0.5 + 0.5;
				float rs2 = ShadeSample(projected1.xyz, duvdist1_dx, duvdist1_dy, ShadowMap1, Tolerance * 4.0);

				if (any(abs(projected.xy)>1.0))
					return rs2;
				projected.y = -projected.y;
				projected.xy = projected.xy * 0.5 + 0.5;
				float rs1 = ShadeSample(projected.xyz, duvdist_dx, duvdist_dy, ShadowMap0, Tolerance*1.5);

				return lerp(rs1, rs2, smoothstep(0.95, 1.0, max(xy.x, xy.y)));
			}
			else
			{
				projected.y = -projected.y;
				projected.xy = projected.xy * 0.5 + 0.5;
				return ShadeSample(projected.xyz, duvdist_dx, duvdist_dy, ShadowMap0, Tolerance*1.5);
			}
		#else
			if (any(xy > 0.95))
			{
				projected1.xyz /= projected1.w;
				if (any(abs(projected1.xy)>1.0))
					return 1.0;
				projected1.y = -projected1.y;
				projected1.xy = projected1.xy * 0.5 + 0.5;
				float rs2 = UnfilteredShadeSample(projected1.xyz, ShadowMap1, Tolerance * 4.0);

				if (any(abs(projected.xy)>1.0))
					return rs2;
				projected.y = -projected.y;
				projected.xy = projected.xy * 0.5 + 0.5;
				float rs1 = UnfilteredShadeSample(projected.xyz, ShadowMap0, Tolerance*1.5);

				return lerp(rs1, rs2, smoothstep(0.95, 1.0, max(xy.x, xy.y)));
			}
			else
			{
				projected.y = -projected.y;
				projected.xy = projected.xy * 0.5 + 0.5;
				return UnfilteredShadeSample(projected.xyz, ShadowMap0, Tolerance*1.5);
			}
			
		#endif
	#else
		float4 projected = mul(ShadowMatrix,float4(world,1.0));
		projected.xyz /= projected.w;
		if (any(abs(projected.xy)>1.0))
			return 1.0;

		projected.y = -projected.y;
		projected.xy = projected.xy * 0.5 + 0.5;
		#if SHADOW_FILTER_QUALITY > 1
			float3 duvdist_dx = ddx(projected.xyz);
			float3 duvdist_dy = ddy(projected.xyz);

			return ShadeSample(projected.xyz, duvdist_dx, duvdist_dy, ShadowMap, Tolerance);
		#else
			return UnfilteredShadeSample(projected.xyz, ShadowMap, Tolerance);
		#endif
	#endif
	#endif
	//return 0;
	//return float(ShadowMap.SampleLevel(ShadowSampler,projected.xy + 0.5 * m_fTexelSize,0.0)+Tolerance > projected.z);
}

float ShadeUnfiltered(float3 world)
{
	#ifdef CASCADING_SHADOW
		float4 projected1 = mul(ShadowMatrix1,float4(world,1.0));
		float4 projected = mul(ShadowMatrix0,float4(world,1.0));
		projected.xyz /= projected.w;
		if (any(abs(projected.xy)>0.95))
		{
			float2 xy = abs(projected.xy);

			projected1.xyz /= projected1.w;
			if (any(abs(projected1.xy)>1.0))
				return 1.0;
			projected1.y = -projected1.y;
			projected1.xy = projected1.xy * 0.5 + 0.5;
			float rs2 = UnfilteredShadeSample(projected1.xyz, ShadowMap1, Tolerance * 3.0);

			if (any(abs(projected.xy)>1.0))
				return rs2;
			projected.y = -projected.y;
			projected.xy = projected.xy * 0.5 + 0.5;
			float rs1 = UnfilteredShadeSample(projected.xyz, ShadowMap0, Tolerance);

			return lerp(rs1,rs2,smoothstep(0.95,1.0,max(xy.x,xy.y)));
		}
		else
		{
			projected.y = -projected.y;
			projected.xy = projected.xy * 0.5 + 0.5;
			return UnfilteredShadeSample(projected.xyz, ShadowMap0, Tolerance);
		}

	#else
		float4 projected = mul(ShadowMatrix,float4(world,1.0));
		projected.xyz /= projected.w;
		if (any(abs(projected.xy)>1.0))
			return 1.0;

		projected.y = -projected.y;
		projected.xy = projected.xy * 0.5 + 0.5;
		return UnfilteredShadeSample(projected.xyz, ShadowMap, Tolerance);
	#endif
	//return 0;
	//return float(ShadowMap.SampleLevel(ShadowSampler,projected.xy + 0.5 * m_fTexelSize,0.0)+Tolerance > projected.z);
}

//float Light(float3 position, float3 normal)
//{
//	return (max(0.0,dot(Sun.xyz,normal)) * Shade(position) * 0.8 + 0.2);
//
//}


