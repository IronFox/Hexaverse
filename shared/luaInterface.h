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
	@param numParameters Number of parameters pushed to the stack. Must match the actually pushed parameters
	@param numResults Number of return values which are pushed to the stack as a result. Typically 0 (void function) or 1 (single return value)
	@param[out] errorOut Resulting error message in case lua execution fails. Updated only if the function returns false
	@return true on success, false otherwise. No results are pushed if the function returns false
	*/
	bool	CallPushedFunction(lua_State*,int numParameters, int numResults, String&errorOut);
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


	namespace Detail
	{
		void PushParameter(lua_State *state, long long i);
		inline void PushParameter(lua_State *state, int i) {PushParameter(state,(long long)i);}
		void PushParameter(lua_State *state, double i);
		void PushParameter(lua_State *state, void*p);
		void PushParameter(lua_State *state, const Reference&);
		void PushStrParameter(lua_State *state, const String&str);

		template <typename T>
			inline void	PushParameterT(lua_State *state, const T&v)
			{
				PushParameter(state,v);
				//FATAL__("Push not defined for type "+String(typeid(T).name()));
			}
		template <>
			inline void	PushParameterT<String>(lua_State *state, const String&v)
			{
				PushStrParameter(state,v);
			}

		void PopStacked(lua_State*);

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

		template <typename T>
			inline  void GetStackedT(const PProgram&st, int addr, T&rs)
			{
				GetStacked(st,addr,rs);
			}



		static inline int PushParameters(lua_State *state)
		{
			return 0;
		}

		template <typename T0>
			static inline int PushParameters(lua_State *state, const T0&p0)
			{
				PushParameterT(state,p0);
				return 1;
			}

		template <typename T0, typename ...T>
			static inline int PushParameters(lua_State *state, const T0&p0, const T&...p)
			{
				PushParameterT(state,p0);
				return 1 + PushParameters(state,p...);
			}



		static inline void GetParameters(lua_State *state, int at)
		{}

		template <typename T0>
			static inline void GetParameters(lua_State *state, int at, const T0&p0)
			{
				if (at < 0)
					GetStacked(state,at,p0);
			}

		template <typename T0, typename ...T>
			static inline void GetParameters(lua_State *state, int at, const T0&p0, const T&...p)
			{
				GetParameters(state,at,p0);
				GetParameters(state,at+1,p);
			}

	}



	/**
	Lua program state.
	All scripts executed on the same program share the same variable/function/w/e constellation.
	New instances of Program start empty and must be created via Program::Create()
	*/
	class Program : public std::enable_shared_from_this<Program>
	{
		String			lastError;
		String			name;
		lua_State		*const state;

		Buffer0<std::function<void()>,Swap> destructors;

		void			Report (int);

		/**/			Program(lua_State*st):state(st)	{}

	public:
		/**/			Program() = delete;
		/**/			Program(const Program&) = delete;
		/**/			~Program();
		void			operator=(const Program&) = delete;

		/**
		Registers a new generic function to be executed before the local object is destroyed.
		The local lua state is already destroyed when these functions are executed
		*/
		void			RegisterDestructor(const std::function<void()>&f)	{destructors << f;}
		/**
		Creates a new instance.
		@return new object on success. An empty results indicates a Lua error (most likely insufficient memory)
		*/
		static PProgram	Create(const String&name = "");
		lua_State*		GetState() const {return state;}

		/**
		Finds a global of the local lua state and stores the result in @a outRef
		@param name Name to look for
		@param[out] outRef Resulting lua reference. Not altered if the method returns false. Otherwise any data previously referenced by outReference may be destroyed (if this is the last reference)
		@return true if the specified global was found and @a outRef updated, otherwise
		*/
		bool			FindGlobal(const String&name, Reference&outRef);

		/**
		Attempts to execute an arbitrary lua script chunk in protected mode
		@param luaCode Code to execute
		@return true on success, false if something happend. Check GetLastError() if the method returns false
		*/
		bool			Execute(const String&luaCode);

		/**
		Fetches a string representation of the last occurred error.
		Updated only if Execute() returns false (left as is otherwise)
		*/
		const String&	GetLastError()	const	{return lastError;}

		/**
		Attempts to store a generic reference as a host byte stream.
		Some data (like functions) cannot be serialized and is omitted.
		@param ref Reference to serialize
		@param[inout] stream Stream to put the resulting byte data to
		@return true if the specified reference is set and the stream was large enough to hold any serializable data, false otherwise
		*/
		bool			SerializeReference(const Reference&ref, IWriteStream&stream);
		/**
		Attempst to store a global variable as a host byte stream.
		Some data (like functions) cannot be serialized and is omitted.
		@param name Name of the global to serialize
		@param[inout] stream Stream to put the resulting byte data to
		@return true if the specified name was found and the stream was large enough to hold any serializable data, false otherwise
		*/
		bool			SerializeGlobal(const String&name,IWriteStream&stream);
		/**
		Deserializes binary host data to a global variable
		@param[inout] data Serialized host data of some lua variable (created by SerializeReference() or SerializeGlobal())
		@param name Name of the global variable to set
		@return true if the data could be deserialized and the global variable was updated, false otherwise
		*/
		bool			DeserializeToGlobal(IReadStream&data, const String&name);
		/**
		Deserializes binary host data to a new lua reference
		@param[inout] data Serialized host data of some lua variable (created by SerializeReference() or SerializeGlobal())
		@param[out] outReference Reference to store the result in. Not updated if the method returns false. Otherwise any data previously referenced by outReference may be destroyed (if this is the last reference)
		@return true if the data could be deserialized and the reference was created, false otherwise
		*/
		bool			DeserializeToReference(IReadStream& data, Reference&outReference);
	};

	class Reference
	{
	private:
		PProgram					prog;
		std::shared_ptr<int>		refID;

		static void Discard(const PProgram&st, int*raw)
		{
			EraseReference(st->GetState(),(int)(size_t)raw);
		}
		void	SetReference(PProgram prog, int refID)
		{
			this->prog = prog;
			this->refID.reset((int*)(size_t)refID,[prog](int*ptr){Discard(prog,ptr);});
		}
	protected:
		/**/	Reference(const PProgram&prog, const String&name)
		{
			Set(prog,name);
		}
		
	public:
		/**/		Reference(){}

		bool		SetFromStack(const PProgram&, int at);
		bool		Set(const PProgram&prog, const String&name)
		{
			int global;
			if (!LookupGlobal(prog->GetState(),name,global))
			{
				this->prog.reset();
				refID.reset();
				return false;
			}
			SetReference(prog, global);
			return true;
		}

		bool		IsSet() const {return !!refID;}
		bool		IsEmpty() const {return !refID;}

		void		swap(Reference&other)
		{
			refID.swap(other.refID);
			prog.swap(other.prog);
		}

		int			GetRefID() const {return reinterpret_cast<int>( refID.get() );}
		const PProgram&GetProgram() const {return prog;}
	};


	template<typename S>
		class Function;

	template <typename ...args_t>
		class Function<void(args_t...)> : public Reference
		{
			typedef Reference	Super;

		public:
			/**/		Function(){}
			/**/		Function(const PProgram&prog, const String&functionName):Super(prog,functionName){}

			void		Set(const Reference&ref)	{((Super&)*this) = ref;}

			void		operator()(args_t...parameters)	const
			{
				if (IsEmpty())
					throw Except::Program::GeneralFault("Lua: Trying to call empty function");
				lua_State*state = GetProgram()->GetState();
				PushRegisteredGlobal(state,GetRefID());
				int numParameters = Detail::PushParameters(state,parameters...);
				String error;
				if (!CallPushedFunction(state,numParameters,0,error))
					throw Except::Program::GeneralFault("Lua: "+error);
			}

		};


	template <typename return_t, typename ...args_t>
		class Function<return_t(args_t...)> : public Reference
		{
			typedef Reference	Super;

		public:
			/**/		Function(){}
			/**/		Function(const PProgram&prog, const String&functionName):Super(prog,functionName){}

			void		Set(const Reference&ref)	{((Super&)*this) = ref;}

			return_t	operator()(args_t...parameters)	const
			{
				if (IsEmpty())
					throw Except::Program::GeneralFault("Lua: Trying to call empty function");
				lua_State*state = GetProgram()->GetState();
				PushRegisteredGlobal(state,GetRefID());
				int numParameters = Detail::PushParameters(state,parameters...);
				String error;
				if (!CallPushedFunction(state,numParameters,1,error))
					throw Except::Program::GeneralFault("Lua: "+error);
				return_t rs;
				Detail::GetStacked(GetProgram(),-1,rs);
				Detail::PopStacked(state);
				return rs;
			}

		};

}

#endif