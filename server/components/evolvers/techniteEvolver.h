#ifndef techniteCoreH
#define techniteCoreH

#include "core.h"


namespace Evolver
{

	class Technite : public Core
	{
		typedef Core	Super;
	public:
		/**/	Technite():Super("Technite v1.0")	{}

		bool	ContinueRoundProcessing() override;
		void	FinishRoundProcessing() override;
	};

}


#endif
