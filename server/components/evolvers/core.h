#ifndef kernelCoreH
#define kernelCoreH

#include "../evolver.h"


namespace Evolver
{

	class Core : public Abstract
	{
	public:
		/**/	Core():Abstract("Core v1.0")	{}

		void	BeginRoundProcessing() override;
		bool	ContinueRoundProcessing() override;
		void	FinishRoundProcessing() override;
	};

}


#endif
