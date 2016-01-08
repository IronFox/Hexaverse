#ifndef kernelCoreH
#define kernelCoreH

#include "../evolver.h"


namespace Evolver
{

	class Core : public Abstract
	{
		typedef Abstract	Super;
	protected:
		/**/	Core(const char*derivativeName):Super(derivativeName)	{}
	public:
		/**/	Core():Super("Core v1.0")	{}

		void	BeginRoundProcessing() override;
		bool	ContinueRoundProcessing() override;
		void	FinishRoundProcessing() override;
	};

}


#endif
