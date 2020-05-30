#include "resourceConverters.h"

namespace Objects
{


	#ifdef HAS_WINDOW
		/*virtual override*/ void		OrganicCompoundFactory::DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags, owner);
			label.MarkBriefEnd();
			label << "Enabled organic compound factories convert "+Resource::Fuel.GetTagStr()+" and "+Resource::CrystallineCompounds.GetTagStr()+" to " +
				Resource::OrganicCompounds.GetTagStr()+"."
			" Organic compound factories, like many structures, require "+Resource::Energy.GetTagStr()+" from generators to operate."
			;
		}
	#endif

	#ifndef CLIENT
		/*virtual override*/ void		OrganicCompoundFactory::Tick(Resource::Vector&v)
		{
			v.Add(Objects::Resource::Energy,-EnergyDemand);
			v.Add(Objects::Resource::CrystallineCompounds,-CrystallineDemand);
			v.Add(Objects::Resource::Fuel,-FuelDemand);
			v.Add(Objects::Resource::OrganicCompounds,OrganicOutput);
		}



	#endif

	/*virtual override*/  void		OrganicCompoundFactory::GetResourceVector(Resource::Vector&balance, Resource::Vector&storageCurrent, Resource::Vector&storageMax) const
	{
		balance.Add(Objects::Resource::Energy,-EnergyDemand);
		balance.Add(Objects::Resource::CrystallineCompounds,-CrystallineDemand);
		balance.Add(Objects::Resource::Fuel,-FuelDemand);
		balance.Add(Objects::Resource::OrganicCompounds,OrganicOutput);
	}









	

	#ifdef HAS_WINDOW
		/*virtual override*/ void		CrystallineCompoundFactory::DescribeType(Description&label, UINT32 descFlags, const OwnerParameter&owner)
		{
			label.BeginBrief();
			Super::DescribeType(label,  descFlags, owner);
			label.MarkBriefEnd();
			label << "Enabled crystalline compound factories convert "+Resource::Minerals.GetTagStr()+" and "+Resource::OrganicCompounds.GetTagStr()+" to " +
				Resource::CrystallineCompounds.GetTagStr()+"."
			" Crystalline compound factories, like many structures, require "+Resource::Energy.GetTagStr()+" from generators to operate."
			;
		}
	#endif

	#ifndef CLIENT
		/*virtual override*/ void		CrystallineCompoundFactory::Tick(Resource::Vector&v)
		{
			v.Add(Objects::Resource::Energy,-EnergyDemand);
			v.Add(Objects::Resource::OrganicCompounds,-OrganicDemand);
			v.Add(Objects::Resource::Minerals,-MineralDemand);
			v.Add(Objects::Resource::CrystallineCompounds,CrystallineOutput);
		}
	#endif

	/*virtual override*/  void		CrystallineCompoundFactory::GetResourceVector(Resource::Vector&balance, Resource::Vector&storageCurrent, Resource::Vector&storageMax) const
	{
		balance.Add(Objects::Resource::Energy,-EnergyDemand);
		balance.Add(Objects::Resource::OrganicCompounds,-OrganicDemand);
		balance.Add(Objects::Resource::Minerals,-MineralDemand);
		balance.Add(Objects::Resource::CrystallineCompounds,CrystallineOutput);
	}


}
