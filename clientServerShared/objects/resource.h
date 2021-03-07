#ifndef objectResourceH
#define objectResourceH

#include <math/vclasses.h>
#include <container/buffer.h>
#include "../config.h"
#include "types.h"
#include <container/hashtable.h>

namespace Objects
{

	/**
	Structures no longer used in contemporary runtime or io-operations.
	Cannot remove because of older save-games
	*/
	namespace Deprecated
	{
		CONSTRUCT_ENUMERATION5(ResourceWithMunition, 
			Fuel, 
			Minerals, 
			Energy, 
			Munitions, 
			Uranium
		);
		CONSTRUCT_ENUMERATION4(ResourceWithoutMunition, 
			Fuel, 
			Minerals, 
			Energy, 
			Uranium
		);


		template <typename Resource>
			struct ResourceVectorT
			{
				int		level[Resource::N];
				static const count_t NumEntries = Resource::N;
				typedef	Resource Enum;

				typedef ResourceVectorT<Resource>	Self;

				/**/	ResourceVectorT()
				{
					Clear();
				}

				void	Clear()
				{
					memset(level,0,sizeof(level));
				}

			};

	}

	/**
	R type declaration.
	A resource may be natural (e.g. fuel), intermediate (e.g. energy), or complex components (e.g. vehicle components).
	Natural and intermediate resources are declared automatically.
	Complex components are declared dynamically.
	*/
	namespace Resource
	{
		struct Type;
		class Table;

		struct ID
		{
			index_t		serialIndex;

			/**/		ID()	{}
			/**/		ID(index_t serialIdx):serialIndex(serialIdx)	{}

			bool		operator==(ID other) const {return serialIndex == other.serialIndex;}
			bool		operator!=(ID other) const {return serialIndex != other.serialIndex;}

			bool		IsValid() const {return serialIndex != InvalidIndex;}

			static const ID	Invalid;

			const Type&		ToType(const Table&) const;
			const Type&		ToType() const;
			char			GetTag(const Table&) const;
			char			GetTag() const;
			String			GetTagStr(const Table&) const;
			String			GetTagStr() const;
			const String&	ToString() const;
			const String&	GetIcon() const;

			ID				operator+(int delta) const {return ID(serialIndex + delta);}

			friend hash_t	Hash(const ID&id) {return (hash_t)id.serialIndex;}
		};

		struct TypePrototype
		{
			typedef TypePrototype Self;

			String		name, icon;	//if icon is empty, use name instead
			byte3		color;

			/**/		TypePrototype()	{}
			explicit	TypePrototype(String&&name, const float3&color):name(std::move(name)),
						color(color.r * 255, color.g*255, color.b*255) {}
			explicit	TypePrototype(String&&name, String&&icon, const float3&color):name(std::move(name)),
						icon(std::move(icon)),
						color(color.r * 255, color.g*255, color.b*255) {}

			void		swap(Self&other)
			{
				name.swap(other.name);
				icon.swap(other.icon);
				swp(color,other.color);
			}

			friend void swap(Self&a, Self&b)
			{
				a.swap(b);
			}
			
			friend void	Deserialize(IReadStream&s, Self&v)
			{
				Deserialize(s,v.name);
				Deserialize(s,v.icon);
				Deserialize(s,v.color);
			}
			friend void	Serialize(IWriteStream&s, const Self&v)
			{
				Serialize(s,v.name);
				Serialize(s,v.icon);
				Serialize(s,v.color);
			}

		};

		struct Type : public TypePrototype
		{
			typedef TypePrototype Super;
			typedef Type Self;

			//local/derived:
			index_t		serialIndex = InvalidIndex;
			char		tag = '\0';

			/**/		Type(){}
			explicit	Type(Super&&s) : Super(std::move(s))	{}
			explicit	Type(const Super&s) : Super(s)	{}
			explicit	Type(String&&name, const float3&color) : Super(std::move(name), color)	{}
			explicit	Type(String&&name, String&&icon, const float3&color) : Super(std::move(name), std::move(icon), color)	{}

