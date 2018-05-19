#pragma once

#include <container/array.h>
#include <container/buffer.h>
#include <global_string.h>
#include "../../lua/luaInterface.h"

namespace Objects
{
	namespace Research
	{
		namespace D = DeltaWorks;
		/**
		Checks whether either a const value or lua function is defined the specified int attribute index
		*/
		bool					LuaIntAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified bool attribute index
		*/
		bool					LuaBoolAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified float attribute index
		*/
		bool					LuaFloatAttributeIsSet(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified int attribute index
		*/
		bool					LuaIntAttributeIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified bool attribute index
		*/
		bool					LuaBoolIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified float attribute index
		*/
		bool					LuaFloatAttributeIsFunctionOrConstNonZero(index_t);

		template <typename T>
			inline bool			LuaAttributeIsSet(index_t) = delete;
		template <>
			inline bool			LuaAttributeIsSet<int>( index_t idx ) {return LuaIntAttributeIsSet(idx);}
		template <>
			inline bool			LuaAttributeIsSet<bool>( index_t idx ) {return LuaBoolAttributeIsSet(idx);}
		template <>
			inline bool			LuaAttributeIsSet<float>( index_t idx ) {return LuaFloatAttributeIsSet(idx);}

		template <typename T>
			inline bool			LuaAttributeIsFunctionOrConstNonZero(index_t) = delete;
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<int>( index_t idx ) {return LuaIntAttributeIsFunctionOrConstNonZero(idx);}
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<bool>( index_t idx ) {return LuaBoolIsFunctionOrConstNonZero(idx);}
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<float>( index_t idx ) {return LuaFloatAttributeIsFunctionOrConstNonZero(idx);}
		




		namespace Attributes
		{

			struct Flags
			{
				enum flag_t
				{
					HasFunctionFlag = 0x1,
					IsConst = 0x2,


					MaxValue = 0x3,
				};

				typedef UINT8			serial_type_t;
			};

	
			template <typename T>
				class TypeMap
				{
				public:
					typedef Flags::serial_type_t	flag_t;
					struct Attrib
					{
						T				value;
						D::String		description;
						flag_t			flags;


						bool			IsSet() const
						{
							return flags != 0;
						}

						bool			IsFunctionOrConstNonZero() const
						{
							return IsSet() && (((flags & Flags::HasFunctionFlag)!=0) || value != 0);
						}
					};
					D::Array<Attrib>	attributes;
					D::Buffer0<index_t>	changedSinceLastRound;

					void				ClearChanges()
					{
						changedSinceLastRound.Clear();
					}

					Attrib&				ForceUpdate(index_t at)
					{
						changedSinceLastRound << at;
						return attributes[at];
					}

					bool				IsSet( index_t at ) const
					{
						if (attributes.IsEmpty())
							return false;
						return Expect::SafeGet(attributes,at,CLOCATION).IsSet();
					}

					bool				IsFunctionOrConstNonZero( index_t at ) const
					{
						if (attributes.IsEmpty())
							return false;
						return Expect::SafeGet(attributes,at,CLOCATION).IsFunctionOrConstNonZero();
					}

					bool				CheckUpdate(index_t at, T newValue, const D::String&newDesc, flag_t flags)
					{
						Attrib&a = attributes[at];
						if (a.description == newDesc && a.value == newValue && a.flags == flags)
							return false;
						a.value = newValue;
						a.description = newDesc;
						a.flags = flags;
						changedSinceLastRound << at;
						return true;
					}

					T				SafeGet(index_t at)	const
					{
						if (at == InvalidIndex)
							return (T)0;
						if (at >= attributes.Count())
						{
							ASSERT__(attributes.IsEmpty());
							return (T)0;
						}
						return attributes[at].value;
					}

					const D::String&	SafeDesc(index_t at) const
					{
						if (at == InvalidIndex)
						{
							static String rs = "???";
							return rs;
						}
						if (at >= attributes.Count())
						{
							ASSERT__(attributes.IsEmpty());
							static String rs = "[null]";
							return rs;
						}
						return attributes[at].description;
					}

				};


			/**
			Table containing all attributes in one array per type: float, int, bool
			*/
			class Table
			{
			public:
				TypeMap<float>		floats;
				TypeMap<int>		ints;
				TypeMap<bool>		bools;


				void				ClearChanges()
				{
					floats.ClearChanges();
					ints.ClearChanges();
					bools.ClearChanges();
				}

				bool				AnyChanged() const
				{
					return
						floats.changedSinceLastRound.Count() > 0
						||
						ints.changedSinceLastRound.Count() > 0
						||
						bools.changedSinceLastRound.Count() > 0
						;
				}
				#ifndef CLIENT
					void			Update(const Lua::Reference&luaResearchState);
				#endif

				template <typename T>
					T				Get(index_t) const = delete;
				template <>
					int				Get<int>(index_t at) const {return ints.SafeGet(at);}
				template <>
					bool			Get<bool>(index_t at) const {return bools.SafeGet(at);}
				template <>
					float			Get<float>(index_t at) const {return floats.SafeGet(at);}
				template <typename T>
					const D::String&Describe(index_t) const = delete;
				template <>
					const D::String&Describe<int>(index_t at) const {return ints.SafeDesc(at);}
				template <>
					const D::String&Describe<bool>(index_t at) const {return bools.SafeDesc(at);}
				template <>
					const D::String&Describe<float>(index_t at) const {return floats.SafeDesc(at);}

			private:

			};
		}

		template <typename T>
			struct Attribute
			{
				index_t		linearIndex = InvalidIndex;

				/**/		Attribute()	{}
				/**/		Attribute(nullptr_t)	{}
				/**/		Attribute(const T&constOverride)
				{
					SetConst(constOverride);
				}

				bool		IsFunctionOrConstNonZero() const
				{
					return Research::LuaAttributeIsFunctionOrConstNonZero<T>(linearIndex);
				}
				bool		IsSet() const
				{
					return Research::LuaAttributeIsSet<T>(linearIndex);
				}
				bool		IsEmpty() const {return !IsSet();}


				void		Clear(index_t linearIndex)
				{
					this->linearIndex = linearIndex;
				}

				T			GetValue(const Attributes::Table&researchState)	const
				{
					return researchState.Get<T>(linearIndex);
				}

				D::String	Describe(const Attributes::Table&researchState)	const
				{
					return researchState.Describe<T>(linearIndex);
				}

			private:

			};	


	}
}
