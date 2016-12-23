#ifndef objectResourceH
#define objectResourceH

#include <math/vclasses.h>
#include <container/buffer.h>
#include "../config.h"
#include "types.h"

namespace Objects
{
	CONSTRUCT_ENUMERATION5(ResourceWithMunition, Fuel, Minerals, Energy, Munitions, Uranium);
	CONSTRUCT_ENUMERATION4(ResourceWithoutMunition, Fuel, Minerals, Energy, Uranium);



	template <typename Resource>
		struct ResourceVectorT;

	typedef ResourceVectorT<Resource>	ResourceVector;

	class ResourceFlagSet
	{
	private:
		static_assert(Resource::N<=32,"Need other flag storage");
		typedef UINT32	flagT;
		flagT	isSet = 0;

		explicit	ResourceFlagSet(flagT set) : isSet(set)	{}
	public:
		/**/	ResourceFlagSet()	{}
		/**/	ResourceFlagSet(Resource res)	{Set(res);}
		/**/	ResourceFlagSet(Resource::value_t res)	{Set(res);}
		/**/	ResourceFlagSet(const ResourceVector&res)	{SetAllNonZero(res);}

		void	Set(Resource res)	{isSet |= ((flagT)1 << (flagT)res);}
		void	SetAllNonZero(const ResourceVector&res);
		
		void	operator|=(ResourceFlagSet res)	{isSet |= res.isSet;}
		ResourceFlagSet	operator|(ResourceFlagSet res) const	{return ResourceFlagSet(isSet | res.isSet);}
		bool	operator&(Resource res) const {return (isSet & ((flagT)1 << (flagT)res)) != 0;}
		bool	operator&(index_t resIndex) const {return (isSet & ((flagT)1 << (flagT)resIndex)) != 0;}
	};
	
	struct NaturalResourceVector
	{
		int		fuel,
				minerals,
				uranium;

		/*
		Minimum stack height is 2. Translation <3 = 2 means that there are only 4 yield on those areas. As a result a fuel mine on such a location cannot pay for itself.
		*/
		static const int TranslationFactor = 3;	//!< Translation factor used to scale resource vectors samples from the world

		/**/	NaturalResourceVector():fuel(0),minerals(0),uranium(0)	{}


		template <typename Resource>
		void	operator=(const ResourceVectorT<Resource>&vec)
		{
			this->fuel = vec.level[Resource::Fuel];
			this->minerals = vec.level[Resource::Minerals];
			this->uranium = vec.level[Resource::Uranium];
		}


		void	operator*=(float fc)
		{
			fuel *= fc;
			minerals *= fc;
			uranium *= fc;
		}
		bool	SampleWorld(index_t lowCellID);
		bool	IsNotEmpty() const {return fuel != 0 || minerals != 0 || uranium != 0;}
		bool	IsEmpty() const {return fuel == 0 && minerals == 0 && uranium == 0;}
	};

	template <typename Resource>
		struct ResourceVectorT
		{
			int		level[Resource::NumberOfPossibilities];
			static const count_t NumEntries = Resource::NumberOfPossibilities;

			typedef ResourceVectorT<Resource>	Self;

			/**/	ResourceVectorT()
			{
				Clear();
			}

			template<typename Resource2>
				void	LoadCompatible(const ResourceVectorT<Resource2>&other)
				{
					Resource2 otherID;
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					{
						const char*str = Resource::Reinterpret(i).ToString();
						if (otherID.Decode(str))
						{
							level[i] = other.level[otherID];
						}
						else
							level[i] = 0;
					}
				}
			template<>
				void	LoadCompatible<Resource>(const ResourceVectorT<Resource>&other)
				{
					operator=(other);
				}

			void	Clear()
			{
				memset(level,0,sizeof(level));
			}

			void	operator=(const NaturalResourceVector&v)
			{
				Clear();
				level[Resource::Fuel] = v.fuel;
				level[Resource::Minerals] = v.minerals;
				level[Resource::Uranium] = v.uranium;
			}

			bool	operator<(const Self&v) const
			{
				return VecUnroll<Resource::NumberOfPossibilities>::allLess(level,v.level);
			}
			bool	operator<=(const Self&v) const
			{
				return !VecUnroll<Resource::NumberOfPossibilities>::oneGreater(level,v.level);
			}

