#ifndef componentLuaInterfaceH
#define componentLuaInterfaceH

#include <global_string.h>
#include <interface/write_stream.h>
#include <container/buffer.h>
#include <functional>

struct lua_State;

int LuaRun (int argc, const char *argv[]);


/**
Lua-related definitions and helpers
*/
namespace Lua
{
	class Reference;
	class Program;
	class Table;
	typedef std::shared_ptr<Program> PProgram;
	typedef std::weak_ptr<Program> WProgram;


	///**
	//Capsuled lua-state. Must use special delete-function upon destruction
	//*/
	//typedef std::shared_ptr<lua_State>	PState;

	/**
	Pushes the value of the global refID to the stack.
	If the respective global does not exist or contains nil, then nil is pushed
	*/
	void	PushRegisteredGlobal(lua_State*, int refID);
	/**
	Executes a function as currently pushed to the stack.
	The calling method must first push the function itself, followed by each parameter.
	The number of pushed parameters must match @a numParameters.
	Both the function and parameters are removed from the stack as a result of execution.
	Upon success, all requested return values are pushed to the stack
	Throws Lua::Exception on failure. No return values are pushed in case an exception is thrown
	@param numParameters Number of parameters pushed to the stack. Must match the actually pushed parameters
	@param numResults Number of return values which are pushed to the stack as a result. Typically 0 (void function) or 1 (single return value)
	@param[out] errorOut Resulting error message in case lua execution fails. Updated only if the function returns false
	*/
	void	CallPushedFunction(lua_State*,int numParameters, int numResults);
	/**
	Finds a global (variable, function, w/e) by name, and pushes it to a new refID.
	@param name Name of the global to look for
	@param[out] outRefID Resultung refID. Updated only if the function returns true
	@return true if the name was found and a new refID was created (and stored in @a outRefID), false otherwise
	*/
	bool	LookupGlobal(lua_State *,const String&name, int&outRefID);
	/**
	@deprecated
	Creates a true copy of the specified refID
	@return new refID that holds a copy of the specified refID's contents
	*/
	int		DuplicateReference(lua_State *, int refID);
	/**
	Erases a given refID. Any associated data is flagged for garbage collection.
	Subsequent calls to LookupGlobal() or DuplicateReference() may again return this refID as new.
	@param refID refID to erase
	*/
	void	EraseReference(lua_State *, int refID);

	class Exception : public Except::Program::GeneralFault
	{
		typedef Except::Program::GeneralFault Super;
	public:
		/**/		Exception(){}\
		/**/		Exception(const char*message):Super(message) {}\
		/**/		Exception(const Except::globalString&string):Super(string){}\
		/**/		Exception(const TCodeLocation&loc):Super(loc){}\
		/**/		Exception(const TCodeLocation&loc, const char*message):Super(loc,message) {}\
		/**/		Exception(const TCodeLocation&loc, const Except::globalString&string):Super(loc,string){}\
		template <class C>		Exception(const IString<C>&string):Super(string){}\
		template <class C>		Exception(const TCodeLocation&loc, const IString<C>&string):Super(loc,string){}\
		static const Except::Name&		StaticGetName()	{static const Except::Name name=Except::Name(Super::StaticGetName(),"/Lua::Exception"); return name;}
		virtual	const char*		GetType()	const override					{return StaticGetName();}
	};


	namespace Detail
	{
		template <typename T>
			inline int DeleteT(lua_State*st)
			{
				void*d = GetUserData(st,1);
				ASSERT_NOT_NULL__(d);
				T*obj = (T*)d;
				((T*)d)->~T();	//destroy
				return 0;
			}



		void* PushShared(lua_State*state, size_t fsize, int(*fdel)(lua_State*));


		void PushParameter(lua_State *state, long long i);
		inline void PushParameter(lua_State *state, int i) {PushParameter(state,(long long)i);}
		void PushParameter(lua_State *state, double i);
		void PushParameter(lua_State *state, void*p);
		void PushParameter(lua_State *state, const Reference&);
		void PushStrParameter(lua_State *state, const String&str);




