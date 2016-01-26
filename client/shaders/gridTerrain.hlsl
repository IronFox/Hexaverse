#include "common.inc.hlsl"
#include "shadow.hlsli"


#ifdef PARABOLOID_DISPLAY

	cbuffer ParaboloidDirection_Height
	{
		float4		ParaboloidDirection_Height;
	};
	cbuffer ParaboloidColor_FlatRadius
	{
		float4		ParaboloidColor_FlatRadius;
	};

#endif

#ifdef SPHERE_DISPLAY
	cbuffer SphereDef
	{
		float4		SphereDef;
	};
	cbuffer SphereColor
	{
		float4		SphereColor;
	};
#endif

TextureCube<float>	HeightMap;
SamplerState			HeightMapSampler;

cbuffer HeightMapOrientation
{
	float4x4	HeightMapOrientation0,
				HeightMapOrientation1,
				HeightMapOrientation2;
};

cbuffer Time
{
	float	Time;
};

struct A2V
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	#if defined(SPHERE_DISPLAY) || defined(PARABOLOID_DISPLAY)
		float radial : Radial;
	#endif
	float4 position : SV_POSITION;
	float3 world : World;
	float3 normal : Normal;
	float intensity : Intensity;
	float2 structureBlueshift : StructureBlueshift;
	float4 color : COLOR;
	float3 glowColor : GlowColor;
	float diffuse : Diffuse;
	float4 atmosphere : Atmosphere;
};



float4 Amplitude(float3 p, float frequency, float tfrequency, float offset, float dir)
{
	float amp = 1.0 / abs(frequency);
	return float4(	dir * (float3)sin((p*frequency+Time*tfrequency+offset)) *amp,
					amp);
}

float4 Amplitude(float3 p, float frequency, float offset)
{
	p.xyz += float3(-17.9837,79.334698,-127.0346);
	return Amplitude(p,frequency,0.7*frequency,offset,1.0) + Amplitude(p,frequency,-0.7*frequency,PI *1.0124125* offset,-1.0);
}

float SampleHeightMap(float3 p)
{
	return
		(
		HeightMap.SampleLevel(HeightMapSampler,p,0.0)
		*
		HeightMap.SampleLevel(HeightMapSampler,mul(HeightMapOrientation0,float4(p,1.0)).xyz,0.0)
		*
		HeightMap.SampleLevel(HeightMapSampler,mul(HeightMapOrientation1,float4(p,1.0)).xyz,0.0)
		*
		HeightMap.SampleLevel(HeightMapSampler,mul(HeightMapOrientation2,float4(p,1.0)).xyz,0.0)
		)
		;
}


void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal.xyz;
	output.world = input.position.xyz;
	output.position = TransformAndProject(output.world);
	output.intensity = input.position.w;
	output.color = max(input.color,(float4)0.0);
	output.glowColor = max(-input.color.rgb,(float3)0.0)*0.5;

	//lava glow:
	float3 seed = 
		float3(
			length(input.position.xy),
			length(input.position.xz),
			length(input.position.yz)

		);
	float4 comb4 = 
					(
						+ Amplitude(input.position.xyz,0.66,PI)
						+ Amplitude(seed,0.87,-8.1)
						+ Amplitude(input.position.xyz,0.31,PI*1.2354)
						+ Amplitude(seed,0.21,18.1)
					);

	float3 comb3 = comb4.xyz / comb4.w
					 * 0.5 + 0.5;
	float comb = comb3.x * comb3.y * comb3.z;
	comb = clamp(comb*2.0,0.0,1.0);
	//comb = 1.0 - sqr(1.0 - comb

	output.glowColor *= SampleHeightMap(input.position.xyz)*7.0 + 0.2;
	//output.glowColor *= sqr(sqr(SampleHeightMap(input.position.xyz)))*25.0*25.0*5.0 + 0.4;
	//comb*1.6+0.4;


	output.color.rgb *= SunColor.rgb;
	//output.color.rgb += output.glowColor;
	output.diffuse = max(0.0, dot(input.normal.xyz, Sun.xyz));
	output.structureBlueshift.x = input.normal.w;
	output.structureBlueshift.y = smoothstep(-0.4,0.3, -dot(normalize(output.world),Sun.xyz));
	output.atmosphere = CalculatePositionFog(output.world);

	#ifdef PARABOLOID_DISPLAY
		float3 center = ParaboloidDirection_Height.xyz * ParaboloidDirection_Height.w;
		float h2 = length(output.world);
		float3 world = (output.world/h2) * ParaboloidDirection_Height.w;
		float d = distance(center,world);

		float maxH = -0.5 / ParaboloidColor_FlatRadius.w * sqr(d) + ParaboloidColor_FlatRadius.w * 0.5;
		output.radial = maxH - (h2 - ParaboloidDirection_Height.w);
	#endif
	#ifdef SPHERE_DISPLAY
		float d = distance(SphereDef.xyz,output.world);
		output.radial = SphereDef.w-d;
	#endif
	
}

