#ifndef componentKernelH
#define componentKernelH

#include <global_string.h>

namespace Evolver
{

	class Abstract
	{
	public:
		const String		name;



		float				animationLength = 1.f;

		/**/				Abstract(const char*name):name(name)	{}

		virtual void		BeginRoundProcessing()=0;
		virtual bool		ContinueRoundProcessing()=0;
		virtual void		FinishRoundProcessing()=0;

		void				ProcessRound()	{BeginRoundProcessing(); while (ContinueRoundProcessing()); FinishRoundProcessing();}



	};

	void				Create();
	Abstract*			Get(const String&name);
	Abstract*			GetDefault();
}

#endif
