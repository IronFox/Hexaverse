#include "common.inc.hlsl"
#include "shadow.hlsli"


static const float4 WaterColor = float4(0.5f, 0.6f, 1.f,0.6f);

struct A2V
{
	float4 position : POSITION;
	float3 normal : NORMAL;
	float visibility : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 normal : Normal;
	float intensity : Intensity;
	float2 visibility : COLOR;
	float3 diffuseSpecularFresnel : DiffuseSpecular;
	float4 atmosphere : Atmosphere;
	//float4 directionalAtmosphere : DirectionalAtmosphere;
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
	output.visibility.x = (fmod(input.visibility,0.25)*4.0-0.05)/0.9;
	output.visibility.y = 1.0 - floor(input.visibility*4.0)*0.25;
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
	rs.a += 1.0 - sqr(sqr(1.0 - input.diffuseSpecularFresnel.z));
	//clip(input.visibility-0.01);
	float avg = Avg(rs.rgb) * 0.5 + 0.5;
	//rs.rgb = lerp((rs.rgb + avg /** 2.0*/)/2.0, rs.rgb, saturate(input.visibility + rs.a*0.5));
	rs.rgb = lerp((float3)(diffuse+specular+0.4), rs.rgb, saturate(input.visibility.x*0.75+0.25 /*+ rs.a*0.5*/));
	rs.rg *= 0.5 + 0.5 * input.visibility.y;
	rs.ab += 0.25  * (1.0 - input.visibility.y);

	
	rs.rgb = lerp(rs.rgb,input.atmosphere.rgb,input.atmosphere.a);
//	rs.rgb = lerp(rs.rgb,input.atmosphere.rgb,1.0 - sqr(1.0 - input.atmosphere.a));

	rs.rgb *= intensity;

	//rs = input.atmosphere;


	//rs.rgb = (float3)sqr(sqr(sqr(input.diffuseSpecularFresnel.z)));
	//rs.a = 1.0;

	return rs;
}