		template <typename T>
			struct ParameterT
			{
				static void	Push(lua_State *state, const T&v)
				{
					PushParameter(state,v);
				}
			};
		template <>
			struct ParameterT<String>
			{
				static void Push(lua_State *state, const String&v)
				{
					PushStrParameter(state,v);
				}
			};
		template <typename T>
			struct ParameterT<std::shared_ptr<T>>
			{
				static void Push(lua_State*state, const std::shared_ptr<T>&p)
				{
					typedef std::shared_ptr<T>	ST;
					void* dat = PushShared(state,sizeof(ST),DeleteT<ST>);
					new (dat)ST(p);
				}
			};

		void PopStacked(lua_State*);

		void*RequireStackedUserData(const PProgram&,int addr,size_t requireByteSize, int (*gcFunction)(lua_State*), bool allowNil, bool pCall);
		void GetStacked(const PProgram&, int addr, bool&rs);
		void GetStacked(const PProgram&, int addr, String&rs);
		void GetStacked(const PProgram&, int addr, void*&rs);
		void GetStacked(const PProgram&, int addr, long long&rs);
		void GetStacked(const PProgram&, int addr, double&rs);
		void GetStacked(const PProgram&, int addr, Reference&rs);
		
		inline void GetStacked(const PProgram&st, int addr, float&rs)
		{
			double tmp;
			GetStacked(st,addr,tmp);
			rs = (float)tmp;
		}

		inline void GetStacked(const PProgram&st, int addr, int&rs)
		{
			long long temp;
			GetStacked(st,addr,temp);
			rs = (int)temp;
		}



		template <typename T, bool PCall>
			struct BaseStackedT
			{
				static void Get(const PProgram&st, int addr, T&rs)
				{
					GetStacked(st,addr,rs);
				}
				static T Get(const PProgram&st, int addr)
				{
					T tmp;
					GetStacked(st,addr,tmp);
					return tmp;
				}
			};

		template <typename T, bool PCall>
			struct BaseStackedT<std::shared_ptr<T>, PCall>
			{
				static void Get(const PProgram&st, int addr, std::shared_ptr<T>&rs)
				{
					void*data = RequireStackedUserData(st,addr,sizeof(rs), DeleteT<std::shared_ptr<T> >,true,PCall);
					if (!data)
					{
						rs = std::shared_ptr<T>();
						return;
					}
					rs = *(const std::shared_ptr<T>*)(data);
				}
				static const std::shared_ptr<T>& Get(const PProgram&st, int addr)
				{
					void*data = RequireStackedUserData(st,addr,sizeof(std::shared_ptr<T>), DeleteT<std::shared_ptr<T> >,true,PCall);
					static std::shared_ptr<T> empty;
					if (!data)
						return empty;
					return *(const std::shared_ptr<T>*)(data);
				}

			};

		template <typename T>
			struct Strip : public std::remove_const<typename std::remove_reference<T>::type>
			{};

		template <typename T, bool PCall>
			struct StackedT : public BaseStackedT<typename Strip<T>::type,PCall>
			{};

		static inline int PushParameters(lua_State *state)
		{
			return 0;
		}

		template <typename T0>
			static inline int PushParameters(lua_State *state, const T0&p0)
			{
				ParameterT<T0>::Push(state,p0);
				return 1;
			}

		template <typename T0, typename ...T>
			static inline int PushParameters(lua_State *state, const T0&p0, const T&...p)
			{
				ParameterT<T0>::Push(state,p0);
				return 1 + PushParameters(state,p...);
			}



		static inline void GetParameters(lua_State *state, int at)
		{}

		template <typename T0>
			static inline void GetParameters(lua_State *state, int at, const T0&p0)
			{
				if (at < 0)
					StackedT<T0,true>::Get(state,at,p0);
			}

		template <typename T0, typename ...T>
			static inline void GetParameters(lua_State *state, int at, const T0&p0, const T&...p)
			{
				GetParameters(state,at,p0);
				GetParameters(state,at+1,p);
			}


