#pragma once
#ifdef HAS_WINDOW

#include <grid/worldData.h>
#include <grid/sensorCoverage.h>
#include <math/matrix.h>
#include <math/vclasses.h>

namespace Grid
{

	index_t	TestLowGridIntersection(TVec3<> b, TVec3<> d, float radius, float&distance);

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
	void	RenderLowGrid();

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

}

#endif