static const float MaxAmbient = 0.45;
static const float3 BlueAmbient = float3(0.6,0.65,1.0) * MaxAmbient;

float4 psMain(V2P input) : SV_TARGET
{



	float step = max(abs(ddx(input.intensity)), abs(ddy(input.intensity)));
	float centerGridOpacity = (1.0 - smoothstep(0.07 - step, 0.07 + step, abs(input.intensity))) / (1.0 + 10.0 * step * step);
	float outerGridOpacity = (1.0 - smoothstep(0.07, 0.14 + step*2.0, abs(input.intensity))) / (1.0 + 10.0 * step * step);
//	float outerGridOpacity = (1.0 - smoothstep(0.2 - step, 0.2 + step, abs(input.intensity))) / (1.0 + 10.0 * step * step);
	outerGridOpacity = max(0.0,	outerGridOpacity - centerGridOpacity);

	centerGridOpacity *= 0.5;
	outerGridOpacity *= 0.5;
	//(centerGridOpacity)*0.5 + 0.5;
	//outerGridOpacity = (outerGridOpacity)*0.75 + 0.25;

	float3 tone;

	step = max(abs(ddx(input.structureBlueshift.x)), abs(ddy(input.structureBlueshift.x)));
	float structure = (smoothstep(0.9 - step, 0.9 + step, input.structureBlueshift.x) + 1.0 - input.structureBlueshift.x) * 0.5 + 0.5;

	float diffuse = input.diffuse;
	float light = diffuse > 0 ? Shade(input.world) : 0.0;

	//max(0.0, dot(input.normal, Sun));
	float3 baseAmbient = lerp((float3)MaxAmbient,BlueAmbient,input.structureBlueshift.y);
	float3 baseDiffuse = (float3)1.0 - baseAmbient;

	float4 rs = float4(/*tone **/ input.color.xyz * structure * ((baseDiffuse*diffuse *light + baseAmbient) * (0.25 + 0.75 * input.color.a)), 1.0);
	rs.rgb += input.glowColor;

	{
//		if (all(rs.rgb == 0.0))
		float bright = min(min(rs.g,rs.b),rs.r);
//		overlayColor = (0.5+bright) * (1.0 - smoothstep(0.2,0.3,bright));
		//if (bright < 0.25)
		//	overlayColor = 0.5+bright;
		rs.rgb = lerp(rs.rgb,(float3)(0.5),outerGridOpacity * (1.0 - smoothstep(0.2,0.3,bright)));//outer grid is 0.5 grey
		rs.rgb *= (1.0 - centerGridOpacity);	//center grid is black



	//	rs.rgb = lerp(rs.rgb, (float3)overlayColor, 1.0 - intensity);
	}


	
	#if defined(SPHERE_DISPLAY) || defined(PARABOLOID_DISPLAY)


		float r = -input.radial + 1.0;
			//return float4((float3)r,1.0);
			//float3 sphereProjected = mul(SphereMatrix,float4(input.world,1.0)).xyz;
		float sphereOpacity = (1.0 - smoothstep(0.95,1.05,r)) * (0.5 + 0.5 * smoothstep(0.8,0.95,r));
		//float sphereOpacity = input.sphereOpacity;

		#ifdef SPHERE_DISPLAY
			float3 color = SphereColor.rgb;
		#elif defined(PARABOLOID_DISPLAY)
			float3 color = ParaboloidColor_FlatRadius.rgb;
		#endif

		rs.rgb = lerp(rs.rgb, color, sphereOpacity );

	#endif

	//rs.rgb = CalculatePositionFog(input.world, rs.rgb);
	rs.rgb = lerp(rs.rgb,input.atmosphere.rgb,input.atmosphere.a);

	return rs;
}


