#include "t2.h"
#include <global_string.h>
#include "mobileEntity.h"
#include "faction.h"
#include "interface.h"

namespace Objects
{
	namespace T2
	{
		using namespace DeltaWorks;

		Properties::Properties()
		{
			#ifdef _DEBUG
				experience = 1000;
			#endif
		}



		#ifndef CLIENT
		void Upgrade(MobileEntity&v, const PWorldEntity&target)
		{
			if (!v.CanUpgrade())
			{
				LogEvent(EventClass::Preset::commandRejected,&v,v.GetName()+ " can not upgrade other units");
				return;
			}
			if (!target || !v.owner->CanSee(target,v.entityTime) || target.get() == &v)
			{
				LogEvent(EventClass::Preset::commandRejected,&v,"Upgrade: Nothing here");
				return;
			}
			if (target->t2 && !target->t2->CanLevelUp())
			{
				LogEvent(EventClass::Preset::commandRejected,&v,"Upgrade: '"+target->GetNiceName()+"' is not ready for upgrade");
				return;
			}



			const auto storage = GetStorage(v,Research::globalState.t2.upgradeCostType);


			if (Research::globalState.t2.upgradeCostQuantity > storage)
			{
				LogEvent(EventClass::Preset::important,&v,"Upgrade: Insufficient "+String(Research::globalState.t2.upgradeCostType.ToString())+" in storage to upgrade '"+target->GetNiceName()+"'. Needs "+String(Research::globalState.t2.upgradeCostQuantity - storage)+" more");
				ApplicationInterface::SignalOperationLacksResources(v,Research::globalState.t2.upgradeCostType,v.entityTime);
				return;
			}

			ChangeStorage(v,v.entityTime, Research::globalState.t2.upgradeCostType,-Research::globalState.t2.upgradeCostQuantity);

			if (!target->t2)
				target->t2.reset(new Objects::T2::Properties());
			else
				target->t2->LevelUp();
			Objects::Research::Recompute(*target->t2);

			LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ) ,&v,"'"+target->GetNiceName()+"' upgraded to "+target->GetTypeNameAppendix());

			target->SignalChangeByExternalCause(WorldEntity::T2Changed,v.entityTime);
		}
		#endif


		String	TModifier::Describe() const
		{
			if (multiplier == 1)
				return "";
			String rs = " ";
			if (multiplier != 1)
				rs += "*"+String(multiplier);
			return rs + "(T2)";
		}

		void		Properties::GainXP(UINT32 xp)
		{
			experience += xp;
			xpChanged = true;
			changed = true;
		}

		void		Properties::SetLevel(UINT32 level)
		{
			if (level == this->level)
				return;
			this->level = level;
			lvlChanged = true;
			changed = true;
		}

		void		Properties::LevelUp()
		{
			if (!CanLevelUp())
				throw Except::Program::UnsupportedRequest(CLOCATION,"Insufficient experience to achieve next level");
			SetLevel(level + 1);
		}




		/*virtual override*/ void Properties::Pack(IWriteStream&s,Target target) const
		{
			s.WritePrimitive(level);
			if (target == Target::Hostile)
			{
				s.WritePrimitive(false);
 			}
			else
			{
				s.WritePrimitive(true);

				s.WritePrimitive(experience);
				s.WritePrimitive(currentLevelAtXp);
				s.WritePrimitive(nextLevelAtXP);

				s.WritePrimitive(initiative);
				s.WritePrimitive(speed);
				s.WritePrimitive(maxHealth);
				s.WritePrimitive(scanningRange);
				s.WritePrimitive(attacksPerRound);
				s.WritePrimitive(damageLevel);
				s.WritePrimitive(explosionRadius);
				s.WritePrimitive(horizontalAttackRange);
				
				s.WritePrimitive(healthRegeneration);
			}
		}

		/*virtual override*/ void Properties::Unpack(IReadStream&s)
		{
			s.ReadPrimitive(level);
			bool detailed;
			s.ReadPrimitive(detailed);
			if (detailed)
			{
				s.ReadPrimitive(experience);
				s.ReadPrimitive(currentLevelAtXp);
				s.ReadPrimitive(nextLevelAtXP);

				s.ReadPrimitive(initiative);
				s.ReadPrimitive(speed);
				s.ReadPrimitive(maxHealth);
				s.ReadPrimitive(scanningRange);
				s.ReadPrimitive(attacksPerRound);
				s.ReadPrimitive(damageLevel);
				s.ReadPrimitive(explosionRadius);
				s.ReadPrimitive(horizontalAttackRange);
				s.ReadPrimitive(healthRegeneration);
			}
			else
			{
				experience = currentLevelAtXp = nextLevelAtXP = 0;

				initiative.Reset();
				speed.Reset();
				maxHealth.Reset();
				scanningRange.Reset();
				attacksPerRound.Reset();
				damageLevel.Reset();
				explosionRadius.Reset();
				horizontalAttackRange.Reset();
				healthRegeneration.Reset();
			}
		}

		/*virtual override*/ void Properties::ToSavegame(IWriteStream&s) const
		{
			s.WritePrimitive(SerialVersion);
			s.WritePrimitive(experience);
			s.WritePrimitive(level);
		}

		/*virtual override*/ void Properties::FromSavegame(IReadStream&s)
		{
			auto v = SerialVersion;
			s.ReadPrimitive(v);
			if (v != SerialVersion)
				throw Except::IO::DriveAccess::FileFormatFault(CLOCATION,"Expected version "+String(SerialVersion)+" but found "+String(v));
			s.ReadPrimitive(experience);
			s.ReadPrimitive(level);
			xpChanged = true;
			lvlChanged = true;
			changed = true;
		}
	}

}
