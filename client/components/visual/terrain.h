#pragma once
#ifdef HAS_WINDOW

#include <grid/worldData.h>
#include <math/matrix.h>
#include <math/vclasses.h>
#include "../../ExtRenderer/DirectX11.h"

namespace VisualTerrain
{

	index_t	TestLowGridIntersection(TVec3<> b, TVec3<> d, float radius, float&distance);
	bool	TestHullIntersection(const TVec3<>&b, const TVec3<>&d, float&distance, TVec3<>&outNormal, bool includeWater);


	void	UpdateAtmosphere(const float4&fogColor);
	void	UpdateAtmosphere(float radius, const float4&fogColor);
	void	UpdateSun(const float4&sunPlane_Speed,const float4&sunColor);


	const float3&	GetSunDirection();
	void	PerFrameUpdateSunDirection();
	void	UpdateSunDirection(const float3&, bool shadows = true);
	/**
	Retrieves a directional vector of the sun at an altered rotation speed.
	Given speed-factor 1, the result exactly matches the current direction of the sun.
	@param[out] outDir Resulting sun direction. Normalized.
	@param speedFactor Factor to accelerate (or slow down) regular sun speed by. Any non-NaN, non-INF value is good.
	@return Angle used to compute the resulting outDir. Depending on game progression and @a speedFactor,
			this value can be large.
	*/
	double	GetAcceleratedSunDirection(TVec3<>&outDir, float speedFactor=1.f);
	void	ResetSunRotation();
	/**
	Retrieves the orientation system of the sun.
	The z-axis is the rotation axis, the other two axes are orthogonal to each other and the z-axis but otherwise arbitrary.
	*/
	const TMatrix3<>& GetSunSystem();



	/**
	Updates plantary parameters to be used prior to rendering flat grid components (RenderNeighborhood(), RenderLowCell()). Does not affect other rendering operations
	*/
	void	UpdatePlanetRenderingParameters(float radius);
	void	RenderLowCell(index_t stackID);
	void	RenderTransparentLowCell(index_t stackID);
	void	RenderLowGrid();
	void	FlushAllRendererData();


	//global construction functions

	/**
	Loads all GPU resources associated with rendering terrain.
	Must be called after Objects::Create(), since it uses the same shadow map
	*/
	void	Create();

	/**
	Frees allocated GPU resources.
	Subsequent rendering instructions will fail (horribly)
	*/
	void	Destroy();


	//scene rendering functions:
	void	RenderOpaqueTerrain(const Engine::Frustum<>&frustum);
	void	RenderShadows();
	void	RenderWater(const Engine::Frustum<>&frustum);
	void	RenderOverlay(const Engine::Frustum<>&frustum);
	void	RenderResources(const Engine::Frustum<>&frustum);

	/**
	Checks whether or not the specified family is outdated.
	This function does not check for index validity!
	*/
	bool	FamilyIsOutdated(index_t);
	void	RebuildOutdatedFamilies();
	/**
	Sets all families outdated, requesting a full rebuild when next needed
	*/
	void	Invalidate(bool geometry=true, bool resources=true);
	void	InvalidateFamily(BaseGrid::family_t,bool geometry=true, bool resources=true);
	

	namespace SeeThrough
	{

		/**
		Updates see-through to the specified settings. Do not alter related values manually
		@param enable Enable see-through. Set false to disable see-through
		*/
		void	Update(bool enable, const Grid::LCellIdI&focusPoint);

		/**
		Queries whether see-through is currently enabled
		*/
		bool	IsEnabled();

		/**
		Queries the current focus point of the see-through clipping
		*/
		const Grid::LCellIdI&	GetFocusPoint();

		/**
		Retrieves the low-resolution cell id closest to the current camera focus point for use to update see-through.
		If the current camera location is within proximity of the current cut-off focus cell, then the focus cell is returned instead.
		The returned height is set to the current focus height and may be ignored if not needed.
		*/
		Grid::LCellIdI		LocateCamera();



		/**
		Clipping function for see-through. Returns the int height (not unsigned layer) at which (and above) clipping occurs
		*/
		int		ClipHeight(index_t cellID);


	}

	
	void		SetParaboloidDisplay(const TVec3<>&direction, float height, float flatRadius, const float4&color);
	void		SetSphereDisplay(const TVec3<>&center, float radius, const float4&color);
	void		UnsetParaboloidAndSphereDisplay();






	class OrbitGrid
	{
	public:
		struct TCell
		{
			short visible = 0;


		};


		TCell	cells[Grid::LowSetup::GridType::CellCount];
		float	orbit = 1.f;

		#ifdef HAS_WINDOW
			struct Vertex
			{
				float4		vertex3_edge;
				float4		normal3_opacity;
				float		emp;

				/**/		Vertex()	{};
				/**/		Vertex(const float3&n,float edge, float orbit, float opacity, float emp) : vertex3_edge(n*orbit,edge), normal3_opacity(n,opacity),emp(emp){}
				void		Set(const float3&n,float edge, float orbit, float opacity, float emp) {vertex3_edge = float4(n*orbit,edge); normal3_opacity = float4(n,opacity); this->emp = emp;}
			};
	
			Array<Vertex>	vertices;
			Engine::DX11::InstructionBlock32	instruction;
			void		ForceRecompileNow();
			void		Render(const Engine::Frustum<>&);
		#endif

		bool			valid = false;
		void			Invalidate() {valid = false;}

			
		/**/			OrbitGrid()
		{
			Clear();
		}

		void			ChangeSensory(const Grid::LCellIdI&center, float radius, signed char change);

		void			Clear()
		{
			TCell clear;
			for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
				cells[i] = clear;
			Invalidate();
			//colorArea.active = false;
		}


		//void	Color(const LCellIdI&center, int fuelRadius);
		//void	StripColor();

	};



}

#endif