			void		swap(Self&other)
			{
				Super::swap(other);
				swp(serialIndex,other.serialIndex);
				swp(tag,other.tag);
			}

			friend void swap(Self&a, Self&b)
			{
				a.swap(b);
			}

			ID			GetID() const {return ID(serialIndex);}
			String		GetTagStr() const;
			void		OnDeserialize(index_t serialIndex);
		};

		/**
		Maps resource indexes to otherwise not supported utf8-compatible characters
		*/
		namespace TagMap
		{
			static const count_t Count = 26;

			String		ToString(char tag);
			char		IDToTag(ID);
			char		IndexToTag(index_t);
			ID			TagToID(char);
			index_t		TagToIndex(char);
			bool		IsTag(char);
		}

		/**
		Serializable resource definition.
		*/
		class Table : private Buffer0<Type,Swap>
		{
			typedef Buffer0<Type,Swap> Super;
		public:
			/**/		Table(){};
			/**/		Table(const std::initializer_list<const char*>&names, const std::initializer_list<float3>&colors);
			ID			Append(const TypePrototype&);
			ID			Append(TypePrototype&&);
			ID			Append(const String&, const float3&);
			ID			Append(String&&, const float3&);
			ID			Append(const char*, const float3&);
			ID			Append(const String&, const String&icon, const float3&);
			ID			Append(String&&, String&&icon, const float3&);
			ID			Append(const char*, const char*icon, const float3&);
			void		Set(ID at, const char*, const char*icon, const float3&);
			bool		operator==(const Table&other) const;
			bool		operator!=(const Table&other) const {return !operator==(other);}
			bool		ResetChanged()	{bool old = changed; changed = false; return changed;}
			const Type*	FindByName(const String&) const;
			const Type*	GetTypeByTag(char tag) const
			{
				index_t at = TagMap::TagToIndex(tag);
				if (at >= Count())
					return nullptr;
				return Super::operator+(at);
			}
			template <typename ResourceEnum>
				void	LoadDeprecated()
				{
					Clear();
					for (index_t i = 0; i < ResourceEnum::N; i++)
						Append(ResourceEnum::Reinterpret(i).ToString());
				}

			friend void	Deserialize(IReadStream&s, Table&t)
			{
				const count_t cnt = s.ReadSize();
				t.Clear();
				for (index_t i = 0; i < cnt; i++)
				{
					Deserialize(s, t.Super::Append());
					t.Last().OnDeserialize(i);
				}
			}
			friend void	Serialize(IWriteStream&s, const Table&t)
			{
				s.WriteSize(t.Count());
				foreach (t,tp)
					Serialize(s,*tp);
			}

			inline count_t GetSize() const {return Super::Count();}

			const Type&	operator[](index_t idx) const {return Super::operator[](idx);}
			const Type&	operator[](ID idx) const {return Super::operator[](idx.serialIndex);}
			Super::Count;
			Super::GetLength;
		private:
			ID			Append(Type&&t);

			bool		changed = false;
		};

		/**
		Linearly ordered resources known the system.
		Each resource index matches its entry in this list.
		Filled by the server and synchronized to clients as needed
		*/
		extern Table lastTable, table;
		/**
		The minimal configuration using during previous versions of the game.
		*/
		extern const Table minimal;
		/**
		Locally configured resource table, depending on compiled feature set
		*/
		Table CompileLocalTable();

		/**
		Invariant indices of natural resources.
		The first three resources _must_ be natural resources as implemented by the original engine
		*/
		static const constexpr index_t FuelID = 0, MineralID = 1, UraniumID = 2;

