#include "t2.h"
#include <global_string.h>


namespace Objects
{
	namespace T2
	{
		using namespace DeltaWorks;

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
