#pragma once
#include "../stringTable.h"
#include <interface/hashable.h>

namespace Objects
{
	using namespace DeltaWorks;


	struct TTypeComponent
	{
		ConstName	constType = ConstName::Empty;
		index_t		modDerivedIndex = InvalidIndex;	//if type is mod

		constexpr	TTypeComponent(){}

		constexpr	TTypeComponent(ConstName constType, index_t modDerivedIndex=InvalidIndex):constType(constType),modDerivedIndex(modDerivedIndex)
		{}

		constexpr bool		operator==(const TTypeComponent&other) const {return constType == other.constType && modDerivedIndex == other.modDerivedIndex;}
		constexpr bool		operator!=(const TTypeComponent&other) const {return !operator==(other);}

		friend String ToString(const TTypeComponent& component)
		{
			return component.ToNumericString();
		}


		friend hash_t	Hash(const TTypeComponent&t)
		{
			return HashValue() << t.constType << t.modDerivedIndex;
		}

		String		ToNumericString() const
		{
			if (constType == ConstName::Empty)
				return "<empty>";
			if (modDerivedIndex == InvalidIndex)
				return '#'+String(int(constType));
			return '#'+String(int(constType))+"m"+String(modDerivedIndex);
		}



		friend void Deserialize(DeltaWorks::IReadStream&s, TTypeComponent&v)
		{
			using Serialization::Deserialize;
			ConstName constType;
			index_t modDerivedIndex = InvalidIndex;
			Deserialize(s,constType);
			if (IsMod(constType))
				s.ReadSize(modDerivedIndex);
			v = TTypeComponent(constType,modDerivedIndex);

		}
		friend void Serialize(DeltaWorks::IWriteStream&s, const TTypeComponent&v)
		{
			using Serialization::Serialize;
			Serialize(s,v.constType);
			if (IsMod(v.constType))
				s.WriteSize(v.modDerivedIndex);
		}


		static bool IsMod(ConstName cnst)
		{
			switch (cnst)
			{
				case ConstName::Job_Mod:
				case ConstName::Turret_Mod:
				case ConstName::Type_Mod:
				case ConstName::Type_Tower_Mod:
				case ConstName::VehicleBase_Mod:
					return true;
			}
			return false;
		}

	};

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
		TTypeComponent	baseType=ConstName::Empty;
		/**
		Turret type which is in addition to the base type.
		Set only for dual types (Vehicles, Towers, ...).
		If set, baseType must also be set
		*/
		TTypeComponent	turretType=ConstName::Empty;

		bool			isT2 = false;

		constexpr		EntityType(){}
		constexpr		EntityType(TTypeComponent base, TTypeComponent turret=ConstName::Empty, bool t2=false):baseType(base),turretType(turret),isT2(t2)
		{
			DBG_ASSERT__(IsValid());
		}
		constexpr		EntityType(ConstName base, ConstName turret=ConstName::Empty, bool t2=false):baseType(base),turretType(turret),isT2(t2)
		{
			DBG_ASSERT__(IsValid());
		}
		constexpr		EntityType(TTypeComponent base, bool t2):baseType(base),isT2(t2)
		{
			DBG_ASSERT__(IsValid());
		}

		constexpr bool		operator==(const EntityType&other) const {return baseType == other.baseType && turretType == other.turretType;}
		constexpr bool		operator!=(const EntityType&other) const {return !operator==(other);}
		constexpr bool		operator==(TTypeComponent baseTypeName) const {return HasNoTurret() && this->baseType == baseTypeName;}
		constexpr bool		operator!=(TTypeComponent baseTypeName) const {return HasTurret() || this->baseType != baseTypeName;}
		constexpr bool		operator==(ConstName baseTypeName) const {return HasNoTurret() && this->baseType == baseTypeName;}
		constexpr bool		operator!=(ConstName baseTypeName) const {return HasTurret() || this->baseType != baseTypeName;}

		friend hash_t	Hash(const EntityType&t)
		{
			return HashValue() << t.baseType << t.turretType;
		}

		constexpr bool		IsEmpty() const {return baseType == ConstName::Empty;}
		constexpr bool		IsNotEmpty() const {return baseType != ConstName::Empty;}
		constexpr bool		HasTurret() const {return turretType != ConstName::Empty;}
		constexpr bool		HasNoTurret() const {return turretType == ConstName::Empty;}
		constexpr bool		IsValid() const		{return HasNoTurret() || (IsNotEmpty() && BaseRequiresTurret(baseType.constType));}
		constexpr bool		IsInvalid() const	{return HasTurret() && (IsEmpty() || !BaseRequiresTurret(baseType.constType));}

		String		ToNumericString() const
		{
			if (IsEmpty())
				return "<empty>";
			if (HasNoTurret())
				return baseType.ToNumericString();
			return turretType.ToNumericString()+":"+baseType.ToNumericString();
		}

		constexpr bool		IsVehicleBase() const
		{
			return turretType == ConstName::Empty && IsVehicleBase(baseType.constType);
		}

		static constexpr bool	IsVehicleBase(ConstName baseType)
		{
			switch (baseType)
			{
				case ConstName::VehicleBase_CatBase:
				case ConstName::VehicleBase_HeavyBase:
				case ConstName::VehicleBase_HoverBase:
				case ConstName::VehicleBase_Mod:
					return true;
			}
			return false;
		}

		static constexpr bool	BaseRequiresTurret(ConstName baseType)
		{
			return IsVehicleBase(baseType) || baseType == ConstName::Type_Tower || baseType == ConstName::Type_Tower_Mod;
		}

		constexpr bool IsVehicle() const
		{
			return HasTurret() && IsVehicleBase(baseType.constType);
		}

		friend void Deserialize(DeltaWorks::IReadStream&s, EntityType&v)
		{
			using Serialization::Deserialize;
			Deserialize(s,v.baseType);
			if (BaseRequiresTurret(v.baseType.constType))
				Deserialize(s,v.turretType);
			else
				v.turretType = ConstName::Empty;
		}
		friend void Serialize(DeltaWorks::IWriteStream&s, const EntityType&v)
		{
			using Serialization::Serialize;
			Serialize(s,v.baseType);
			if (BaseRequiresTurret(v.baseType.constType))
				Serialize(s,v.turretType);
		}
	};

}

