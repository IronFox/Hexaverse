#pragma once
#include "../stringTable.h"
#include <interface/hashable.h>

namespace Objects
{
	using namespace DeltaWorks;

	/**
	General purpose entity type descriptor
	*/
	class EntityType
	{
	public:
		/**
		Base type name. For common structures this is the only non-empty type.
		May contain turret-types, too, in some scenarios
		*/
		ConstName	baseType=ConstName::Empty;
		/**
		Turret type which is in addition to the base type.
		Set only for dual types (Vehicles, Towers, ...).
		If set, baseType must also be set
		*/
		ConstName	turretType=ConstName::Empty;

		/**/		EntityType(){}
		/**/		EntityType(ConstName base, ConstName turret=ConstName::Empty):baseType(base),turretType(turret)
		{
			DBG_ASSERT__(IsValid());
		}

		bool		operator==(const EntityType&other) const {return baseType == other.baseType && turretType == other.turretType;}
		bool		operator!=(const EntityType&other) const {return !operator==(other);}
		bool		operator==(ConstName baseTypeName) const {return HasNoTurret() && this->baseType == baseTypeName;}
		bool		operator!=(ConstName baseTypeName) const {return HasTurret() || this->baseType != baseTypeName;}

		friend hash_t	Hash(const EntityType&t)
		{
			return HashValue() << t.baseType << t.turretType;
		}

		bool		IsEmpty() const {return baseType == ConstName::Empty;}
		bool		IsNotEmpty() const {return baseType != ConstName::Empty;}
		bool		HasTurret() const {return turretType != ConstName::Empty;}
		bool		HasNoTurret() const {return turretType == ConstName::Empty;}
		bool		IsValid() const		{return HasNoTurret() || (IsNotEmpty() && BaseRequiresTurret(baseType));}
		bool		IsInvalid() const	{return HasTurret() && (IsEmpty() || !BaseRequiresTurret(baseType));}

		String		ToNumericString() const
		{
			if (IsEmpty())
				return "<empty>";
			if (HasNoTurret())
				return '#'+String(int(baseType));
			return '#'+String(int(turretType))+":#"+String(int(baseType));
		}

		static bool	IsVehicleBase(ConstName baseType)
		{
			switch (baseType)
			{
				case ConstName::VehicleBase_CatBase:
				case ConstName::VehicleBase_HeavyBase:
				case ConstName::VehicleBase_HoverBase:
					return true;
			}
			return false;
		}

		static bool	BaseRequiresTurret(ConstName baseType)
		{
			return IsVehicleBase(baseType) || baseType == ConstName::Type_Tower;
		}

		bool IsVehicle() const
		{
			return HasTurret() && IsVehicleBase(baseType);
		}

		friend void SerialSync(DeltaWorks::IReadStream&s, EntityType&v)
		{
			SerialSync(s,v.baseType);
			if (BaseRequiresTurret(v.baseType))
				SerialSync(s,v.turretType);
			else
				v.turretType = ConstName::Empty;
		}
		friend void SerialSync(DeltaWorks::IWriteStream&s, EntityType v)
		{
			SerialSync(s,v.baseType);
			if (BaseRequiresTurret(v.baseType))
				SerialSync(s,v.turretType);
		}
	};

}

