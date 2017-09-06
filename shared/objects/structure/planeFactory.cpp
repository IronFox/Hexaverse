#include <global_root.h>
#include "planeFactory.h"
#include "../prototypes.h"
#include "../subOrbital.h"

namespace Objects
{
	Buffer0<ConstName>			PlaneFactory::prototypes;


	/*virtual override*/ void		PlaneFactory::GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&)	const
	{
		v.level[Resource::Energy] = -EnergyConsumption;
		v.level[Resource::Minerals] = -(int)MobileEntityProducer::productionPerRound;
		v.level[Resource::Fuel] = -FuelDemand;
	}
	
	/*virtual override*/ bool				PlaneFactory::AwaitsUserInteraction() const 
	{
		return  MobileEntityProducer::AwaitsUserInteraction();
	}
	
	/*virtual override*/ bool				PlaneFactory::CanProduce(const PWorldEntity&e) const 
	{
		return e->IsPlane() && prototypes.Contains(e->GetCTypeName());
	}

	/*virtual override*/ void				PlaneFactory::PrototypeSetup()
	{
		Super::PrototypeSetup();

		foreach (Objects::SubOrbital::prototypes,proto)
		{
			auto p = *proto;
			prototypes << p->GetCTypeName();
		}

	}
	

	#ifndef CLIENT
	
		/*virtual override*/ void			PlaneFactory::OnSleepChange(float&t)
		{
			Super::OnSleepChange(t);

			if (IsSleeping() && IsEnabled())
			{
				LogEvent(EventSignificance::Common,this,"Fell asleep. Disabling");
				SetEnabled(false,t);
			}
		}	

		/*virtual override*/ void				PlaneFactory::Tick(ResourceVector&v)
		{
			v.level[Resource::Energy] -= EnergyConsumption;
			v.level[Resource::Fuel] -= FuelDemand;

			float t = 0.f;	//buildings always come at the beginning
			MobileEntityProducer::Tick(v,t);
		}
	#else
		#ifdef HAS_WINDOW
			/*virtual override*/ void				PlaneFactory::VisitAllCommands(FCommandVisitor visitor, bool force)
			{
				MobileEntityProducer::VisitAllCommands(visitor,force);
				Super::VisitAllCommands(visitor,force);
			}
		#endif
		
	#endif
}

