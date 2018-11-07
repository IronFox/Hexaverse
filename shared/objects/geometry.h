#pragma once

#include <container/buffer.h>
#include <math/vclasses.h>
#include <math/matrix.h>


namespace Objects
{
	using namespace DeltaWorks;



	struct Vertex
	{
		float3		vertex;
		float3		normal;
		float4		color;

		/**/		Vertex()	{};
		/**/		Vertex(const float3&vtx, const float3&n, const float3&color) : vertex(vtx), normal(n), color(color, 1)	{}
		/**/		Vertex(const float3&vtx, const float3&n, const float4&color) : vertex(vtx), normal(n), color(color)	{}
	};



	/**
	Multi-LOD geometrical surface description
	*/
	class Geometry
	{
	public:
		class LOD
		{
		public:
			Ctr::Buffer0<Vertex>	vertices;
			Ctr::Buffer0<UINT32>	indices;

			void	swap(LOD&other)
			{
				vertices.swap(other.vertices);
				indices.swap(other.indices);
			}

			void	Clear()
			{
				vertices.Clear();
				indices.Clear();
			}

			void	Compact()
			{
				vertices.Compact();
				indices.Compact();
			}

			template <typename T>
			void	PutIndex(UINT32 offset, T index)
			{
				indices << UINT32(offset + index);
			}

		};
	private:
		LOD*		currentLOD;
		Ctr::Buffer<LOD, 4, Swap>	lods;
		UINT32		vertexOffset,indexOffset;
		M::Box<>		boundingBox;

		void		_Reset()
		{
			vertexOffset = 0;
			indexOffset = 0;

		}


	public:
		/**/		Geometry()	{ currentLOD = &lods.Append();  _Reset(); }

		const LOD&	GetLOD(index_t lod) const { return lods[lod]; }

		count_t		CountLODs() const { return lods.Count(); }

		void	AppendAndActivateNewLOD()	{ currentLOD = &lods.Append(); _Reset(); }
		void	SetLOD(index_t lod)		{ while (lod >= lods.Count()) lods.Append(); currentLOD = lods + lod; _Reset(); }
		void	Clear()
		{
			lods.Truncate(1);
			if (lods.IsEmpty())
				lods.Append();
			currentLOD = &lods.First();
			currentLOD->Clear();
			_Reset();
		}

		void	ClearCompletely()
		{
			lods.Clear();
			currentLOD = nullptr;
			_Reset();
		}

		void	Scale(float factor)
		{
			foreach(currentLOD->vertices, vtx)
				vtx->vertex *= factor;
		}

		void	MakeVertex(const Vertex&vtx)
		{
			currentLOD->vertices << vtx;
		}

		void	MakeVertex(const float3&vtx, const float3&n, const float4&color)
		{
			MakeVertex(Vertex(vtx, n, color));
		}
		void	MakeVertex(const float3&vtx, const float3&n, const float3&color)
		{
			MakeVertex(Vertex(vtx, n, color));
		}
		void				ComputeNormals();
		void				GenerateNormals()				{ ComputeNormals(); }
		bool				TestIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float & distance)	const;
		void				UpdateBoundingBox();

		void	SetVertexOffsetToCurrent()
		{
			vertexOffset = (UINT32)currentLOD->vertices.Count();
			indexOffset = (UINT32)currentLOD->indices.Count();
		}

		void	MakeQuad(index_t i0, index_t i1, index_t i2, index_t i3)
		{
			UINT32*out = currentLOD->indices.AppendRow(6);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i1);
			(*out++) = (UINT32)(vertexOffset + i2);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i2);
			(*out++) = (UINT32)(vertexOffset + i3);
		}

		void	MakeQuadInv(index_t i0, index_t i1, index_t i2, index_t i3)
		{
			UINT32*out = currentLOD->indices.AppendRow(6);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i2);
			(*out++) = (UINT32)(vertexOffset + i1);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i3);
			(*out++) = (UINT32)(vertexOffset + i2);
		}

		void	MakeQuad(const Vertex&v0, const Vertex&v1, const Vertex&v2, const Vertex&v3)
		{
			index_t vertexOffset = currentLOD->vertices.Count();
			currentLOD->vertices << v0 << v1 << v2 << v3;
			UINT32*out = currentLOD->indices.AppendRow(6);
			(*out++) = (UINT32)(vertexOffset + 0);
			(*out++) = (UINT32)(vertexOffset + 1);
			(*out++) = (UINT32)(vertexOffset + 2);
			(*out++) = (UINT32)(vertexOffset + 0);
			(*out++) = (UINT32)(vertexOffset + 2);
			(*out++) = (UINT32)(vertexOffset + 3);
		}

		void	MakeQuadInv(const Vertex&v0, const Vertex&v1, const Vertex&v2, const Vertex&v3)
		{
			index_t vertexOffset = currentLOD->vertices.Count();
			currentLOD->vertices << v0 << v1 << v2 << v3;
			UINT32*out = currentLOD->indices.AppendRow(6);
			(*out++) = (UINT32)(vertexOffset + 0);
			(*out++) = (UINT32)(vertexOffset + 2);
			(*out++) = (UINT32)(vertexOffset + 1);
			(*out++) = (UINT32)(vertexOffset + 0);
			(*out++) = (UINT32)(vertexOffset + 3);
			(*out++) = (UINT32)(vertexOffset + 2);
		}



		void	MakeTriangle(index_t i0, index_t i1, index_t i2)
		{
			UINT32*out = currentLOD->indices.AppendRow(3);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i1);
			(*out++) = (UINT32)(vertexOffset + i2);
		}

		void	MakeTriangleInv(index_t i0, index_t i1, index_t i2)
		{
			UINT32*out = currentLOD->indices.AppendRow(3);
			(*out++) = (UINT32)(vertexOffset + i0);
			(*out++) = (UINT32)(vertexOffset + i2);
			(*out++) = (UINT32)(vertexOffset + i1);
		}

		void	AppendTriangleIndices(const UINT32*indices, count_t numIndices)
		{
			if (!numIndices)
				return;
			ASSERT__(!(numIndices % 3));
			UINT32*out = currentLOD->indices.AppendRow(numIndices);
			const UINT32*in = indices,
				*const end = indices + numIndices;
			for (; in != end; ++in, ++out)
			{
				*out = vertexOffset + *in;
			}
			ASSERT__(out == currentLOD->indices.end());
		}

		void	AppendTriangleIndices(const Ctr::BasicBuffer<UINT32>&indices)
		{
			AppendTriangleIndices(indices.pointer(), indices.Count());
		}

		void	AppendQuadIndices(const UINT32*indices, count_t numIndices)
		{
			if (!numIndices)
				return;
			ASSERT__(!(numIndices % 4));
			UINT32*out = currentLOD->indices.AppendRow(numIndices / 4 * 6);
			const UINT32*in = indices,
				*const end = indices + numIndices;
			for (; in != end; in+=4, out+=6)
			{
				out[0] = vertexOffset + in[0];
				out[1] = vertexOffset + in[1];
				out[2] = vertexOffset + in[2];
				
				out[3] = vertexOffset + in[0];
				out[4] = vertexOffset + in[2];
				out[5] = vertexOffset + in[3];
			}
			ASSERT__(out == currentLOD->indices.end());
		}
		
		void LoadFromObj(const PathString&filename);

		const M::Box<>&	GetBoundingBox()	const;
	};


}