		/**
		Non dynamic resources. These are always available and contained by minimal (Munitions may be omitted):
		*/
		extern const ID	
			Fuel, 
			Minerals,
			Uranium,
			Energy,
			Munitions,
			CrystallineCompounds,
			OrganicCompounds,
			CompositeMaterials,
			_;	//not a resource, just a terminal symbol so the previous one can end with a comma. do not use


		class FlagSet
		{
		public:
			typedef FlagSet Self;
			typedef UINT64	flag_t;
			struct TFlagSet
			{
				flag_t	set;

				/**/	TFlagSet(flag_t set):set(set)	{}
			};
			/**/	FlagSet(TFlagSet set) : isSet(set.set)	{}
			/**/	FlagSet()	{}
			/**/	FlagSet(index_t resourceIndex)		{Set(resourceIndex);}
			/**/	FlagSet(Resource::ID id) {Set(id);}

			void	Set(index_t resourceIndex)	{isSet |= ((flag_t)1 << (flag_t)resourceIndex);}
			void	Set(Resource::ID resource)	{Set(resource.serialIndex);}
			flag_t	GetNumeric() const {return isSet;}
			static Self	FromNumeric(flag_t num) {return Self(TFlagSet(num));}

			bool	IsEmpty() const {return isSet == 0;}
			bool	IsNotEmpty() const {return isSet != 0;}
		
			void	operator=(Resource::ID resource)	{isSet = ((flag_t)1 << (flag_t)resource.serialIndex);}

			void	operator|=(Self res)	{isSet |= res.isSet;}
			void	operator|=(Resource::ID res)	{Set(res);}
			void	operator|=(index_t res)	{Set(res);}
			Self	operator|(Self res) const	{return TFlagSet(isSet | res.isSet);}
			bool	operator&(index_t resIndex) const {return (isSet & ((flag_t)1 << (flag_t)resIndex)) != 0;}
			count_t	Count() const;
		private:
			flag_t	isSet = 0;
		};
	
		class Vector
		{
		public:
			typedef Vector Self;


			/**/	Vector(){}
			/**/	Vector(const CoreVector&v){operator=(v);}
			void	Clear()	{storage.Clear();}
			int		Get(index_t resource) const	{return operator[](resource);}
			int		Get(Resource::ID resource) const	{return operator[](resource.serialIndex);}
			void	Set(index_t, int);
			void	Set(Resource::ID idx, int v)	{Set(idx.serialIndex,v);}
			void	Add(index_t, int);
			void	Add(Resource::ID idx, int v)	{Add(idx.serialIndex,v);}
			bool	IsZero() const;
			bool	IsNonZero() const;
			void	operator=(const CoreVector&);
			void	operator+=(const CoreVector&v);

			void	ImportForeign(const Self&foreignData, const Resource::Table&foreignTable)
			{
				Clear();
				for (index_t i = 0; i < Resource::table.Count(); i++)
				{
					const Resource::Type*ot = foreignTable.FindByName(Resource::table[i].name);
					if (ot != nullptr)
						Set(i,foreignData.Get(ot->serialIndex));
				}
			}

			template <typename R>
				void	ImportDeprecated(const Deprecated::ResourceVectorT<R>&source)
				{
					Clear();
					for (index_t i = 0; i < R::N; i++)
					{
						const Resource::Type*t = Resource::table.FindByName(R::Reinterpret(i).ToString());
						if (t != nullptr)
							Set(t->serialIndex,source.level[i]);
					}
				}

			friend void	Deserialize(IReadStream&s, Self&v)
			{
				Deserialize(s,v.storage);
			}
			friend void	Serialize(IWriteStream&s, const Self&v)
			{
				Serialize(s,v.storage);
			}

			bool	operator<(const Self&v) const
			{
				for (index_t i = 0; i < Resource::table.Count(); i++)
					if (Get(i) >= v.Get(i))
						return false;
				return true;
			}
			bool	operator<=(const Self&v) const
			{
				for (index_t i = 0; i < Resource::table.Count(); i++)
					if (Get(i) > v.Get(i))
						return false;
				return true;
			}


