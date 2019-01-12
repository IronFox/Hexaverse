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


void vsMain(A2V input, out V2P output, uint instanceID : SV_InstanceID)
{
	float3x3 system;
	system[0] = input.instDir;
	system[2] = input.instUp;
	system[1] = cross(input.instUp,input.instDir);
	output.normal = mul(input.normal,system);
	output.world = mul(input.position,system)*input.instScale + input.instPosition;
	output.position = TransformAndProject(output.world);
	output.color = float4(max(input.colorRGB_wind.rgb,(float3)0.0),1.0);
	output.glowColor = max(-input.colorRGB_wind.rgb,(float3)0.0)*0.5;
	output.color.rgb *= SunColor.rgb;
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


