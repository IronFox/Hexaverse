#ifndef kernelCoreH
#define kernelCoreH

#include "../evolver.h"
#include <math/vector.h>

namespace Evolver
{

	class Core : public Abstract
	{
		typedef Abstract	Super;

		bool				active = false;
	protected:
		/**/				Core(const char*derivativeName):Super(derivativeName)	{}
	public:
		/**/				Core():Super("Core v1.0")	{}

		virtual bool		IsActive() const override {return active;}

		virtual void		BeginRoundProcessing() override;
		virtual bool		ContinueRoundProcessing() override;
		virtual void		FinishRoundProcessing() override;

		virtual void		HandleChainReactionExplosion(const TExplosion&) override;
	};

}


#endif