			bool	operator>(const Self&v) const
			{
				for (index_t i = 0; i < Resource::table.Count(); i++)
					if (Get(i) <= v.Get(i))
						return false;
				return true;
			}
			bool	operator>=(const Self&v) const
			{
				for (index_t i = 0; i < Resource::table.Count(); i++)
					if (Get(i) < v.Get(i))
						return false;
				return true;
			}


			void	operator+=(const Self&v);
			void	operator-=(const Self&v);
			void	operator*=(int multiplier);
			bool	operator==(const Self&v) const;
			bool	operator!=(const Self&v) const;
			Self	operator-(const Self&v)	const;
			Self	operator+(const Self&v) const;
			/**
			Calculates how often the specified other vector fits into the local vector.
			Effectively the lowest resource division result, rounded down.
			If the other vector is zero, the result is the maximum representable int value.
			The result may be negative if the sign of the local and given vectors
			differ.
			*/
			int		operator/(const Self&v) const;
			Self	operator*(int multiplier) const;
			/**
			Sums up all levels
			*/
			int		Sum() const;
			/**
			Sums up only levels below 0.
			The result is negative or 0
			*/
			int		SumNegative() const;
			/**
			Sums up only levels above 0
			*/
			int		SumPositive() const;
			/**
			Separates the local vector into a positive and negative set
			@param[out] positive Target positive resource vector. Will contain positive or zero values only. Cleared before adding values (if any)
			@param[out] negative Target negative resource vector. Will contain negative or zero values only. Cleared before adding values (if any)
			*/
			void	SeparateBySign(Self&positive, Self&negative) const;
			/**
			Assures that no local level can exceed the respective level specified in @a limit.
			The behavior is undefined if the limit contains negative values
			*/
			void	Cap(const Self&limit);
			/**
			Sets all local levels to their negative value
			*/
			void	Negate();
			/**
			Generates a string representation of the local vector in the form "X: 3, Y: -1".
			Zero-values are not reported. The returned string may be empty
			*/
			String	ToString() const;
			/*
			{
				Ctr::Buffer0<String> rs;
				for (index_t i = 0; i < Resource::table.GetSize(); i++)
					if (level[i] != 0)
						rs << Resource::ID(i).ToString()+String(": ")+String(level[i]);
				return implode(',',rs.pointer(),rs.Count());
			}*/

			/**
			Checks whether all local levels are 0 or higher
			*/
			bool	IsStable() const;
			bool	AnyIsNegative() const	{return !IsStable();}
			bool	AnyIsNonZeroPositive() const;
			/**
			Checks if any local level of the specified resource flag set is non-0
			*/
			bool	AnyIsSet(const FlagSet&set) const;

			/**
			Maps all resource indexes where the local vector contains a non-0 value
			*/
			FlagSet		IdentifySet() const;
			count_t		GetSize() const {return storage.Count();}
			int			operator[](index_t resource) const;
			int			operator[](Resource::ID resource) const	{return operator[](resource.serialIndex);}
		private:
			Array<int>	storage;

			void		Tidy();
			int&		Require(index_t idx);
			int&		Require(Resource::ID idx) {return Require(idx.serialIndex);}
		};

		struct CoreVector
		{
			typedef CoreVector Self;
			typedef unsigned T;

			union
			{
				struct
				{
					T	fuel,
						minerals,
						uranium;
				};
				T		array[3];
			};

			/*
			Minimum stack height is 2. Translation <3 = 2 means that there are only 4 yield on those areas. As a result a fuel mine on such a location cannot pay for itself.
			*/
			static const int TranslationFactor = 3;	//!< Translation factor used to scale resource vectors samples from the world

			/**/	CoreVector():fuel(0),minerals(0),uranium(0)	{}

			friend hash_t	Hash(const Self&s)
			{
				return HashValue().AddGeneric(s.fuel).AddGeneric(s.minerals).AddGeneric(s.uranium);
			}

