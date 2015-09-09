#include "common.inc.hlsl"




struct A2V
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float4 color : COLOR;
};

struct V2P
{
	float4 position : SV_POSITION;
	float3 world : World;
	float3 normal : Normal;
	float4 color : COLOR;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2P output)
{
	output.normal = input.normal.xyz;
	output.world = input.position.xyz;
	output.position = TransformAndProject(output.world);
	output.color = input.color;
}

float4 psMain(V2P input) : SV_TARGET
{
	float step0 = max(abs(ddx(input.color.a)), abs(ddy(input.color.a)));
	float step1 = step0;// * 2.0;
	float x = fmod(input.color.a,1.0);
	float marker = 1.0 - 0.15 * ((1.0 - smoothstep(0.05 - step1, 0.05 + step1, x)) + (smoothstep(0.95 - step1, 0.95 + step1, x))) / (1.0 + 10.0 * step0 * step0);

	float color = Max(input.color.rgb);// - Min(input.color.rgb);
	float fresnel = color >= 0.0 ? (((1.0 - abs(dot(normalize(WorldCamera[3].xyz-input.world),input.normal))))) : 0.75;

	return float4(input.color.rgb * (0.5 + 0.5 * fresnel) * marker,
					 0.2+color*0.7 + max(0.0,fresnel-0.25)*0.75);

	//float4(0.7, 1.3, 0.8, smoothstep(0.8, 0.9, input.sourceOpacity));;
}


