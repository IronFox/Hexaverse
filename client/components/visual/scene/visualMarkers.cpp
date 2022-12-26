#include "visualMarkers.h"
#include <container/hashtable.h>
#include "objectRendering.h"
#include <objects/systems.h>
#include "../../cameraControl.h"
#include <objects/markers.h>
#include <objects/faction.h>


namespace Objects { namespace VisualMarkers
{

	using namespace DeltaWorks;
	
	namespace Debug
	{
		namespace Details
		{
			struct LocalMarker
			{
				index_t		id;

				Grid::LCellIdI	location;
				M::TVec3<>		color,
								labelCenter;
				float4			keyColor;
				String			label;
				M::TMatrix4<>	system;

				bool			labelVisible = false,seeThrough=false;
				M::TVec3<>		screenLocation;
			};


			index_t counter = 0;
			HashTable<index_t,index_t> indexMap;
			HashTable<Grid::LCellIdI,index_t> locationMap;
			Vector<LocalMarker>  markers;


			index_t NextID()
			{
				counter++;
				while (counter == InvalidIndex || indexMap.IsSet(counter))
					counter++;
				return counter;
			}

		}


		bool		IdIsUsed(index_t id)
		{
			using namespace Details;
			return indexMap.IsSet(id);
		}
	


		void		CreatePreIdentified( index_t id, const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label, bool seeThrough )
		{
			ASSERT__(id != InvalidIndex);
			using namespace Details;
			if (indexMap.IsSet(id))
				throw new Except::Program::UniquenessViolation(CLOCATION,"Trying to re-register marker "+String(id));
			counter = std::max(id,counter);

			index_t at;
			LocalMarker*m = nullptr;
			if (locationMap.Query(loc,at))
			{
				m = markers + at;
				LogEvent(EventSignificance::Debug,"Replacing visual marker "+String(m->id)+" at "+ToString(loc));
				indexMap.Unset(m->id);
			}
			else
			{
				at = markers.Count();
				m = &markers.Append();
				locationMap.Set(loc,at);
			}
			m->id = id;
			indexMap.Set(m->id,at);
			m->color = color;
			m->label = label;
			m->location = loc;
			m->keyColor = float4(color,1.f);
			const auto h = Grid::GetLayerHeight(loc.height);
			BuildSystem(loc,h,h,m->system);
			m->labelCenter = m->system.w.xyz + m->system.z.xyz * 2.f;
			m->seeThrough = seeThrough;
			LogEvent(EventSignificance::Debug,"Created visual marker "+String(m->id)+" at "+ToString(loc));

			ASSERT_EQUAL__(markers.Count(),indexMap.Count());
			ASSERT_EQUAL__(markers.Count(),locationMap.Count());
		}

		index_t		Create( const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label, bool seeThrough )
		{
			const index_t rs = Details::NextID();
			CreatePreIdentified(rs,loc,color,label,seeThrough);
			return rs;

		}

		void		Delete( index_t idx )
		{
			LogEvent(EventSignificance::Debug,"Erasing visual marker "+String(idx));
			using namespace Details;
			index_t at;
			if (!indexMap.QueryAndUnset(idx,at))
			{
				LogEvent(EventSignificance::NonFatalError,"Trying to remove non-existent visual marker "+String(idx));
				return;
			}
			locationMap.Unset(markers[at].location);
			markers.Erase(at);
			indexMap.VisitAllValues([at](index_t&found)
			{
				if (found > at)
					found--;
			});
			locationMap.VisitAllValues([at](index_t&found)
			{
				if (found > at)
					found--;
			});
			ASSERT_EQUAL__(markers.Count(),indexMap.Count());
			ASSERT_EQUAL__(markers.Count(),locationMap.Count());
		}

		void		FlushAll( )
		{
			using namespace Details;
			if (markers.IsNotEmpty())
				LogEvent(EventSignificance::Debug,"Erasing "+String(markers.Count())+" visual marker(s)");
			indexMap.Clear();
			markers.Clear();
			locationMap.Clear();
		}

	}


	void	Render( )
	{
		namespace Debug = Debug::Details;
		if (Debug::markers.IsEmpty() && Objects::MarkerStore::client.GetArtifacts().allianceDefs.IsEmpty())
			return;
		display.StoreRendererState();
		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::Add);
		foreach (Objects::MarkerStore::client.GetArtifacts().allianceDefs, def)
		{
			if (def->marker->age < 1)
			{
				display.SetObjectMatrix(def->system);
				keyColor.Update(def->color);
				display.Render(gatheringPoint,true);
			}
		}
		foreach (Debug::markers,m)
		{
			if (m->seeThrough)
			{
				display.SetObjectMatrix(m->system);
				keyColor.Update(m->keyColor);
				display.Render(gatheringPoint,true);
			}
		}
		display.SetDepthTest(true,true);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		foreach (Debug::markers,m)
		{
			display.SetObjectMatrix(m->system);
			keyColor.Update(m->keyColor);
			display.Render(gatheringPoint,true);
			m->labelVisible = m->label.IsNotEmpty()
				&& Camera::scene.Project(m->labelCenter,m->screenLocation);
		}

		foreach (Objects::MarkerStore::client.GetArtifacts().allianceDefs, def)
		{
			//display.SetDepthTest(def->marker->age >= 1,false);
			display.SetObjectMatrix(def->system);
			keyColor.Update(def->color);
			display.Render(gatheringPoint,true);
			def->labelVisible = def->marker->text.IsNotEmpty()
				&& Camera::scene.Project(def->labelCenter,def->screenLocation);
		}

		display.RestoreRendererState();
	}

	void	Locate(const M::TVec3<>&projected)
	{
		textout.MoveTo(float3(projected.xy * 0.5f + 0.5f,-projected.z));	
	}
	
	void	RenderText( )
	{
		namespace Debug = Debug::Details;
		if (Debug::markers.IsEmpty() && Objects::MarkerStore::client.GetArtifacts().allianceDefs.IsEmpty())
			return;

		display.StoreRendererState();

		display.SetDepthTest(true,false);

		textout.PushState();
		textout.Scale(2.f);
		foreach (Debug::markers,m)
			if (m->labelVisible)
			{
				display.SetDepthTest(!m->seeThrough,false);

				Locate(m->screenLocation);
				textout.SetColor(m->color);
				textout << m->label;
			}

		foreach (Objects::MarkerStore::client.GetArtifacts().allianceDefs, def)
			if (def->labelVisible)
			{
				display.SetDepthTest(def->marker->age >= 1,false);

				Locate(def->screenLocation);
				textout.SetColor(def->color);
				textout << def->marker->text;
			}


		textout.PopState();

		display.RestoreRendererState();
	}


	void	RenderTranslucentBeams()
	{
		display.StoreRendererState();
		display.SetRasterizer(true,false);	//some shafts are flipped, but culling makes no sense anyways
		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::Add);

		foreach (Debug::Details::markers,m)
			if (m->seeThrough)
				Objects::Renderer::RenderBeam(m->system.w.xyz, Grid::TopPlanetRadius - Grid::BottomPlanetRadius,
					0.4f, float4(m->color,1), float4(m->color,0));
		
		foreach (Objects::MarkerStore::client.GetArtifacts().allianceDefs, def)
			if (def->marker->age < 1)
				Objects::Renderer::RenderBeam(def->system.w.xyz, Grid::TopPlanetRadius - Grid::BottomPlanetRadius,
						0.4f, def->color, float4(def->color.rgb,0));

		display.RestoreRendererState();
	}



}}

