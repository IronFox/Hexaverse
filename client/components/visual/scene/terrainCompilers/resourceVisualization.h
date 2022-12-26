#pragma once
#ifdef HAS_WINDOW

#include "compilerBase.h"
#include <container/hashtable.h>
#include <objects/resource.h>

namespace VisualTerrain
{
	


	struct ResourceVertex //! Temporarily the same layout as TerrainVertex
	{
		float4		vertex;
		float4		normal;
		float4		color;

		/**/		ResourceVertex()	{};
		/**/		ResourceVertex(const float3&vtx, const float3&n, const float4&color, float structure) : vertex(vtx,1), normal(n,structure), color(color){}
		/**/		ResourceVertex(const float3&vtx, const float3&n, const float3&color, float structure) : vertex(vtx,1), normal(n, structure), color(color,1.f){}


		const M::TVec3<>& GetPosition() const {return vertex.xyz;}
	};


	class ResourceStackDetail
	{
	public:
		typedef ResourceStackDetail			Self;

		Hull<ResourceVertex>				hull;
		Engine::DX11::InstructionBlock32	instruction,shadowInstruction;	// might cast shadow some day


		void						Destroy()
		{
			hull.Clear();
			shadowInstruction = instruction = Engine::DX11::InstructionBlock32();
		}

		friend void swap(Self&a, Self&b)
		{
			using std::swap;
			swap(a.hull,b.hull);
			swap(a.instruction,b.instruction);
			swap(a.shadowInstruction,b.shadowInstruction);
		}

		M::Sphere<>					GetBoundingSphere() const
		{
			return hull.GetBoundingSphere();
		}



		/**/
	};


	class ResourceStack
	{
	public:
		typedef ResourceStack	Self;

		bool								isOccupied;
		FixedArray<ResourceStackDetail,4>	details;

		friend void swap(Self&a, Self&b)
		{
			using std::swap;
			swap(a.details,b.details);
			swap(a.isOccupied,b.isOccupied);
		}

		void								Compile(const Objects::Resource::CoreVector&vec);
		M::Sphere<>							GetBoundingSphere() const {return details.First().GetBoundingSphere();}
	};


	struct TStackInstance
	{
		TBounds								bounds;
		index_t								stackIndex;
		M::TMatrix4<>						system;
	};

	struct TResourceCover
	{
		Vector<TStackInstance>				regularStacks, occupiedStacks;


		void								Clear()
		{
			regularStacks.Clear();
			occupiedStacks.Clear();
		}

		TBounds								CalculateBounds()
		{
			TBounds rs;
			rs.boundingBox = M::Box<>::Invalid;

			foreach(regularStacks, stack)
				rs.boundingBox.Include(stack->bounds.boundingBox);
			foreach(occupiedStacks, stack)
				rs.boundingBox.Include(stack->bounds.boundingBox);
			rs.boundingSphere.center = rs.boundingBox.GetCenter();
			rs.boundingSphere.radius = 0.f;
			foreach(regularStacks, stack)
				rs.boundingSphere.Include(stack->bounds.boundingSphere);
			foreach(occupiedStacks, stack)
				rs.boundingSphere.Include(stack->bounds.boundingSphere);
			return rs;
		}
	};


	class StackKey
	{
	public:
		typedef StackKey Self;

		Objects::Resource::CoreVector		vector;
		bool								isConsumedElsewhere;


		/**/								StackKey()	{}
		/**/								StackKey(const Objects::Resource::CoreVector&vector,
												bool isConsumedElsewhere):vector(vector),
												isConsumedElsewhere(isConsumedElsewhere){}

		friend hash_t						Hash(const Self&s)
		{
			using GlobalHashFunctions::Hash;
			return HashValue(Hash(s.vector)).Add(Hash(s.isConsumedElsewhere));
		}

		bool								operator==(const Self&other) const
		{
			return vector == other.vector && isConsumedElsewhere == other.isConsumedElsewhere;
		}

		bool	operator!=(const Self&other) const
		{
			return !operator==(other);
		}
	};

	class ResourceStackStore
	{
	public:

		TStackInstance						Instantiate(const StackKey&key, const M::TMatrix4<>&system, Sys::SpinLock&lock)
		{
			M::Sphere<> bounds;
			index_t idx = GetOrBuild(key, lock,bounds);
			const ResourceStack&stack = stacks[idx];
			M::TVec3<> center;
			M::Mat::Transform(system,bounds.center,center);
			TStackInstance rs;
			rs.system = system;
			rs.stackIndex = idx;
			rs.bounds.boundingSphere = M::Sphere<>(center,bounds.radius);
			rs.bounds.boundingBox = M::Box<>().SetCenter(center,bounds.radius);
			return rs;
		}

		index_t								GetOrBuild(const StackKey&key, Sys::SpinLock&lock, M::Sphere<>&outBoundingSphere)
		{
			lock.lock();
				const index_t*idx = stackMap.QueryPointer(key);
				if (idx != nullptr)
				{
					lock.unlock();
					return *idx;
				}
				index_t rs = stacks.Count();
				stacks.Append();
				stackMap.Set(key,rs);
			lock.unlock();

			ResourceStack s;
			s.isOccupied = key.isConsumedElsewhere;
			s.Compile(key.vector);
			outBoundingSphere = s.GetBoundingSphere();

			lock.lock();
				using std::swap;
				swap(stacks[rs],s);
			lock.unlock();

			return rs;
		}

		void								Render(const TStackInstance&inst) const;


		void								PrepareRendering(const DX11::BaseInstructionBlock&regular, const DX11::BaseInstructionBlock&occupied);

	private:
		HashTable<StackKey,index_t>			stackMap;
		Vector<ResourceStack>				stacks;
		index_t								newFrom = 0;
	};




	void CompileResources(TResourceCover&cover, Grid::ResourceDiscovery&visibility, index_t offset, count_t count, Grid::FClipHeight clipHeight, const ArrayRef<Grid::Resource>&resources, ResourceStackStore&store);

}


#endif
