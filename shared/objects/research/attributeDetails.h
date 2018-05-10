#include <global_string.h>


namespace Objects
{
	namespace Research
	{
		using namespace DeltaWorks;


		extern String						attributeContext;

		CONSTRUCT_ENUMERATION3(OnAttributeNotFound, Fail, Warn, Ignore);


		template <typename T>
			class LuaAttributes
			{
			private:
				struct ConstOverride
				{
					T			constValue;
					bool		isSet = false;
				};

				struct Attribute
				{
					Lua::Function<T(Lua::Reference)>		luaGetValue;
					Lua::Function<String(Lua::Reference)>	luaDescribeValue;
					ConstOverride							constOverride;

					void		ExportTo( const Lua::Reference&state, typename Research::Attributes::TypeMap<T>::Attrib&outAttrib ) const
					{
						if (constOverride.isSet)
							outAttrib.value = constOverride.constValue;
						else
						{
							if (luaGetValue.IsEmpty())
								outAttrib.value = (T)0;
							else
								outAttrib.value = luaGetValue(state);
						}

						if (luaDescribeValue.IsSet())
							outAttrib.description = luaDescribeValue(state);
						else
						{
							using StringConversion::ToString;
							outAttrib.description = ToString(outAttrib.value);
						}

						outAttrib.flags = GetFlags();
					}

					Research::Attributes::Flags::serial_type_t GetFlags() const
					{
						Research::Attributes::Flags::serial_type_t rs = 0;
						if (luaGetValue.IsSet())
							rs |= Research::Attributes::Flags::HasFunctionFlag;
						if (constOverride.isSet)
							rs |= Research::Attributes::Flags::IsConst;
						return rs;
					}



					void		CheckExportTo( const Lua::Reference&state, Research::Attributes::TypeMap<T>&outAttribs, index_t at ) const
					{
						T value;
						if (constOverride.isSet)
							value = constOverride.constValue;
						else
						{
							if (luaGetValue.IsEmpty())
								value = (T)0;
							else
								value = luaGetValue(state);
						}

						String desc;
						if (luaDescribeValue.IsSet())
							desc = luaDescribeValue(state);
						else
						{
							using StringConversion::ToString;
							desc = ToString(value);
						}

						outAttribs.CheckUpdate(at,value,desc,GetFlags());
					}

					void	SetConst( T value )
					{
						constOverride.constValue = value;
						constOverride.isSet = true;
					}

					bool	IsSet() const
					{
						return luaGetValue.IsSet() || constOverride.isSet;
					}

					bool	IsFunctionOrConstNonZero() const
					{
						return constOverride.isSet ? constOverride.constValue != 0 : luaGetValue.IsSet();
					}

					void	swap(Attribute&other)
					{
						using std::swap;
						swap(constOverride,other.constOverride);
						luaGetValue.swap(other.luaGetValue);
						luaDescribeValue.swap(other.luaDescribeValue);
					}

					friend void swap(Attribute&a, Attribute&b)
					{
						a.swap(b);
					}


				};

			public:

				static void Set(const Lua::Table&source, const String&path, const String&name, Research::Attribute<T>&a, OnAttributeNotFound onAttributeNotFound = OnAttributeNotFound::Fail)
				{

					a.Clear(list.Count());
					auto&attrib = list.Append();

					#ifndef CLIENT


						Lua::NamedReference ref;
						if (source.IsSet())
							ref = source.GetField(name);
						if (ref.IsEmpty())
						{
							switch (onAttributeNotFound)
							{
								case OnAttributeNotFound::Fail:
									throw Lua::Exception("Field '"+path+"."+name+"' is not defined");
								case OnAttributeNotFound::Warn:
									LogEvent(EventClass::Preset::configurationError,attributeContext+": Field '"+path+"."+name+"' is not defined");
									break;
							}
							return;
						}

						if (ref.IsTable())
						{
							Lua::Table t = ref;
			
							attrib.luaGetValue = t.GetField("Get");
							if (attrib.luaGetValue.IsEmpty())
								throw Lua::Exception("Function '"+path+"."+name+".Get' is not defined");

							attrib.luaDescribeValue = t.GetField("Describe");
						}
						elif (ref.IsFunction())
						{
							attrib.luaGetValue = ref;
						}
						else
						{
							attrib.SetConst(ref.GetAs<T>());
						}
					#endif
				}

				static void Clear()
				{
					list.Clear();
				}

				static bool IsSet( index_t idx )
				{
					if (list.IsEmpty())
						return false;
					return Expect::SafeGet(list,idx,CLOCATION).IsSet();
				}

				static bool IsFunctionOrConstNonZero( index_t idx )
				{
					if (list.IsEmpty())
						return false;
					return Expect::SafeGet(list,idx,CLOCATION).IsFunctionOrConstNonZero();
				}

				static void ExportFunctionMapTo( Array<bool>&outFunctionIsSet )
				{
					outFunctionIsSet.SetSize(list.Count());
					for (index_t i = 0; i < list.Count(); i++)
					{
						outFunctionIsSet[i] = list[i].luaGetValue.IsSet();
					}
				}

				static void ExportTo( const Lua::Reference&state, Research::Attributes::TypeMap<T>&outType )
				{
					if (outType.attributes.Count() != list.Count())
					{
						outType.attributes.SetSize(list.Count());
						for (index_t i = 0; i < list.Count(); i++)
						{
							auto&a = outType.ForceUpdate(i);
							list[i].ExportTo(state,a);
						}
					}
					else
					{
						for (index_t i = 0; i < list.Count(); i++)
							list[i].CheckExportTo(state,outType,i);
					}
				}

			private:




			
				static Buffer0<Attribute,Swap>	list;

			};

		template <typename T>
			/*static*/ Buffer0<typename LuaAttributes<T>::Attribute,Swap>	LuaAttributes<T>::list;

		template <typename T>
			static void InitAttrib(const Lua::Table&source, const String&path, const String&name, Attribute<T>&a, OnAttributeNotFound onAttributeNotFound = OnAttributeNotFound::Fail)
			{
				LuaAttributes<T>::Set(source,path,name,a,onAttributeNotFound);
			}



	}
}
