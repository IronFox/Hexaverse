#include "common.inc.hlsl"
#include "shadow.hlsli"


cbuffer Time
{
	float	Time;
};

struct A2V
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 colorRGB_wind : COLOR;
	float3 instPosition : InstancePosition;
	float3 instUp : InstanceUp;
	float3 instDir : InstanceDir;
	float  instScale : InstanceScale;
	float  instTone : InstanceTone;
	float  instLight : InstanceLight;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 normal : Normal;
	float blueshift : Blueshift;
	float4 color : COLOR;
	float3 glowColor : GlowColor;
	float diffuse : Diffuse;
	float4 atmosphere : Atmosphere;
};

TextureCube<float>	HeightMap;
SamplerState			HeightMapSampler;

cbuffer HeightMapOrientation
{
	float4x4	HeightMapOrientation0,
				HeightMapOrientation1,
				HeightMapOrientation2;
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

float3 Tone(float tone)
{
	float gb = 0.5 + tone * 0.5;
	return float3(1+tone,gb,gb);
}

void vsMain(A2V input, out V2P output)
{
	float3x3 system;
	system[0] = input.instDir;
	system[2] = input.instUp;
	system[1] = cross(system[2],system[0]);
	
	float3 dir = normalize(system[0] * SampleHeightMap(input.instPosition)
				+ system[1] * SampleHeightMap(-input.instPosition));
	float3 dir2 = cross(dir,input.instUp);
	float wind = (cos(Time*0.5 + dot(input.instPosition,dir)*10.0+ dot(input.instPosition,dir2)*10.0)+sin(Time*3.23512*0.5)*0.25) * input.colorRGB_wind.w;
	system[2] += dir * wind * 0.125;
	
	
	
	output.normal = mul(input.normal,system);
	output.world = mul(input.position,system)*input.instScale + input.instPosition;
	output.position = TransformAndProject(output.world);
	output.color = float4(max(input.colorRGB_wind.rgb,(float3)0.0),1.0);
	output.glowColor = max(-input.colorRGB_wind.rgb,(float3)0.0)*0.5;
	output.color.rgb *= SunColor.rgb;
	
	float red = saturate((input.instTone - 0.5) *2.0);
	float redYellow = saturate((input.instTone) *2.0);
	float yellow = redYellow - red;
	
	
	float rTone = red*0.35;
	float ryTone = redYellow * 0.35;
	output.color.r *= 1.0 + ryTone;
	output.color.g *= 0.5 + 0.5 * (1.0 - rTone);
	output.color.b *= 0.5 + 0.5 * (1.0 - ryTone);
	output.color.rgb = saturate(output.color.rgb);
	output.color.rgb *= input.instLight / (1.0 + abs(wind)*0.2);
	
	output.diffuse = max(0.0, dot(output.normal, Sun.xyz));
	output.blueshift = smoothstep(-0.4,0.3, -dot(normalize(output.world),Sun.xyz));
	output.atmosphere = CalculatePositionFog(output.world);
}

static const float MaxAmbient = 0.45;
static const float3 BlueAmbient = float3(0.6,0.65,1.0) * MaxAmbient;

float4 psMain(V2P input) : SV_TARGET
{
	//return float4(input.normal * 0.5 + 0.5,1.0);
	//return float4((float3)input.diffuse,1.0);
	float3 tone;
	float diffuse = input.diffuse;
	float light = diffuse > 0 ? Shade(input.world) : 0.0;

	float3 baseAmbient = lerp((float3)MaxAmbient,BlueAmbient,input.blueshift);
	float3 baseDiffuse = (float3)1.0 - baseAmbient;

	float4 rs = float4(/*tone **/ input.color.rgb * ((baseDiffuse*diffuse *light + baseAmbient)), 1.0);
	rs.rgb += input.glowColor;
	rs.rgb = lerp(rs.rgb,input.atmosphere.rgb,input.atmosphere.a);
	return rs;
}


