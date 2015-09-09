#ifndef common_inc
#define common_inc

cbuffer Camera
{
	float4x4	ProjectionMatrix,
				ViewMatrix,
				ViewProjectionMatrix,
				WorldCamera;
};


cbuffer Object
{
	float4x4	ObjectMatrix;
};

cbuffer ObjectPosition
{
	float4		ObjectPosition;
};


float4 Project(float3 p)
{
	return mul(ProjectionMatrix,float4(p,1.0));
}

float4 Project(float4 p)
{
	return mul(ProjectionMatrix,p);
}


float4 TransformAndProject(float3 p)
{
	return mul(ViewProjectionMatrix,float4(p,1.0));
//	return Project(mul(ViewMatrix,float4(p,1.0)).xyz);
}


float sqr(float x)	{return x*x;}
float sqr(float2 x)	{return dot(x,x);}
float sqr(float3 x)	{return dot(x,x);}
float sqr(float4 x)	{return dot(x,x);}

float3 sqr3(float3 x)	{return x*x;}

float Max(float4 x)	{return max(max(x.x,x.w),max(x.z,x.y));}
float Max(float3 x)	{return max(x.x,max(x.z,x.y));}
float Max(float2 x)	{return max(x.x,x.y);}
float Min(float4 x)	{return min(min(x.x,x.w),min(x.z,x.y));}
float Min(float3 x)	{return min(x.x,min(x.z,x.y));}
float Min(float2 x)	{return min(x.x,x.y);}
float Sum(float4 x)	{return x.x + x.y + x.z + x.w;}
float Sum(float3 x)	{return x.x + x.y + x.z;}
float Sum(float2 x)	{return x.x + x.y;}
float Avg(float4 x)	{return Sum(x)*0.25;}
float Avg(float3 x)	{return Sum(x)*(1.0/3.0);}
float Avg(float2 x)	{return Sum(x)*0.5;}


static const float Pi = 3.1415926535897932384626433832795;
static const float PI = Pi;


#endif
