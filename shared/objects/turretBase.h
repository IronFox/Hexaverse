#ifndef objectTurretBaseH
#define objectTurretBaseH

#include <container/queue.h>
#include <container/hashtable.h>
#include "../sphereGrid.h"
#include "components.h"

#ifdef HAS_WINDOW
	#include <al/openal.h>
#endif


namespace Objects
{

	//class Faction;
	//typedef std::shared_ptr<Faction>	PFaction;
	//typedef std::weak_ptr<Faction>		WFaction;

	//typedef PFaction		OwnerParameter;



	#ifdef HAS_WINDOW
		/**
		Used to cache and render the paraboloid above a turret base
		*/
		class ParaboloidBuffer
		{
			TVec3<> lastCenter;
			float lastRange = 0.f;
			float lastExp = 0.f;
			TMatrix4<> matrix;
			DX11::InstructionBlock32	attackInstruction,extendedInstruction;

			static void		MakeHull(DX11::InstructionBlock32&target, const TMatrix4<>&matrix, float flatRange, float explosionRadius, const TVec3<>&center);

		public:
			void			Render(const Grid::LCellIdI&cell, float zOffset, float flatRange, float explosionRadius, const float4&color, bool extended);
		};
	#endif

	class TurretBase 
	{
		class AreaHelper
		{
		public:
			Queue<Grid::LCellIdI>	queue;
			GenericHashSet<Grid::LCellIdI,Grid::LCellIdI>	visited;

			bool				operator>>(Grid::LCellIdI&next)
			{
				return queue >> next;
			}

			void				operator<<(const Grid::LCellIdI&next)
			{
				if (!visited.SetNew(next))
					return;
				queue << next;
			}

			void				Clear()
			{
				queue.Clear();
				visited.Clear();
			}

		};


	public:
		static bool				CheckArea(const OwnerParameter&myFaction, const Grid::LCellIdI&, float explosionRadius);

		Component::Turret*		turret;
		#ifdef CLIENT
			float				turretAngleX, turretAngleY;
		#endif
		#ifdef HAS_WINDOW
			OpenAL::Source		activitySound;	//used for rotation by offensive turrets, and arbitrary sounds by others
		#endif
									

		#ifdef HAS_WINDOW
			ParaboloidBuffer	paraboloidBuffer;
		#endif


		/**/					TurretBase(Component::Turret*turret):turret(turret)
		#ifdef CLIENT
												,turretAngleX(0), turretAngleY(0)
		#endif
								{
									#ifdef HAS_WINDOW
									//	rotationSound.Create();
									#endif
								}
		virtual					~TurretBase()	{}


		virtual float			GetAttackActionLeft() const=0;

		int						GetAttacksPerRound() const	{return turret->GetAttacksPerRound(GetOwner());}
		int						GetDamagePotentialPerRound() const {OwnerParameter owner = GetOwner(); return std::max(1,turret->GetAttacksPerRound(owner)) * turret->GetDamageLevel(owner);}
		float					GetAttackInitiative()const	{return turret->initiative * GetAttackActionLeft();}
		int						CountPossibleAttacks() const {return GetAttacksPerRound() * GetAttackActionLeft();}
		virtual bool			NextTaskIsAttack() const = 0;
		virtual float			GetFlatAttackRange() const = 0;
		virtual String			DescribeFlatAttackRange() const = 0;

		/**
		Checks if there are enough resources available for a single shot.
		Cannot be const due to the complexity of resource allocation of mobile entities.
		Must not persistently alter the local entity, however.
		*/
		virtual bool			HasAmmoForShot() = 0;
		virtual OwnerParameter	GetOwner() const = 0;
		
		/**
		Determines whether or not the local turret base can hit a given location.
		Provides the precise aiming target if hitting is possible only through explosion radius extension.
		@param source Attacking turret location
		@param destIn Original destination location
		@param destOut Final destination location. If the given location lies within the local attack range, destOut will match destIn.
			Otherwise a close approximation will be given that is still in range but will hit the target through the explosion radius
		@return true if an applicable destination was found, false otherwise
		*/
		bool					ExtCanHit(const OwnerParameter&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, Grid::LCellIdI&destOut)	const;




		#ifndef CLIENT
			virtual void		Attack(float&currentTime) = 0;
			virtual bool		FrontQueuePossibleAttacks() = 0;
		#endif
	};

	typedef std::shared_ptr<TurretBase>	PTurretBase;
}

#endif
