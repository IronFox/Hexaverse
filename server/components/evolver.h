#ifndef componentKernelH
#define componentKernelH

#include <global_string.h>
#include <sphereGrid.h>

namespace Evolver
{
	struct TExplosion
	{
		Grid::LCellIdI		center = Grid::LCellIdI::Invalid;
		float				radius=1;
		int					damageLevel=1;
		float				atTime=0;


		/**/				TExplosion()	{}
		/**/				TExplosion(const Grid::LCellIdI&center, float radius, int damageLevel, float atTime):center(center),radius(radius),damageLevel(damageLevel),atTime(atTime)	{}
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



	};

	void				Create();
	Abstract*			Get(const String&name);
	Abstract*			GetDefault();
}

#endif
