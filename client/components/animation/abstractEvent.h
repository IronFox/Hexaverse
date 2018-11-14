#pragma once

#include <memory>
#include <uniqueID.h>
#include <math/vector.h>
#include <protocol.h>
#include <global_string.h>

namespace Animation
{
	namespace M = DeltaWorks::M;
	namespace D = DeltaWorks;
	/*abstract*/ class Event
	{
	public:
		const TUniqueID			targetEntityID;

		static float			currentTime;


		/**/					Event(const TUniqueID&targetID);
		virtual					~Event()	{};
		virtual void			Begin()=0;
		virtual void			End() {};
		/**
		Updates the local event.
		Not guaranteed to be called
		@param Relative time in [0,1]
		*/
		virtual void			Update(float x){};
		#ifdef HAS_WINDOW
			virtual void		Render() const {};
		#endif
		virtual D::String		Describe() const=0;


		virtual bool			IsWorldDelta() const {return false;}
		virtual bool			IsMobileEntityMotion() const {return false;}
		virtual bool			IsMobileState() const {return false;}
	};

	typedef std::shared_ptr<Event>	PEvent;


}
