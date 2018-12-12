#include "debugMarkers.h"
#include <container/hashtable.h>
#include "objectRendering.h"
#include <objects/systems.h>
#include "../../cameraControl.h"


namespace Objects { namespace DebugMarkers
{

	using namespace DeltaWorks;
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

			bool			labelVisible = false;
			M::TVec2<>		screenLocation;
		};


		index_t counter = 0;
		HashTable<index_t,index_t> indexMap;
		HashTable<Grid::LCellIdI,index_t> locationMap;
		Buffer0<LocalMarker>  markers;


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
	


	void		CreatePreIdentified( index_t id, const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label )
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
		LogEvent(EventSignificance::Debug,"Created visual marker "+String(m->id)+" at "+ToString(loc));

		ASSERT_EQUAL__(markers.Count(),indexMap.Count());
		ASSERT_EQUAL__(markers.Count(),locationMap.Count());
	}

	index_t		Create( const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label )
	{
		const index_t rs = Details::NextID();
		CreatePreIdentified(rs,loc,color,label);
		return rs;

	}

	void		Delete( index_t idx )
	{
		LogEvent(EventSignificance::Debug,"Erasing visual marker "+idx);
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

	void	Render( )
	{
		using namespace Details;
		if (markers.IsEmpty())
			return;
		display.StoreRendererState();
		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::Add);
		foreach (markers,m)
		{
			display.SetObjectMatrix(m->system);
			keyColor.Update(m->keyColor);
			display.Render(gatheringPoint,true);
		}
		display.SetDepthTest(true,true);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		foreach (markers,m)
		{
			display.SetObjectMatrix(m->system);
			keyColor.Update(m->keyColor);
			display.Render(gatheringPoint,true);
			m->labelVisible = m->label.IsNotEmpty()
				&& Camera::scene.PointToScreen(m->labelCenter,m->screenLocation);
		}
		display.RestoreRendererState();
	}

	void	RenderText( )
	{
		using namespace Details;
		if (markers.IsEmpty())
			return;
		textout.PushState();
		textout.Scale(2.f);
		foreach (markers,m)
			if (m->labelVisible)
			{
				textout.MoveTo(m->screenLocation * 0.5f + 0.5f);
				textout.SetColor(m->color);
				textout << m->label;
			}
		textout.PopState();
	}

}}

