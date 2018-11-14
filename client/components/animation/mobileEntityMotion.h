#pragma once
#include "abstractEvent.h"
#include <protocol.h>
#include "../animation.h"

namespace Animation
{
	class MobileEntityMotion : public Event
	{
		typedef Event			Super;
	public:
		/**/					MobileEntityMotion(Protocol::MobileEntityMotion&);
		void					Append(Protocol::MobileEntityMotion&, Animation::TTime&);
		virtual void			Begin() override;
		virtual void			Update(float x) override;
		virtual void			End() override;
		virtual D::String		Describe() const override;
		virtual bool			IsMobileEntityMotion() const override {return true;}

		bool					EndsAt(const Grid::LCellIdI&loc) const {return path.rawPath.Last().first == loc;}
	private:
		struct Path
		{
			Buffer0<std::pair<Grid::LCellIdI,float> >	rawPath;
			bool				changed;

			struct TNode
			{
				float3	dir,
						normal,
						position;
			};

			Array<TNode>		nodes;
			//TUniqueID			targetID;
		

			/**/				Path():changed(true)	{}

			void				Rebuild(const Objects::PMobileEntity&target);
			void				ApplyTo(const Objects::PMobileEntity&target, float t);
			bool				Contains(const Grid::LCellIdI&loc)	{foreach (rawPath,p) if (p->first == loc) return true; return false;}
		};
		Path					path;
		Objects::PMobileEntity	allocatedTarget;
	};

}

