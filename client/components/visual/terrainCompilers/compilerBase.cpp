#include "compilerBase.h"
#include <grid/stack.h>

namespace VisualTerrain
{

	TBaseColor	GetBaseColor(const Grid::CellStack::Cell&cell)
	{
		TBaseColor rs = cell.typeColor[cell.content]; 
		//if (submergence > 0)
			//color = float3(0.1f);
		//Vec::abs(color);color *= 0.5f;

		rs.color.r *= Grid::powi(0.8f, cell.submergence); 
		rs.color.g *= Grid::powi(0.85f, cell.submergence);
		rs.color.b *= Grid::powi(0.95f, cell.submergence); 
		//rs.visibility = visible > 0 ? 1.f : 0.f;
		return rs; 
	}

	TLitColor			GetColorU(const Grid::CellStack&stack,  unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType)
	{
		if (layer >= Grid::CellStack::Range)
		{
			mixes = false;
			outType = Grid::Content::Clear;
			return TLitColor(Grid::TVisibility(1.f));
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = layer > 0 ? stack.volumeCell[layer - 1] : Grid::CellStack::bottom;
		const Grid::TVisibility&vis = layer > 0 ? stack.volumeCell[layer - 1].GetVisibility() : stack.volumeCell[0].GetVisibility();
		mixes = cell.content == oppositeContent || Cell::typeColorMixes[cell.content];
		outType = Cell::Content::Reinterpret(cell.content);
		return TLitColor(GetBaseColor(cell), stack.volumeCell[layer].GetLightLevel(),0.f,vis);
	}
	
	TLitColor			GetDownColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType)
	{
		if (layer+1 >= Grid::CellStack::Range)
		{
			mixes = false;
			outType = Grid::Content::Clear;
			return TLitColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = stack.volumeCell[layer + 1];
		mixes = oppositeContent==cell.content || Cell::typeColorMixes[cell.content];
		outType = Cell::Content::Reinterpret(cell.content);
		return TLitColor(GetBaseColor(cell), stack.volumeCell[layer].GetLightLevel(),0.f, cell.GetVisibility());
	}


	TWeightedColor			GetMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent)
	{
		if (layer >= Grid::CellStack::Range)
		{
			//sumBrightnessTo += 1.f;
			outContent = Grid::Content::Clear;
			sumVisibilityTo ++;
			return TWeightedColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&upper = stack.volumeCell[layer];

		if (!layer)
		{
			sumBrightnessTo += upper.GetLightLevel();
			sumVisibilityTo += upper.IsCurrentlyVisible();
			outContent = Grid::Content::Clear;
			return TWeightedColor();
		}
		const Cell&cell = stack.volumeCell[layer - 1];
		float light = upper.GetLightLevel();
		sumVisibilityTo += (cell.IsCurrentlyVisible());
		if (!cell.IsOpaque() || upper.IsOpaque())
		{
			sumBrightnessTo += light * 1.3f;
			outContent = Grid::Content::Clear;
			return TWeightedColor();
				//float4(0, 0, 0, -0.1f);
				//float4(0.2f) * light;	//slight light increase
		}
		else
		{
			sumBrightnessTo += light;
			if (oppositeContent == cell.content || Cell::typeColorMixes[cell.content])
			{
				outContent = Grid::Content::Reinterpret( cell.content );
				return TWeightedColor(GetBaseColor(cell), 1.f);
			}
			outContent = Grid::Content::Clear;
			return TWeightedColor();
		}

	}


	
	TWeightedColor			GetDownMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Grid::Content&outContent)
	{
		if (layer >= Grid::CellStack::Range)
		{
			sumBrightnessTo += 1.f;
			outContent = Grid::Content::Clear;
			return TWeightedColor();
		}
		if (!layer)
		{
			sumBrightnessTo += stack.volumeCell[layer].GetLightLevel();
			outContent = Grid::Content::Clear;
			return TWeightedColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = stack.volumeCell[layer - 1],
				&upper = stack.volumeCell[layer];
		float light = cell.GetLightLevel();
		sumVisibilityTo += cell.IsCurrentlyVisible();
		if (cell.IsOpaque() || !upper.IsOpaque())
		{
			sumBrightnessTo += light * 1.3f;
			outContent = Grid::Content::Clear;
			return TWeightedColor();
				//float4(0, 0, 0, -0.1f);
				//float4(0.2f) * light;	//slight light increase
		}
		else
		{
			sumBrightnessTo += light;
			if (oppositeContent == upper.content || Cell::typeColorMixes[upper.content])
			{
				outContent = Grid::Content::Reinterpret( upper.content );
				return TWeightedColor(GetBaseColor(upper), 1.f);
			}
			outContent = Grid::Content::Clear;

			return TWeightedColor();
		}

	}



	TBaseColor::TBaseColor(const TWeightedColor&c):color(c.color / c.weight)
	{}


	void	TLitColor::CompileFinalColor(TVec4<>&rs)	const
	{
		rs.a = capped ? 0.f : lightLevel;
		rs.rgb = color;
		#ifndef DEBUG_HIDE_SENSORY
			if (visibility < 0.5f)
			{
				#if 0
					color = (float3)0.f;
				#else
					//float avg = (color.r + color.g + color.b) / 3.f * 0.5f + 0.5f;
					if (color.r < 0)
					{
						rs.r = -(1.f - (1.f + color.r) / 4.f);
						rs.g = -(1.f - (1.f + color.g) / 4.f);
						rs.b = -(1.f - (1.f + color.b) / 4.f);
					}
					else
					{
						rs.r = 1.f - (1.f - color.r) / 4.f;
						rs.g = 1.f - (1.f - color.g) / 4.f;
						rs.b = 1.f - (1.f - color.b) / 4.f;
					}
					//color.r = (color.r + typeColor[0].r*2) / 3.f;
					//color.g = (color.g + typeColor[0].g*2) / 3.f;
					//color.b = (color.b + typeColor[0].b*2) / 3.f;
				#endif
			}
			else
		#endif

		if (capped)
			CapColor(rs.rgb);
		ApplyEMP(emp,rs.rgb);
	}

	/*static*/ void TLitColor::ApplyEMP(float intensity,TVec3<>&rs)
	{
		if (intensity > 0.f)
		{
			rs *= -1.5f;
			rs.rg *= 0.8f;
			rs.b *= 1.2f;
			//rs.b *= 1.5f;
			//rs.b -= (2.f + rs.b)*intensity;
			//TVec3<> rs2;
			//if (rs.r < 0)
			//	rs2 = rs * -0.4f + 0.6f;
			//else
			//	rs2 = rs * 0.4f + 0.6f;
			//rs2.r *= 0.7f;
			//rs2.g *= 0.85f;
			//if (rs.r < 0)
			//	rs2 = -rs2;
			//Vec::interpolate(rs,rs2,intensity,rs);
			//if (rs.b > 0)
			//	rs.b = -rs.b;

		}
	}

	void TLitColor::CapColor(TVec3<>&rs)	const
	{
		if (rs.r >= 0.f)
		{
			rs = Vector<>::zero;
			//rs.r = (1.f - visibility) ;
		}
		else
			rs *= 0.5f;
	}




}

