#ifndef lineRendererH
#define lineRendererH

#ifdef HAS_WINDOW
#include "../../ExtRenderer/DirectX11.h"



namespace LineRenderer
{
	void	Create();
	void	Destroy();

	void	SetColor(const float4&color);
	void	Rasterize(const float3&p0, const float3&p1);
	void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1);
	void	Rasterize(const TFloatRect&);
	void	Flush();
}

#endif
#endif
