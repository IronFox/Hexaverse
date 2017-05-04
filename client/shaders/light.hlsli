#ifndef lightHLSLI
#define lightHLSLI

cbuffer Sun
{
	float4	Sun;
};

cbuffer SunColor
{
	float4	SunColor;
};




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




#endif /*lightHLSLI*/