			bool	operator==(const Self&other) const
			{
				return fuel == other.fuel && minerals == other.minerals && uranium == other.uranium;
			}

			bool	operator!=(const Self&other) const
			{
				return !operator==(other);
			}

			void	operator=(const Vector&vec)
			{
				this->fuel = vec.Get(Resource::Fuel);
				this->minerals = vec.Get(Resource::Minerals);
				this->uranium = vec.Get(Resource::Uranium);
			}


			void	operator*=(float fc)
			{
				fuel *= fc;
				minerals *= fc;
				uranium *= fc;
			}

			T		GetSum() const {return fuel + minerals + uranium;}

			constexpr count_t Count() const {return ARRAYSIZE(array);}

			T		operator[](index_t resIdx) const
			{
				DBG_ASSERT_LESS__(resIdx, Count());
				return array[resIdx];
			}

			ID	GetResource(index_t resIdx) const
			{
				switch (resIdx)
				{
					case 0:
						return Fuel;
					case 1:
						return Minerals;
					case 2:
						return Uranium;
					static_assert(ARRAYSIZE(array)==3,"Expand switch here");
				}
				FATAL__("Bad resource index given: "+String(resIdx));
				return Fuel;
			}
	

			bool	SampleWorld(index_t lowCellID, bool translateToActual=true);
			bool	IsNotEmpty() const {return fuel != 0 || minerals != 0 || uranium != 0;}
			bool	IsEmpty() const {return fuel == 0 && minerals == 0 && uranium == 0;}

			friend void	Deserialize(IReadStream&s, Self&v)
			{
				v.fuel = (T)s.ReadSize();
				v.minerals = (T)s.ReadSize();
				v.uranium = (T)s.ReadSize();
			}
			friend void	Serialize(IWriteStream&s, const Self&v)
			{
				s.WriteSize(v.fuel);
				s.WriteSize(v.minerals);
				s.WriteSize(v.uranium);
			}
		};



		struct FullVector
		{
			Vector	supply, demand, balance, storage, storageMax;


			void	Include(const Vector&v, const Vector&storage, const Vector&storageMax);
		};



		class PositiveVector : public Vector
		{
		public:
			typedef Vector Super;
			typedef PositiveVector Self;

			/**/	PositiveVector()	{}
			/**/	PositiveVector(const Super&v);

			void	swap(Self&other)
			{
				std::swap((Super&)*this,(Super&)other);
			}


			friend void	Deserialize(IReadStream&s, Self&v)
			{
				Deserialize(s,(Super&)v);
			}
			friend void	Serialize(IWriteStream&s, const Self&v)
			{
				Serialize(s,(const Super&)v);
			}

			static	void					Serialize(IWriteStream&outStream, const Super&vec);
			static	void					Deserialize(IReadStream&inStream, Super&vec);


			bool		operator==(const Super&vec) const
			{
				return vec == *this;
			}
			bool		operator!=(const Super&vec) const
			{
				return vec != *this;
			}

			void		operator=(const Super&vec)
			{
				((Super&)*this) = vec;
			}
			void		operator=(const CoreVector&vec)
			{
				((Super&)*this) = vec;
			}	
		};


	}




	//static const char TextoutResourceTagOffset = (char)2;	//global char offset used for mineral output in strings
	//static_assert((index_t)TextoutResourceTagOffset + Resource::table.GetSize()+2 < (index_t)'\n',"Relocate tag offset");
	//static inline char ResourceTag(index_t index) {return (char)(TextoutResourceTagOffset + (char)index);}
	static inline char TimeResourceTag() {return (char)1;}
	static inline char TrafficResourceTag() {return (char)2;}
	static inline bool IsTag(char c) {return c == TimeResourceTag() || c == TrafficResourceTag() || Resource::table.GetTypeByTag(c) != nullptr;}
	String GetTagName(char c);
}


#endif
