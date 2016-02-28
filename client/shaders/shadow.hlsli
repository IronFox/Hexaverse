
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


cbuffer Sun
{
	float4	Sun;
};

cbuffer SunColor
{
	float4	SunColor;
};






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

		static const int Range = 6;

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
	#ifdef CASCADING_SHADOW
		float4 projected1 = mul(ShadowMatrix1,float4(world,1.0));
		float4 projected = mul(ShadowMatrix0,float4(world,1.0));
		projected.xyz /= projected.w;
		float2 xy = abs(projected.xy);

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
		float4 projected = mul(ShadowMatrix,float4(world,1.0));
		projected.xyz /= projected.w;
		if (any(abs(projected.xy)>1.0))
			return 1.0;

		projected.y = -projected.y;
		projected.xy = projected.xy * 0.5 + 0.5;
		return ShadeSample(projected.xyz, ShadowMap, Tolerance);
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


cbuffer AtmosphereConfig
{
	/*
		r: Quadratic atmosphere radius
		g: linear atmosphere radius
	*/
	float4	AtmosphereConfig;
	float4	AtmosphereColor;


};


float2 GetIntersections(float3 centerToEye, float3 eyeDirN)
{
	//float ed2 = sqr(eyeDir);
	float p = 2.0 * dot(centerToEye,eyeDirN);
	float q = (sqr(centerToEye) - AtmosphereConfig.r);

	float s = sqr(p)-4.0*q;
	if (s < 0.0)
		return (float2)0;
	s = sqrt(s);
	return ((float2)(-p) + float2(-s,s)) * 0.5;
}


float4 GetIntersections2(float3 centerToEye, float3 eyeDirN)
{
	//float ed2 = sqr(eyeDir);
	float p = 2.0 * dot(centerToEye,eyeDirN);
	float q = (sqr(centerToEye) - AtmosphereConfig.r);

	float s = sqr(p)-4.0*q;
	if (s < 0.0)
		return (float4)0;
	s = sqrt(s);
	float2 intersections = ((float2)(-p) + float2(-s,s)) * 0.5;

	float horizonAt = clamp(-dot(centerToEye,Sun.xyz) / dot(eyeDirN,Sun.xyz), intersections.x,intersections.y);
	if (dot(Sun.xyz,eyeDirN) < 0.0)	//light first
	{
		return float4(intersections.x,horizonAt, horizonAt, intersections.y);	//light first
	}
	else  //light second
		return float4(horizonAt, intersections.y, intersections.x,horizonAt);	//light first

}


static const float atmosphereDensityConstant = 0.04,
					lowerAtmosphereVisibilityEdge = -0.1,
					upperAtmosphereVisibilityEdge = 0.4;



float EstimateLight(float x, float3 centerToEye, float3 eyeDirN)
{
	return dot(Sun.xyz,eyeDirN) * 0.5 * sqr(x) + dot(Sun.xyz,centerToEye) * x;

}

float2 CalculateFogOpacity(float3 worldPos, bool clampDistance)
{
	float3 direction = worldPos - WorldCamera[3].xyz;
	float distance = length(direction);
	float3 directionN = direction / distance;
	float4 intersections = max(GetIntersections2(WorldCamera[3].xyz, directionN), (float4)0);
	if (clampDistance)
		intersections = min(intersections,(float4)distance);
	float atmosphereTravelLight = intersections.y - intersections.x,
			atmosphereTravelDark = intersections.w - intersections.z;
	if (atmosphereTravelLight + atmosphereTravelDark < 0.001)
		return (float2)0.0;

	float radialDistance = min(1.0,sqr(cross(directionN,worldPos / AtmosphereConfig.g)));
	float factor = /*sqr*/(/*sqr*/(1.0 - radialDistance));
//Vector3.Cross(ray.direction, point - ray.origin).magnitude;


	float innerDensity = atmosphereDensityConstant * (atmosphereTravelLight + atmosphereTravelDark) * factor;
	float density = clamp(1.0-exp(-sqr(innerDensity)),0.0,1.0);	//applying some exponent here only pushes the fog back
	
//	if (intersections.y < intersections.x || intersections.w < intersections.z)
	//	clip(-1.0);

	float light = (EstimateLight(intersections.y,WorldCamera[3].xyz, directionN) - EstimateLight(intersections.x,WorldCamera[3].xyz, directionN)) 
					/ AtmosphereConfig.g / 40.0;
	light = saturate(light);
	//max(
					//max(max(0.0,dot(n0,Sun) * 0.9 + 0.1),
					 //max(0.0,dot(n1,Sun) * 0.9 + 0.1)),
					 //max(0.0,dot(n2,Sun) * 0.9 + 0.1));



	return  float2(density, light *0.6 + 0.4);
}


//static const float3 FogColor = float3(0.7,0.8,1.0);

float4 CalculateDirectionFog(float3 worldPos)
{
	float2 atmosphereVisibility = CalculateFogOpacity(worldPos,false);
	return float4(AtmosphereColor.rgb * atmosphereVisibility.g,atmosphereVisibility.r * AtmosphereColor.a);
}

float4 CalculatePositionFog(float3 worldPos)
{
//	clip(-1.0);
	//project worldPos to at most center planet plane. dont work. planet plane dont exist
	float2 atmosphereVisibility = CalculateFogOpacity(worldPos,true);
	return float4(AtmosphereColor.rgb * atmosphereVisibility.g,atmosphereVisibility.r * AtmosphereColor.a);
}



float3 CalculatePositionFog(float3 worldPos, float3 inColor)
{
//	clip(-1.0);
	//project worldPos to at most center planet plane. dont work. planet plane dont exist
	float2 atmosphereVisibility = CalculateFogOpacity(worldPos,true);
	if (atmosphereVisibility.x < 0.001)
		return inColor;
	return lerp(inColor,AtmosphereColor.rgb * atmosphereVisibility.g, atmosphereVisibility.r * AtmosphereColor.a);
}


