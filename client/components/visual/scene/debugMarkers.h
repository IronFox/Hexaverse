#pragma once
#ifdef HAS_WINDOW
#include <global_string.h>
#include <grid/setup.h>
#include <math/vector.h>

namespace Objects
{
	namespace DebugMarkers
	{
		namespace D = DeltaWorks;
		namespace M = D::Math;

		/**
		Creates a new marker.
		*/
		index_t		Create(const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label);
		/**
		Creates a new marker using an externally defined unique ID.
		@param id Unique marker identifier. Must be neither InvalidIndex nor already in use
		*/
		void		CreatePreIdentified(index_t id, const Grid::LCellIdI&loc, const M::TVec3<>&color, const D::String&label);
		/**
		Checks whether an ID candidate is already in use
		*/
		bool		IdIsUsed(index_t id);
		/**
		Deletes one specified marker.
		*/
		void		Delete(index_t);
		/**
		Flushes all markers.
		*/
		void		FlushAll();
		/**
		Renders all markers and updates label screen positions
		*/
		void		Render();
		/**
		Renders labels according to label positions determined during Render()
		*/
		void		RenderText();
	}
}



#endif
