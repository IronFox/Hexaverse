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


static const float4 WaterColor = float4(0.5f, 0.6f, 1.f,0.6f);

struct A2V
{
	float4 position : POSITION;
	float3 normal : NORMAL;
	float visibility : COLOR;
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
	float2 visibility_empFree : COLOR;
	float3 diffuseSpecularFresnel : DiffuseSpecular;
	float4 atmosphere : Atmosphere;
};


static const float n1 = 1.000293;	//air
static const float n2 = 1.3330;	//water

static const float r0 = sqr((n1 - n2) / (n1 + n2));

void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal;
	output.world = input.position.xyz;
	output.position = TransformAndProject(output.world);
	output.intensity = input.position.w;
	output.visibility_empFree.x = (fmod(input.visibility,0.25)*4.0-0.05)/0.9;
	output.visibility_empFree.y = 1.0 - floor(input.visibility*4.0)*0.25;	//int(4*input.visibility) can vary between 0 (no EMP effects) and 3 (full effect)
	output.diffuseSpecularFresnel.x = max(0.0, dot(input.normal.xyz, Sun.xyz));

	float3 reflected = normalize(reflect(output.world- WorldCamera[3].xyz, output.normal));
	output.diffuseSpecularFresnel.y = pow(max(0.0, dot(reflected, Sun.xyz)),32);


	//better fresnel:
	float r = r0 + (1.0 - r0)*pow(1.0 - 0.6*abs(dot(input.normal,reflected)),5);


//	output.diffuseSpecularFresnel.z = (((1.0 - abs(dot(WorldCamera[2].xyz,input.normal)))));
	output.diffuseSpecularFresnel.z = r;
	//((smoothstep(0.1,0.5,1.0 - abs(dot(reflected,input.normal)))));

	float afres = 1.0 - sqr(1.0 - output.diffuseSpecularFresnel.z);
	float4 rawFog = CalculateDirectionFog(output.world);
	//float3 dirFog = rawFog.rgb * rawFog.a;
	output.atmosphere = lerp( CalculatePositionFog(output.world),rawFog,saturate(afres)*0.5);


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



float4 psMain(V2P input) : SV_TARGET
{
	float step = max(abs(ddx(input.intensity)), abs(ddy(input.intensity)));
	float intensity = (1.0 - smoothstep(0.07 - step, 0.07 + step, abs(input.intensity))) / (1.0 + 10.0 * step * step);
	intensity = (1.0 - intensity)*0.5 + 0.5;	//overlay intensity

	float overlayColor;
	float3 tone;


	float light = input.diffuseSpecularFresnel.x > 0 ? Shade(input.world) : 0.0;
	float specular = input.diffuseSpecularFresnel.y * light;
	float diffuse = input.diffuseSpecularFresnel.x * light;

	//max(0.0, dot(input.normal, Sun));

	float4 rs = float4(/*tone **/ SunColor.rgb * WaterColor.rgb * ((diffuse *0.3 + (specular * 1.6) + 0.4) ),
		 (WaterColor.a + specular) * (0.5 + 0.5*input.diffuseSpecularFresnel.z));
	//rs.a += 1.0 - sqr(sqr(1.0 - input.diffuseSpecularFresnel.z))*0.5;
	rs.a += 1.0 -pow(abs(1.0 - input.diffuseSpecularFresnel.z),12)*0.9;



	//clip(input.visibility-0.01);
	float avg = Avg(rs.rgb) * 0.5 + 0.5;
	//rs.rgb = lerp((rs.rgb + avg /** 2.0*/)/2.0, rs.rgb, saturate(input.visibility + rs.a*0.5));
	rs.rgb = lerp(SunColor.rgb * (diffuse+specular*0.5+0.1), rs.rgb, saturate(input.visibility_empFree.x*0.6+0.4 /*+ rs.a*0.5*/));

	rs.rg *= 0.5 + 0.5 * input.visibility_empFree.y;	//degrade red and green in the presence of EMP down to 50%
	rs.ab += 0.25  * (1.0 - input.visibility_empFree.y);	//increase blue in the presence of EMP up to +0.25
	rs.a += 0.2 * (1.0 - input.visibility_empFree.x);

	rs.rgb *= intensity;	//grid

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

	
	rs.rgb = lerp(rs.rgb,input.atmosphere.rgb,input.atmosphere.a);


	return rs;
}