		void*	GetUserData(lua_State*,int idx);



		void*	BeginFunction(lua_State *state, PProgram&outProgram);
		void*	RegisterFunction(const PProgram & prog, size_t fsize, int(*fdel)(lua_State*) ,int(*f)(lua_State*));
		void	MoveStackTopToGlobal(const PProgram&prog,const String&name);

		void	Raise(lua_State *state,const String&error);
		
		

		//index building from: http://stackoverflow.com/questions/29057781/how-to-implement-an-easy-bind-that-automagically-inserts-implied-placeholders
		template <std::size_t... Is>
			struct indices {};

		template <std::size_t N, std::size_t... Is>
			struct build_indices  : build_indices<N-1, N-1, Is...> {};

		template <std::size_t... Is>
			struct build_indices<0, Is...> : indices<Is...> {};


		template <typename T>
			class BaseExec {};

		template <typename R, typename... P>
			class BaseExec<R(P...)>
			{
			public:
				template <index_t...I>
					static int Execute(lua_State *state, indices<I...>)
					{
						try
						{
							Lua::PProgram prog;
							const auto&f = *(const std::function<R(P...)>*)BeginFunction(state,prog);
							if (!prog)
								return 0;
							try
							{
								R rs = f(StackedT<typename Strip<P>::type,true>::Get(prog,I+1)...);
								ParameterT<R>::Push(state,rs);
								return 1;
							}
							catch (const std::exception&e)
							{
								Raise(state,e.what());
							}
							return 0;
						}
						catch (const std::exception&ex)
						{
							Raise(state,ex.what());
							return 0;
						}
					}
			};

		template <typename ...P>
			class BaseExec<void(P...)>
			{
			public:
				template <index_t...I>
					static int Execute(lua_State *state, indices<I...>)
					{
						try
						{
							Lua::PProgram prog;
							const auto&f = *(const std::function<void(P...)>*)BeginFunction(state,prog);
							if (!prog)
								return 0;
							try
							{
								f(StackedT<typename Strip<P>::type,true>::Get(prog,I+1)...);
							}
							catch (const std::exception&e)
							{
								Raise(state,e.what());
							}
							return 0;
						}
						catch (const std::exception&ex)
						{
							Raise(state,ex.what());
							return 0;
						}

					}
			};
		

		template <typename T>
			class LuaExec {};

		template <typename R, typename... P>
			class LuaExec<R(P...)> : public BaseExec<R(P...)>
			{
				typedef BaseExec<R(P...)>	Super;
			public:
				static int Execute(lua_State *state)
				{
					return Super::Execute(state,build_indices<sizeof...(P)>{});
				}			
			
			
			};

		template <typename ...P>
			class LuaExec<void(P...)> : public BaseExec<void(P...) >
			{
				typedef BaseExec<void(P...)>	Super;
			public:
				static int Execute(lua_State *state)
				{
					return Super::Execute(state,build_indices<sizeof...(P)>{});
				}			
						
			};

		
		template <typename F>
			inline void PushLibFunctionT(const Lua::PProgram&prog, const std::function<F>&f)
			{
				
				//std::function<F> * fptr = new std::function<F>(f);
				//Lua::WProgram*pptr = new Lua::WProgram(prog);
				typedef std::function<F>	FT;
				void* dat = RegisterFunction(prog,sizeof(FT),DeleteT<FT>,LuaExec<F>::Execute);
				new (dat)FT(f);
			}

		template <typename F>
			struct LibFunction
			{};
		
		template <typename R, typename...P>
			struct LibFunction<R(P...)>
			{

				static void	Push(const PProgram&prog, R(*f)(P...))
				{
					PushLibFunctionT(prog,std::function<R(P...)>(f));
				}
			};

		template <typename R, typename...P>
			struct LibFunction<std::function<R(P...)> >
			{
				static void	Push(const PProgram&prog, const std::function<R(P...)>&f)
				{
					PushLibFunctionT(prog,f);
				}

			};

	}



