#include "debugMarkers.h"
#include <container/hashtable.h>
#include "objectRendering.h"
#include <objects/systems.h>


namespace Objects { namespace DebugMarkers
{

	using namespace DeltaWorks;
	namespace Details
	{
		struct LocalMarker
		{
			index_t		id;

			Grid::LCellIdI	location;
			M::TVec3<>		color;
			float4			keyColor;
			String			label;
			M::TMatrix4<>	system;
		};


		index_t counter = 0;
		HashTable<index_t,index_t> indexMap;
		HashTable<Grid::LCellIdI,index_t> locationMap;
		Buffer0<LocalMarker>  markers;


		index_t NextID()
		{
			const index_t rs = counter;
			counter++;
			if (counter == InvalidIndex)
				counter++;
			return rs;
		}

	}


	index_t		Create( const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label )
	{
		using namespace Details;
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
		m->id = NextID();
		indexMap.Set(m->id,at);
		m->color = color;
		m->label = label;
		m->location = loc;
		m->keyColor = float4(color,1.f);
		const auto h = Grid::GetLayerHeight(loc.height);
		BuildSystem(loc,h,h,m->system);
		LogEvent(EventSignificance::Debug,"Created visual marker "+String(m->id)+" at "+ToString(loc));

		ASSERT_EQUAL__(markers.Count(),indexMap.Count());
		ASSERT_EQUAL__(markers.Count(),locationMap.Count());

		return m->id;
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
		foreach (markers,m)
		{
			display.SetObjectMatrix(m->system);
			keyColor.Update(m->keyColor);
			display.Render(gatheringPoint,true);
		}
	}

	void	RenderText( )
	{

	}

}}

