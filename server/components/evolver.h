#pragma once

#include <global_string.h>
#include <grid/stack.h>

namespace Evolver
{
	struct TExplosion
	{
		Grid::LCellIdI		center = Grid::LCellIdI::Invalid;
		float				radius=1;
		int					damageLevel=1;
		float				atTime=0;
		Objects::PWorldEntity	causedBy;


		/**/				TExplosion()	{}
		/**/				TExplosion(const Objects::PWorldEntity&causedBy, const Grid::LCellIdI&center, float radius, int damageLevel, float atTime)
							:causedBy(causedBy),
							center(center),radius(radius),damageLevel(damageLevel),atTime(atTime)	{}
	};

	class Abstract
	{
	public:
		const String		name;



		float				animationLength = 1.f;

		/**/				Abstract(const char*name):name(name)	{}

		virtual void		BeginRoundProcessing()=0;
		virtual bool		ContinueRoundProcessing()=0;
		virtual void		FinishRoundProcessing()=0;
		
		virtual void		HandleChainReactionExplosion(const TExplosion&)=0;

		void				ProcessRound()	{BeginRoundProcessing(); while (ContinueRoundProcessing()); FinishRoundProcessing();}

		virtual bool		IsActive() const =0;


	};

	void				Create();
	Abstract*			Get(const String&name);
	Abstract*			GetDefault();
}