	/**
	Lua program state.
	All scripts executed on the same program share the same variable/function/w/e constellation.
	New instances of Program start empty and must be created via Program::Create()
	*/
	class Program : public std::enable_shared_from_this<Program>
	{
		lua_State		*const state;

		/**/			Program(lua_State*st):state(st)	{}

	public:
		/**/			Program() = delete;
		/**/			Program(const Program&) = delete;
		/**/			~Program();
		void			operator=(const Program&) = delete;

		/**
		Creates a new instance.
		@return new object on success. An empty results indicates a Lua error (most likely insufficient memory)
		*/
		static PProgram	Create();
		/**
		Fetches the internal lua state. Never NULL
		*/
		lua_State*		GetState() const {return state;}

		/**
		Finds a global of the local lua state and stores the result in @a outRef
		@param name Name to look for
		@param[out] outRef Resulting lua reference. Not altered if the method returns false. Otherwise any data previously referenced by outReference may be destroyed (if this is the last reference)
		@return true if the specified global was found and @a outRef updated, otherwise
		*/
		bool			FindGlobal(const String&name, Reference&outRef);
		Reference		FindGlobal(const String&name);

		/**
		Attempts to execute an arbitrary lua script chunk in protected mode.
		Throws Lua::Exception() in case of errors
		@param luaCode Code to execute
		*/
		void			Execute(const String&luaCode, const String&context="");
		/**
		Attempts to exdecute a script file from disk.
		Throws file open faults or Lua::Exception in case of errors
		*/
		void			ExecuteFile(const PathString&filename);
		/**
		Deserializes binary host data to a new lua reference
		@param[inout] data Serialized host data of some lua variable (created by SerializeReference() or SerializeGlobal())
		@param[out] outReference Reference to store the result in. Not updated if the method returns false. Otherwise any data previously referenced by outReference may be destroyed (if this is the last reference)
		@return true if the data could be deserialized and the reference was created, false otherwise
		*/
		bool			DeserializeToReference(IReadStream& data, Reference&outReference);
		Reference		DeserializeToReference(IReadStream& data);

		/**
		Creates a new table and returns the reference to it. Apart from the returned reference, there is no link of the local program to the returned table
		*/
		Table			CreateTable();
		/**
		Associates the specified name with the given reference
		@return true if the given reference is not empty, false otherwise
		*/
		void			SetGlobal(const String&name, const Reference&);

		/**
		Queries a global variable.
		The resulting reference may be empty if the requested global does not exist
		*/
		Reference		GetGlobal(const String&name);
	};

	/**
	Generic lua variable reference.
	Can point to anything lua can store in its stack slots.

	References keep a weak shared pointer to the program they were created from
	*/
	class Reference
	{
	private:
		WProgram					prog;
		std::shared_ptr<int>		refID;

		static void Discard(const WProgram&st, int*raw)
		{
			PProgram prog = st.lock();
			if (prog)
				EraseReference(prog->GetState(),(int)(size_t)raw);
		}
		void	SetReference(WProgram prog, int refID)
		{
			this->prog = prog;
			this->refID.reset((int*)(size_t)refID,[prog](int*ptr){Discard(prog,ptr);});

			if (IsTableReference() && !IsTable())
				throw Exception("Trying to assigning non-table Lua reference to table reference variable");
			if (IsFunctionReference() && !IsFunction())
				throw Exception("Trying to assigning non-function Lua reference to function reference variable");
		}
	protected:

		virtual bool	IsTableReference() const	{return false;}
		virtual bool	IsFunctionReference() const	{return false;}
		
	public:

		/**/		Reference(){}
		virtual		~Reference() {};

