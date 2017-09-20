#ifndef objectPrototypesH
#define objectPrototypesH

#include "types.h"
#include "../stringTable.h"
#include <container/buffer.h>
#include <global_string.h>
#include <functional>
#include <container/hashtable.h>
#include "entityType.h"

namespace Objects
{
	typedef std::function<bool(const PWorldEntity&)>	FPrototypeScanner;


	template <typename Item, typename ItemType>
		class GenericCollectionStore
		{
		public:
			Buffer0<Item>				list;
			StringTable<Item>			stringTable;
			HashTable<ItemType,Item>	typeTable;
			HashTable<ItemType,String>	typeToStringTable;
			HashTable<String,ItemType>	stringToTypeTable;

			void	Add(const Item&item, const ItemType&t, String name)
			{
				VerifyIntegrity();

				list << item;
				ASSERT1__(typeTable.SetNew(t,item),t);
				ASSERT1__(stringTable.SetNew(name,item),name);
				ASSERT1__(typeToStringTable.SetNew(t,name),t);
				ASSERT1__(stringToTypeTable.SetNew(name,t),name);
				if (name.EraseCharacter(' '))
				{
					ASSERT1__(stringTable.SetNew(name,item),name);
					ASSERT1__(stringToTypeTable.SetNew(name,t),name);
				}

				VerifyIntegrity();
			}


			void	VerifyIntegrity()	const
			{
				ASSERT_LESS_OR_EQUAL__(list.Count(),stringTable.Count());
				ASSERT_EQUAL__(list.Count(),typeTable.Count());
				ASSERT_LESS_OR_EQUAL__(list.Count(),stringToTypeTable.Count());
				ASSERT_EQUAL__(list.Count(),typeToStringTable.Count());
			}

			/**
			Includes another collection of the same type into the local table.
			Must be completely disjoint
			*/
			template <typename Item2>
			void	Include(const GenericCollectionStore<Item2,ItemType> &other)
			{
				VerifyIntegrity();
				other.VerifyIntegrity();

				stringTable.IncludeCast(other.stringTable);
				typeTable.IncludeCast(other.typeTable);
				stringToTypeTable.IncludeCast(other.stringToTypeTable);
				typeToStringTable.IncludeCast(other.typeToStringTable);
				list.Append(other.list);

				VerifyIntegrity();
			}

			void	Clear()
			{
				list.Clear();
				stringTable.Clear();
				typeTable.Clear();
				stringToTypeTable.Clear();
				typeToStringTable.Clear();
			}

		};

	template <typename Item, typename ItemType>
		class GenericCollection
		{
			typedef GenericCollectionStore<Item,ItemType>	Store;
			typedef GenericCollection<Item,ItemType>	Self;

			Store	store;
		protected:
			void	Add(const Item&item, const ItemType&t, String name)
			{
				store.Add(item,t,name);
			}
		public:
			typedef typename Buffer0<Item>::const_iterator	const_iterator;


			const Store&	GetStore() const {return store;}

			const_iterator	begin() const {return store.list.begin();}
			const_iterator	end() const {return store.list.end();}
			count_t	size() const {return store.list.size();}
			count_t	Count() const {return store.list.size();}
			const Item&	operator[](index_t at)	{return store.list[at];}

			/**
			Includes another collection of the same type into the local table.
			Must be completely disjoint
			*/
			template <typename Item2>
			void	Include(const GenericCollection<Item2,ItemType> &other)
			{
				store.Include(other.GetStore());
			}

			void	Clear()
			{
				store.Clear();
			}

			bool	IsEmpty() const {return store.list.IsEmpty();}
			bool	IsNotEmpty() const {return store.list.IsNotEmpty();}

			/**
			Does a hashtable lookup of the specified type.
			If the type is not found, the method returns an empty item (null if the type supports that)
			*/
			Item	Find(const ItemType&t) const
			{
				return store.typeTable.Get(t,Item());
			}

			/**
			Does a hashtable lookup of the specified type.
			If the type is not found, the method returns an empty item (null if the type supports that)
			*/
			Item	FindByName(const StringRef&name) const
			{
				return store.stringTable.Get(name,Item());
			}

			/**
			Does a hashtable lookup of the specified type.
			If the type is not found, an exception is thrown
			*/
			Item	FindByName(const String&name) const
			{
				return store.stringTable.Get(name,Item());
			}
			/**
			Does a hashtable lookup of the specified type, and returns its precise descriptor.
			If the type is not found, an exception is thrown
			*/
			ItemType	ToType(const String&name) const
			{
				auto ptr = store.stringToTypeTable.QueryPointer(name);
				if (ptr)
					return *ptr;
				throw Except::Program::ParameterFault(CLOCATION,"Unable to decode '"+name+"'");
			}
			/**
			Does a hashtable lookup of the specified type, and returns its precise descriptor.
			If the type is not found, an exception is thrown
			*/
			ItemType	ToType(const StringRef&name) const
			{
				auto ptr = store.stringToTypeTable.QueryPointer(name);
				if (ptr)
					return *ptr;
				throw Except::Program::ParameterFault(CLOCATION,"Unable to decode '"+String(name)+"'");
			}

			/**
			Does a hashtable lookup of the specified type, and returns its string representation.
			If the type is not found, an exception is thrown
			*/
			const String&		ToTypeName(const ItemType&t) const
			{
				auto ptr = store.typeToStringTable.QueryPointer(t);
				if (ptr)
					return *ptr;
				throw Except::Program::ParameterFault(CLOCATION,"Unable to convert type to string: not found");
			}

		};


	



	namespace Prototypes
	{
		template <typename T>
			class Collection : public GenericCollection<std::shared_ptr<T>,EntityType>
			{
				typedef GenericCollection<std::shared_ptr<T>,EntityType> Super;
				typedef Collection<T>	Self;
				typedef std::shared_ptr<T>		Item;
			public:
				void	Add(const Item&item)
				{
					Super::Add(item,item->GetType(),item->GetTypeName());
				}

				Self&	operator<<(const Item&item)
				{
					Add(item);
					return *this;
				}


				template <typename Item2>
					void	IncludeList(const BasicBuffer<Item2>&elements)
					{
						foreach (elements,el)
							Add(*el);
					}
			};

		extern Collection<WorldEntity>	all;
		extern Collection<Structure>	structures;
		extern Collection<SubOrbital::Plane>		planes;
		#ifndef NO_SATELLITES
			extern Collection<Satellite>satellites;
		#endif


		/**
		Collects/creates all prototypes, and registers them in the respective prototype field
		*/
		void			RegisterAll();

		void			Destroy();

		PWorldEntity	Find(EntityType t);
	}

	namespace Component
	{
		template <typename T>
			class Collection : public GenericCollection<T*,ConstName>
			{
				typedef GenericCollection<T*,ConstName> Super;
				typedef Collection<T>	Self;
				typedef T*					Item;
			public:
				void	Add(const Item&item)
				{
					Super::Add(item,item->constName,item->typeName);
				}

				Self&	operator<<(const Item&item)
				{
					Add(item);
					return *this;
				}
			};

		extern Collection<VehicleBase>	baseComponents;
		extern Collection<Turret>		vehicleTurretComponents,
										allTurretComponents;
		extern Collection<Base>			allComponents;
	}
	#ifndef NO_SATELLITES
		extern Buffer0<PSatellite>		satellitePrototypes;
	#endif

	String			ToString(const EntityType&);
	EntityType		DecodeType(const String&);


	namespace Component
	{
		void			ConfigureAll();
		void			DestroyAll();
	}
}




#endif
