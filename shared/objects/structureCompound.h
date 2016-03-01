#ifndef objectsStructureCompoundH
#define objectsStructureCompoundH

#include "../config.h"
#include "types.h"
#include <container/buffer.h>
#include "resource.h"
#include <functional>

namespace Objects
{






	
	class Compound 
	{
		static int					GetEnableChangeBalance(const ResourceVector&local);
		bool						FindBestEnableCandidates(PWorkingStructure*bestOut, int&bestBalance, ResourceVector&balance,  index_t recursionDepth, int parentSum, const WWorkingStructure*parent) const;
		bool						FindBestEnableCandidates(PWorkingStructure*bestOut, count_t bestOutCount) const;

		static void					TempSetEnabled(WorkingStructure*,bool);
	public:
		static const UINT32 InvalidNetwork = 0xFFFFFFFF;



		Buffer0<WWorkingStructure>	containedStructures;
		FullResourceVector			total;
		#ifndef CLIENT
			#ifdef ONE_COMPOUND_PER_FACTION
				ResourceVector		overproduction;	//set during initial phases, cleared before animation phase
			#endif
		#endif

		bool						stable;
		//ID							hashIdentifier;

		#ifndef CLIENT
			static WorkingStructure*lastDisabled;
		#endif

		void				Clear()
		{
			total = FullResourceVector();
			containedStructures.Clear();
			#ifndef CLIENT
				#ifdef ONE_COMPOUND_PER_FACTION
					overproduction = ResourceVector();
				#endif
			#endif
		}

		void				swap(Compound&other)
		{
			containedStructures.swap(other.containedStructures);
			swp(total,other.total);
			swp(stable,other.stable);
			#ifndef CLIENT
				#ifdef ONE_COMPOUND_PER_FACTION
					swp(overproduction,other.overproduction);
				#endif
			#endif
			//swp(hashIdentifier,other.hashIdentifier);
		}
		void				Compact()
		{
			containedStructures.Compact();
		}

		void				AppendStructure(Structure*s);
		#ifndef CLIENT
			void			TickBegin();
			void			TickEnd(float atTime);

			void			FeedLeftovers(ResourceVector&vector, float atTime);


			void			Collect(ResourceVector demand, float atTime);
			void			Distribute(ResourceVector supply, float atTime);
			FullResourceVector	GetVector(const Array<bool>&virtualEnabled) const;
		#endif
		bool				CanEnable(WorkingStructure*, String&outWhy, bool ignoreCanEnable=false) const;
		void				RefreshState(float atTime);
		FullResourceVector	GetVector() const;



		//struct Transfer
		//{
		//	static bool		isActive;
		//	static UINT32	toNetwork;
		//	static PVehicle	fromVehicle;

		//	static void		Abort()	{isActive = false; toNetwork = 0; fromVehicle.reset();}
		//};

	};

	#ifndef ONE_COMPOUND_PER_FACTION
		/**
		Linear list of all compounds. This list is reconstructed whenever changes to the compound structure
		are detected or at least assumed.
		The contained objects are reconstructed each time, and referencable only by integer
		*/
		extern Buffer0<Compound,Swap>		compounds;
	#endif

	void RefreshCompoundStates( float atTime);


	/**
	Iterates through all currently known compounds, including that of the Nexus faction
	*/
	void ForeachCompound(const std::function<void(Compound&)>&func);


}


#endif