		/**
		Updates the local reference to the top-most entry of the current lua stack associated with the specified program.
		The stack remains unchanged by the operation (the referenced element remains in the stack)
		The method fails if the given program is empty, its stack does not contain the addressed element, or the addressed element is nil.
		@return true if the local reference was updated
		@param prog Program to reference
		@param idx Stack address in the ranges [1,top] and [-top,-1]. 0 is invalid
		*/
		bool		ResetFromStack(const PProgram&prog, int idx);
		/**
		Updates the local reference to the given program and name
		The method fails if the specified program is not set, or the name does not exist
		@return true if the local reference was updated
		@param prog Program to reference
		@param name Name to lookup
		*/
		bool		ResetToGlobal(const PProgram&prog, const String&name)
		{
			int global;
			if (!prog || !LookupGlobal(prog->GetState(),name,global))
			{
				return false;
			}
			SetReference(prog, global);
			return true;
		}

		bool		IsSet() const {return !prog.expired() && refID;}
		bool		IsEmpty() const {return !IsSet();}
		bool		IsTable() const;
		bool		IsFunction() const;

		void		swap(Reference&other)
		{
			refID.swap(other.refID);
			prog.swap(other.prog);
		}

		int			GetRefID() const {return reinterpret_cast<int>( refID.get() );}
		PProgram	GetProgram() const {return prog.lock();}
		lua_State*	GetState() const {PProgram prog = this->prog.lock(); return prog ? prog->GetState() : nullptr;}


		/**
		Pushes the local reference to the top of the stack
		*/
		bool		Push()	const {if (IsEmpty()) return false; PushRegisteredGlobal(GetState(),GetRefID()); return true;}

		/**
		Fetches the fereenced value as a host copy.
		The method fails if the local reference is empty.
		If the referenced data is not of the respective primitive type the operation may return empty values
		*/
		template <typename T>
			bool	Get(T&result) const
			{
				if (!Push())
					return false;
				try
				{
					Detail::StackedT<T,false>::Get(GetProgram(),-1,result);
				}
				catch (...)
				{
					Detail::PopStacked(GetState());
					throw;
				}
				Detail::PopStacked(GetState());
				return true;
			}
		template <typename T>
			T		GetAs() const {T rs; Get(rs); return rs;}

		/**
		Attempts to store the local reference as a host byte stream.
		If the local reference is empty a Lua::Exception is thrown.
		Some data (like functions) cannot be serialized and cause a Lua::Exception to be thrown.
		@param[inout] stream Stream to put the resulting byte data to
		@param ignoreNonSerializableContent Set true to allow and ignore non-serializable content. Otherwise an exception is thrown upon encountering such
		@return true if the stream was large enough to hold any serializable data, false otherwise
		*/
		bool			Serialize(IWriteStream&stream, bool ignoreNonSerializableContent=false)	const;

		bool			DeserializeMerge(IReadStream&stream);
		void			SetGlobal(const String&name);

	};

	/**
	Handler structure for tables
	*/
	class Table : public Reference
	{
		typedef Reference	Super;
		void			SetTopField(const String&name);
		void			SetTopElement(index_t at);
		bool			AppendTopElement();
		bool			PushField(const String&name) const;
		bool			PushElement(index_t) const;
		void			Pop(int cnt);
	protected:
		virtual bool	IsTableReference() const override	{return true;}

	public:
		/**/			Table()	{}
		/**/			Table(const Reference&ref)
		{
			operator=(ref);
		}
		/**
		Fetches a reference for the specified field in the local table.
		The result may be empty if the specified field was not found
		*/
		Reference		GetField(const String&name)	const;
		/**
		Fetches a reference for the specified index (usually in the range [1,GetLength()]).
		The result may be empty if the specified index was not found
		*/
		Reference		GetElement(index_t at) const;

		/**
		Fetches the value of the specified field in the local table.
		Intended to be used for primitive values like ints or strings, but may be used for Reference objects, too
		@return true on success
		*/
		template <typename T>
			bool		GetField(const String&name, T&rs) const
			{
				if (!PushField(name))
					return false;
				rs = Detail::StackedT<T,false>::Get(GetProgram(),-1);
				Pop(2);
				return true;
			}
		/**
		Fetches the value of the specified index element in the local table.
		Intended to be used for primitive values like ints or strings, but may be used for Reference objects, too
		@return true on success
		*/
		template <typename T>
			bool		GetElement(index_t at, T&rs) const
			{
				if (!PushElement(at))
					return false;
				rs = Detail::StackedT<T,false>::Get(GetProgram(),-1);
				Pop(2);
				return true;
			}

