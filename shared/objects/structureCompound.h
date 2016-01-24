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
	public:
		static const UINT32 InvalidNetwork = 0xFFFFFFFF;

		#if defined WIRELESS_VEHICLES && defined CLIENT
			float4					color;
		#endif

		Buffer0<WWorkingStructure>	workingStructures;
		Buffer0<WStructure>			allStructures;
		FullResourceVector			total;
		#ifndef CLIENT
//			#ifdef CONNECTOR_LESS
				ResourceVector		overproduction;	//set during initial phases, cleared before animation phase
	//		#endif
		#endif

		bool						stable;
		//ID							hashIdentifier;


		void				Clear()
		{
			total = FullResourceVector();
			workingStructures.Clear();
			allStructures.Clear();
			#ifndef CLIENT
				overproduction = ResourceVector();
			#endif
		}

		void				swap(Compound&other)
		{
			workingStructures.swap(other.workingStructures);
			allStructures.swap(other.allStructures);
			swp(total,other.total);
			swp(stable,other.stable);
			#ifndef CLIENT
				swp(overproduction,other.overproduction);
			#endif
			//swp(hashIdentifier,other.hashIdentifier);
		}
		void				Compact()
		{
			workingStructures.Compact();
			allStructures.Compact();
		}

		void				AppendStructure(Structure*s);
		#ifndef CLIENT
			void			TickBegin();
			void			TickEnd(float atTime);

			void			FeedLeftovers(ResourceVector&vector, float atTime);


			void			ChangeStorage(ResourceVector&supply, float atTime);
			bool			CanEnable(const WorkingStructure*, String&outWhy) const;
			FullResourceVector	GetVector(const Array<bool>&virtualEnabled) const;
		#endif
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

	#ifndef CONNECTOR_LESS
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
