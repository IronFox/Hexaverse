#include "common.inc.hlsl"


struct A2V
{
	float4 positionMax : PositionMax;
	float4 colorProgress : ColorProgress;
	float2 lineProgress_scale : LineProgress_Scale;
};


struct V2G
{
	float4 positionMax : PositionMax;
	float4 colorProgress : ColorProgress;
	float2 lineProgress_scale : LineProgress_Scale;
};

struct G2P
{
	float4 positionMax : PositionMax;
	float4 position : SV_POSITION;
	float2 uv : UV;
	float4 colorProgress : ColorProgress;
};



//this shader is currently actually used for filled-out circle visualization

void vsMain(A2V input, out V2G output)
{
	output.positionMax = float4(input.positionMax.xyz + WorldCamera[2].xyz * BAR_SIZE, input.positionMax.w);
	output.colorProgress = input.colorProgress;
	output.lineProgress_scale = input.lineProgress_scale;
}



[maxvertexcount (6)]
void gsMain(inout TriangleStream<G2P> stream, point V2G input[1])
{
	G2P output;
	output.colorProgress = input[0].colorProgress;
	output.positionMax = input[0].positionMax;
	float size = 0.02;
	float3	x = WorldCamera[0].xyz*BAR_SIZE*0.5 * input[0].lineProgress_scale.y,
			y = WorldCamera[1].xyz*BAR_SIZE*0.15 * input[0].lineProgress_scale.y;
	
	float3	y0 =  y*input[0].lineProgress_scale.x + WorldCamera[1].xyz*BAR_SIZE * input[0].lineProgress_scale.y,
			y1 =  y0 + y;
	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].positionMax.xyz - x + y0);
	stream.Append(output);
	output.uv = float2(1,0);
	output.position = TransformAndProject(input[0].positionMax.xyz + x + y0);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].positionMax.xyz + x + y1);
	stream.Append(output);
	stream.RestartStrip();


	output.uv = float2(0,0);
	output.position = TransformAndProject(input[0].positionMax.xyz - x + y0);
	stream.Append(output);
	output.uv = float2(1,1);
	output.position = TransformAndProject(input[0].positionMax.xyz + x + y1);
	stream.Append(output);
	output.uv = float2(0,1);
	output.position = TransformAndProject(input[0].positionMax.xyz - x + y1);
	stream.Append(output);
	stream.RestartStrip();
}


float4 psMain(G2P input) : SV_TARGET
{
	float3 rs = input.colorProgress.rgb;

	float edge = sqr(smoothstep(0.0,0.5,abs(input.uv.y-0.5)));
	rs += (1.0 - edge) * 0.3;
	//rs *= (1.0 - edge) * 0.5 + 0.5;
	float3 back = (float3)(edge*0.1);

	float	dx = abs(ddx(input.uv.x)),
			dy = abs(ddy(input.uv.y));
	float	f0 = smoothstep(-dx,dx,input.uv.x),
			f1 = smoothstep(-dx,dx,1.0 - input.uv.x),
			f2 = smoothstep(-dy,dy,input.uv.y),
			f3 = smoothstep(-dy,dy,1.0 - input.uv.y),
			f4 = smoothstep(-dx,dx,input.colorProgress.w / input.positionMax.w - input.uv.x);
	float repeat = fmod(input.positionMax.w * input.uv.x , 1.0);
	float modulate = (min(sqr((repeat - 0.5) * 2.2),1.0)) * (1.0 - 0.5 * smoothstep(0.0,0.5,abs(0.5 - input.uv.y)));
	rs *= lerp(1.0,0.75,modulate);
	back += modulate * 0.1 * input.colorProgress.rgb;
	return lerp(float4(back, edge*0.5 + 0.5), float4(rs,1.0), f0 * f1 * f2 * f3 * f4);
	//input.uv.x > 0.01 && input.colorProgress.w > input.uv.x ? float4(rs,1.0) : float4((float3)back, edge*0.5 + 0.5);
}