			void	operator+=(const NaturalResourceVector&v)
			{
				level[Resource::Fuel] += v.fuel;
				level[Resource::Minerals] += v.minerals;
				level[Resource::Uranium] += v.uranium;
			}
			void	operator+=(const Self&v)
			{
				VecUnroll<Resource::NumberOfPossibilities>::add(level,v.level);
			}
			void	operator-=(const Self&v)
			{
				VecUnroll<Resource::NumberOfPossibilities>::sub(level,v.level);
			}
			void	operator*=(int multiplier)
			{	
				VecUnroll<Resource::NumberOfPossibilities>::mul(level,multiplier);
			}
			bool	operator==(const Self&v) const
			{
				return VecUnroll<Resource::NumberOfPossibilities>::equal(level,v.level);
			}
			bool	operator!=(const Self&v) const
			{
				return !VecUnroll<Resource::NumberOfPossibilities>::equal(level,v.level);
			}
			Self	operator-(const Self&v)	const
			{
				Self rs(*this);
				rs -= v;
				return rs;
			}
			Self	operator*(int multiplier) const
			{	
				Self rs = *this;
				rs *= multiplier;
				return rs;
			}

			int	Sum() const
			{
				int rs = 0;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					rs += level[i];
				return rs;
			}

			int	SumNegative() const
			{
				int rs = 0;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if (level[i] < 0)
						rs += level[i];
				return rs;
			}

			int	SumPositive() const
			{
				int rs = 0;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if (level[i] > 0)
						rs += level[i];
				return rs;
			}

			void	SeparateBySign(Self&positive, Self&negative) const
			{
				static const Self zero;
				VecUnroll<Resource::NumberOfPossibilities>::max(level,zero.level,positive.level);
				VecUnroll<Resource::NumberOfPossibilities>::min(level,zero.level,negative.level);
			}

			void	Cap(const Self&cap)
			{
				VecUnroll<Resource::NumberOfPossibilities>::min(level,cap.level,level);
			}
			void	Negate()
			{
				VecUnroll<Resource::NumberOfPossibilities>::mul(level,-1);
			}
			String	ToString() const
			{
				Buffer0<String> rs;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if (level[i] != 0)
						rs << Resource::Reinterpret(i).ToString()+String(": ")+String(level[i]);
				return implode(',',rs.pointer(),rs.Count());
			}

			bool	IsEmpty() const
			{
				return VecUnroll<Resource::NumberOfPossibilities>::zero(level);
			}
			bool	IsNotEmpty() const
			{
				return !IsEmpty();
			}
			bool	IsStable() const
			{
				static const int compare0[Resource::NumberOfPossibilities] = {0};
				return !VecUnroll<Resource::NumberOfPossibilities>::oneLess(level,compare0);
			}

			bool	AnyIsSet(const ResourceFlagSet&set) const
			{
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					if ((set & i) && level[i] != 0)
						return true;
				return false;
			}
		};



	struct FullResourceVector
	{
		ResourceVector	supply, demand, balance, storage, storageMax;


		void	Include(const ResourceVector&v, const ResourceVector&storage, const ResourceVector&storageMax)
		{
			for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
			{
				int val = v.level[i];
				if (val > 0)
					supply.level[i] += val;
				else
					demand.level[i] += -val;
			}

			balance += v;
			balance += storage;
			this->storage += storage;
			this->storageMax += storageMax;
		}
	
	
	};




	static const char TextoutResourceTagOffset = (char)2;	//global char offset used for mineral output in strings
	static_assert((index_t)TextoutResourceTagOffset + Resource::NumberOfPossibilities+2 < (index_t)'\n',"Relocate tag offset");
	static inline char ResourceTag(index_t index) {return (char)(TextoutResourceTagOffset + (char)index);}
	static inline char TimeResourceTag() {return ResourceTag(Resource::NumberOfPossibilities);}
	static inline char TrafficResourceTag() {return ResourceTag(Resource::NumberOfPossibilities+1);}
	static inline bool IsTag(char c) {return c >= TextoutResourceTagOffset && (index_t)(c-TextoutResourceTagOffset) < Resource::NumberOfPossibilities+2;}

}


#endif
