#ifndef componentLuaInterfaceH
#define componentLuaInterfaceH

#include <global_string.h>
#include <interface/write_stream.h>
struct lua_State;

int LuaRun (int argc, const char *argv[]);


namespace Lua
{

	typedef std::shared_ptr<lua_State>	PState;

	void	PushRegisteredFunction(lua_State*, int refID);
	void	CallPushedFunction(lua_State*,int numParameters, int numResults);
	bool	LookupGlobal(lua_State *,const String&name, int&out);
	int		DuplicateReference(lua_State *, int ref);
	void	EraseReference(lua_State *, int ref);

	void PushParameter(lua_State *state, long long i);
	inline void PushParameter(lua_State *state, int i) {PushParameter(state,(long long)i);}
	void PushParameter(lua_State *state, double i);
	void PushParameter(lua_State *state, void*p);
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


	void GetStacked(lua_State *state, int addr, bool&rs);
	void GetStacked(lua_State *state, int addr, String&rs);
	void GetStacked(lua_State *state, int addr, void*&rs);
	void GetStacked(lua_State *state, int addr, long long&rs);
	void GetStacked(lua_State *state, int addr, double&rs);
	inline void GetStacked(lua_State *state, int addr, float&rs)
	{
		double tmp;
		GetStacked(state,addr,tmp);
		rs = (float)tmp;
	}

	inline void GetStacked(lua_State *state, int addr, int&rs)
	{
		long long temp;
		GetStacked(state,addr,temp);
		rs = (int)temp;
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




	class Reference;


	class Program
	{
		std::shared_ptr<lua_State> state;
		String			lastError;

		static Program*	reportTo;

		static int		Report (lua_State *, int);
		static int		MessageHandler(lua_State*);

	public:
		bool			Create();
		const PState&	GetState() const {return state;}


		bool			FindGlobal(const String&name, Reference&)	const;
		bool			IsSet() const {return !!state;}
		bool			IsEmpty() const {return !state;}

		bool			Execute(const String&luaCode);

		const String&	GetLastError()	const	{return lastError;}

		bool			SerializeGlobal(const String&name,IWriteStream&);
		bool			DeserializeToGlobal(const String&name, IReadStream&);
	};

	class Reference
	{
	private:
		PState					state;
		std::shared_ptr<int>		refID;

		static void Discard(const PState&st, int*raw)
		{
			EraseReference(st.get(),reinterpret_cast<int>(raw));
		}
	protected:
		/**/	Reference(){}
		/**/	Reference(const Program&prog, const String&name)
		{
			Set(prog,name);
		}
		
	public:
		bool		Set(const Program&prog, const String&name)
		{
			int global;
			if (!LookupGlobal(prog.GetState().get(),name,global))
			{
				state.reset();
				refID.reset();
				return false;
			}
			state = prog.GetState();
			PState st = state;
			refID.reset(reinterpret_cast<int*>(global),[st](int*ptr){Discard(st,ptr);});
			return true;
		}

		bool		IsSet() const {return !!refID;}
		bool		IsEmpty() const {return !refID;}

		void		swap(Reference&other)
		{
			refID.swap(other.refID);
			state.swap(other.state);
		}

		int			GetRefID() const {return reinterpret_cast<int>( refID.get() );}
		const PState&GetState() const {return state;}
	};


	template<typename S>
		class Function;

	template <typename ...args_t>
		class Function<void(args_t...)> : public Reference
		{
			typedef Reference	Super;

		public:
			/**/		Function(){}
			/**/		Function(const Program&prog, const String&functionName):Super(prog,functionName){}

			void	operator()(args_t...parameters)	const
			{
				lua_State*state = GetState().get();
				PushRegisteredFunction(state,GetRefID());
				int numParameters = PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,0);
			}

		};


	template <typename return_t, typename ...args_t>
		class Function<return_t(args_t...)> : public Reference
		{
			typedef Reference	Super;

		public:
			/**/		Function(){}
			/**/		Function(const Program&prog, const String&functionName):Super(prog,functionName){}

			return_t	operator()(args_t...parameters)	const
			{
				lua_State*state = GetState().get();
				PushRegisteredFunction(state,GetRefID());
				int numParameters = PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,1);
				return_t rs;
				GetStacked(state,-1,rs);
				return rs;
			}

		};

}

#endif