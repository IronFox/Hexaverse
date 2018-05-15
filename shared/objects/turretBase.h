#ifndef objectTurretBaseH
#define objectTurretBaseH

#include <container/queue.h>
#include <container/hashtable.h>
#include "../grid/stack.h"
#include "components.h"
#include "clientAttachment.h"

#ifdef HAS_WINDOW
	#include <al/openal.h>
#endif


namespace Objects
{

	//class Faction;
	//typedef std::shared_ptr<Faction>	PFaction;
	//typedef std::weak_ptr<Faction>		WFaction;

	//typedef PFaction		OwnerParameter;

	


	class TurretBase 
	{


	public:

		class TargetChoice
		{
		public:
			Grid::LCellIdI		cellTarget;
			ImpactDetails		impact;
			Buffer0<M::TVec4<> > trajectory;

			#ifdef _DEBUG
				struct TField
				{
					Grid::LCellIdI cell;
					bool	outOfRange=false;
					bool	friendlyFound=false;
				};

				Buffer0<TField>	fieldsChecked;
				M::TVec3<>		explosionCenter;
				float			explosionRadius;
			#endif
		};


		static bool				CheckArea(const PFaction&myFaction, const Grid::LCellIdI&, const M::TVec3<>&worldPos,float explosionRadius,TargetChoice&);

		Component::Turret*		turret;
		#ifdef CLIENT
			float				turretAngleX, turretAngleY;
		#endif
		#ifdef HAS_WINDOW
			Engine::OpenAL::Source	activitySound;	//used for rotation by offensive turrets, and arbitrary sounds by others
		#endif
									

		#ifdef HAS_WINDOW
			PClientAttachment	paraboloidBuffer;
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

		int						GetAttacksPerRound() const;
		int						GetDamagePotentialPerRound() const;
		float					GetAttackInitiative()const	{return turret->initiative * GetAttackActionLeft();}
		int						CountPossibleAttacks() const {return GetAttacksPerRound() * GetAttackActionLeft();}
		virtual bool			NextTaskIsAttack() const = 0;
		virtual float			GetFlatAttackRange() const = 0;
		virtual float			GetAttackExplosionRadius() const = 0;
		virtual String			DescribeFlatAttackRange() const = 0;
		virtual WorldEntity&	ToEntity() = 0;
		virtual const WorldEntity&	ToEntity() const = 0;
		virtual float			GetZOffset() const = 0;
		virtual bool			AttacksMovingHostilesDuringMotionPhase() const {return turret->attacksMovingHostilesDuringMotionPhase;}

		/**
		Checks if there are enough resources available for a single shot.
		Cannot be const due to the complexity of resource allocation of mobile entities.
		Must not persistently alter the local entity, however.
		*/
		virtual bool			HasAmmoForShot() = 0;

		/**
		Retrieves the amount of resources that can be converted into the specified operation per action by the local entity
		@return Amount of consumed (or virtual) resources that can be converted per action by the local entity. 0 for entities that cannot perform the requested operation
		*/
		virtual int				GetOperationSpeed(Component::operation_t) const = 0;
		virtual String			DescribeOperationSpeed(Component::operation_t) const = 0;
		bool					CanAttack();
		bool					CanAttackIgnoreAmmo() const;


		/**
		Determines whether or not the local turret base can hit a given location.
		Provides the precise aiming target if hitting is possible only through explosion radius extension.
		@param source Attacking turret location
		@param destIn Original destination location
		@param destOut Final destination location. If the given location lies within the local attack range, destOut will match destIn.
			Otherwise a close approximation will be given that is still in range but will hit the target through the explosion radius
		@return true if an applicable destination was found, false otherwise
		*/
		bool					ExtCanHit(const PFaction&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, TargetChoice&outExtResult)	const;

		static bool				FindTarget(const PFaction&myFaction, TrajectoryType trajectoryType, float flatAttackRange, float explosionRadius, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, TargetChoice&outExtResult);



		#ifndef CLIENT
			/**
			Flushes auto attacks from the front of the queue until a non auto attack is encountered or the queue is empty
			*/
			virtual void		FlushAutoAttacks()=0;
			virtual bool		AttackNext(const Grid::LCellIdI&target, bool autoTarget) = 0;
			/**
			Checks whether or not the local turret base would attack the given target.
			Checks munition availability, range, and target ownership. Also checks for collateral damage.
			If a valid attack trajectory was found, it is stored in @a outTarget, and true is returned.
			@param target Attack target. Must not be null
			@param[out] outTarget Found attack trajectory. Content is undefined if the method returns false
			@return true if the conditions for firing were met, and a good attack trajectory was found, false otherwise
			*/
			bool				WouldAutoAttack(const PWorldEntity&target, TurretBase::TargetChoice&outTarget, float entityTime);
			virtual void		Attack(float&currentTime) = 0;

			/**
			Queues a single auto-target to the front of the local task queue.
			If the number of possible attacks per round are exhausted, insufficient ammunition is available,
			or the local entity state/type otherwise prohibits auto-targeting, then the method returns false.
			Internally typically uses GetSentryTarget()
			@param[out] outChoice Container for data related to the chosen target
			@return true if a target has been queued, false otherwise. For multi-targeting, the method may be called until it returns false
			*/
			virtual bool		FrontQueueAutoAttack(TurretBase::TargetChoice&outChoice) = 0;
		#endif

	private:
		class AreaHelper
		{
		public:
			Queue<Grid::LCellIdI>	queue;
			HashSet<Grid::LCellIdI>	visited;

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
		static bool				IsSafeHit(const PFaction&myFaction, TrajectoryType t, float range,const World::Data&visibleWorld, const World::Data&actualWorld,const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, float explosionRadius, TargetChoice&);

	};

	typedef std::shared_ptr<TurretBase>	PTurretBase;
}

#endif
