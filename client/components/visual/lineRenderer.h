#ifndef lineRendererH
#define lineRendererH

#ifdef HAS_WINDOW
#include "../../ExtRenderer/DirectX11.h"



namespace LineRenderer
{
	using namespace DeltaWorks;
	namespace DX11 = Engine::DX11;

	void	Create();
	void	Destroy();

	void	SetColor(const float4&color);
	void	Rasterize(const float3&p0, const float3&p1);
	void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1);
	void	Rasterize(const M::TFloatRect&);
	void	Flush();



	class Baked
	{
	public:
		struct TVertex
		{
			float3	position;
			float4	color;
		};

		void		AddVertex(const float2&p, const float4&color = float4(1)) {TVertex&v = AddVertex(); v.position.xy = p; v.color = color;}
		void		AddVertex(const float3&p, const float4&color = float4(1)) {TVertex&v = AddVertex(); v.position = p; v.color = color;}
		void		Add(const TVertex&vtx) {vertices.Append(vtx);}
		TVertex&	AddVertex() {return vertices.Append();}
		void		AddLine(index_t v0, index_t v1)
		{
			indices << UINT32(v0 + indexOffset) << UINT32(v1 + indexOffset);
		}

		void	SetIndexOffsetToVertexCount()
		{
			indexOffset = vertices.Count();
		}

		void	Clear()	{vertices.Clear(); indices.Clear(); indexOffset = 0;}
		void	Destroy()	{Clear(); instruction.Clear();}

		void	Bake();
		void	Rasterize()	const;


		friend void	swap(Baked&a, Baked&b)
		{
			using std::swap;
			swap(a.instruction,b.instruction);
			swap(a.vertices,b.vertices);
			swap(a.indices,b.indices);
			swap(a.indexOffset,b.indexOffset);
		}

	private:
		DX11::InstructionBlock32	instruction;
		Buffer0<TVertex>			vertices;
		Buffer0<UINT32>				indices;
		count_t						indexOffset=0;
	};
}

#endif
#endif
