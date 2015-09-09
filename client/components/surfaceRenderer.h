#ifndef surfaceRendererH
#define surfaceRendererH
#ifdef HAS_WINDOW

#include "../ExtRenderer/DirectX11.h"



namespace SurfaceRenderer
{
	void	Create();
	void	Destroy();

	void	SetColor(const float4&color);
	void	Rasterize(const Rect<>&);
	void	RasterizeConvexPolygon(const TVec2<>*field, count_t numVertices);
	void	Rasterize(const TVec2<>&p0, const TVec2<>&p1, const TVec2<>&p2);
	void	Rasterize(const TVec2<>&p0, const TVec2<>&p1, const TVec2<>&p2, const TVec2<>&p3);
	void	Rasterize(const TVec2<>&p0, const TVec4<>&c0, const TVec2<>&p1, const TVec4<>&c1, const TVec2<>&p2, const TVec4<>&c2);
	void	Rasterize(const TVec2<>&p0, const TVec4<>&c0, const TVec2<>&p1, const TVec4<>&c1, const TVec2<>&p2, const TVec4<>&c2, const TVec2<>&p3, const TVec4<>&c3);
	//void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1);
	void	Flush();
}

#endif
#endif