		/**
		Sets a field value in the local table.
		Works for primitive values as well as references
		*/
		template <typename T>
			void		SetField(const String&name, const T&value)	{if (IsEmpty()) return; Detail::ParameterT<T>::Push(GetState(),value); SetTopField(name);}
		/**
		Sets an index element in the local table.
		Works for primitive values as well as references
		*/
		template <typename T>
			void		SetElement(index_t at, const T&value) {if (IsEmpty()) return; Detail::ParameterT<T>::Push(GetState(),value); SetTopElement(at);}
		/**
		Attempts to append a value to the end of the local table.
		Works only if the element GetLength()+1 is not set already
		*/
		template <typename T>
			bool		AppendElement(const T&value)
			{
				if (IsEmpty())
					return false;
				Detail::ParameterT<T>::Push(GetState(),value);
				return AppendTopElement();
			}

		/**
		Updates the local reference.
		The specified reference must be a table (and not empty).
		In debug mode this method will raise a fatal exception otherwise
		@return true if the local reference was updated
		*/
		bool			Reset(const Reference&ref);
		void			operator=(const Reference&ref);

		/**
		Inserts a new named C function into the local table.
		Works for native function pointers, and any version of std::function, but not raw functors (including lambda expressions) atm
		*/
		template <typename F>
			void		SetFunction(const String&name, const F&function)
			{
				Detail::LibFunction<F>::Push(GetProgram(),function);
				SetTopField(name);
			}

		/**
		Queries the length of the local table.
		"the length of a table t is only defined if the table is a sequence, that is, the set of its positive numeric keys is equal to {1..n} for some integer n"
		@return n
		*/
		count_t			GetLength() const;
	};


	class BaseFunction : public Reference
	{
		typedef Reference	Super;
	protected:
		/**/			BaseFunction()	{};
		virtual bool	IsFunctionReference() const override	{return true;}

	public:
		bool			Reset(const Reference&ref);
		void			operator=(const Reference&ref);
	};


	template<typename S>
		class Function;

	template <typename ...args_t>
		class Function<void(args_t...)> : public BaseFunction
		{
			typedef BaseFunction	Super;

		public:
			/**/		Function(){}
			/**/		Function(const PProgram&prog, const String&functionName)
			{
				ResetToGlobal(prog,functionName);
			}
			void		operator=(const Reference&ref)	{Super::operator=(ref);}

			void		operator()(args_t...parameters)	const
			{
				if (IsEmpty())
					throw Except::Program::GeneralFault("Lua: Trying to call empty function");
				lua_State*state = GetProgram()->GetState();
				Super::Push();
				int numParameters = Detail::PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,0);
			}

		};


	template <typename return_t, typename ...args_t>
		class Function<return_t(args_t...)> : public BaseFunction
		{
			typedef BaseFunction	Super;

		public:
			/**/		Function(){}
			/**/		Function(const PProgram&prog, const String&functionName)
			{
				ResetToGlobal(prog,functionName);
			}
			void		operator=(const Reference&ref)	{Super::operator=(ref);}

			return_t	operator()(args_t...parameters)	const
			{
				if (IsEmpty())
					throw Except::Program::GeneralFault("Lua: Trying to call empty function");
				lua_State*state = GetProgram()->GetState();
				Super::Push();
				int numParameters = Detail::PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,1);
				return_t rs;
				try
				{
					Detail::StackedT<return_t,false>::Get(GetProgram(),-1,rs);
				}
				catch (...)
				{
					Detail::PopStacked(state);
					throw;
				}
				Detail::PopStacked(state);
				return rs;
			}

		};

}

#endif