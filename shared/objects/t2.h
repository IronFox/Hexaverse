#pragma once

#include "../typeDecl.h"
#include "packable.h"
#include <global_string.h>

namespace Objects
{
	namespace T2
	{

		struct TModifier
		{
			float		multiplier=1;

			int			Apply(int value) const
			{
				return ceil(multiplier * value);
			}
		
			float		Apply(float value) const
			{
				return multiplier * value;
			}

			void		Reset()
			{
				multiplier = 1;
			}

			bool		Update(float mult)
			{
				bool rs = this->multiplier != mult;
				this->multiplier = mult;
				return rs;
			}

			D::String	Describe() const;
		};


		static_assert(sizeof(TModifier) == 4,"Bad packing");

		class Properties : public Packable
		{
		public:
			static const UINT8 SerialVersion = 2;
			bool		xpChanged = true,
						lvlChanged = true,
						changed = true;

			UINT32		experience=0,
						level=1,
						currentLevelAtXp=0,
						nextLevelAtXP=100000;
			TModifier	initiative,
						speed,
						maxHealth,
						scanningRange,
						attacksPerRound,
						damageLevel,
						explosionRadius,
						horizontalAttackRange,
						healthRegeneration;	//relative to max health, rounded up


			void		GainXP(UINT32 xp);
			void		SetLevel(UINT32 newLevel);
			void		LevelUp();
			bool		CanLevelUp() const {return experience >= nextLevelAtXP;}

			virtual void Pack(D::IWriteStream&s,Target target) const override;
			virtual void Unpack(D::IReadStream&s) override;
			virtual void ToSavegame(D::IWriteStream&s) const override;
			virtual void FromSavegame(D::IReadStream&s) override;
		};
	}



}


