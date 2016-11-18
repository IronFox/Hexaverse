#ifndef componentLuaInterfaceH
#define componentLuaInterfaceH

#include <global_string.h>
#include <interface/write_stream.h>
struct lua_State;

int LuaRun (int argc, const char *argv[]);


namespace Lua
{

	class Reference;
	class Program;

	typedef std::shared_ptr<lua_State>	PState;

	void	PushRegisteredGlobal(lua_State*, int refID);
	void	CallPushedFunction(lua_State*,int numParameters, int numResults);
	bool	LookupGlobal(lua_State *,const String&name, int&out);
	int		DuplicateReference(lua_State *, int ref);
	void	EraseReference(lua_State *, int ref);

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

	void GetStacked(const PState&, int addr, bool&rs);
	void GetStacked(const PState&, int addr, String&rs);
	void GetStacked(const PState&, int addr, void*&rs);
	void GetStacked(const PState&, int addr, long long&rs);
	void GetStacked(const PState&, int addr, double&rs);
	void GetStacked(const PState&, int addr, Reference&rs);
	inline void GetStacked(const PState&st, int addr, float&rs)
	{
		double tmp;
		GetStacked(st,addr,tmp);
		rs = (float)tmp;
	}

	inline void GetStacked(const PState&st, int addr, int&rs)
	{
		long long temp;
		GetStacked(st,addr,temp);
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
		String			name;

		static Program*	reportTo;

		static int		Report (lua_State *, int);
		static int		MessageHandler(lua_State*);

	public:
		bool			Create(const String&name = "");
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
			EraseReference(st.get(),(int)(size_t)raw);
		}
		void	SetReference(PState state, int refID)
		{
			this->state = state;
			this->refID.reset((int*)(size_t)refID,[state](int*ptr){Discard(state,ptr);});
		}
	protected:
		/**/	Reference(const Program&prog, const String&name)
		{
			Set(prog,name);
		}
		
	public:
		/**/		Reference(){}

		bool		SetFromStack(const PState&, int at);
		bool		Set(const Program&prog, const String&name)
		{
			int global;
			if (!LookupGlobal(prog.GetState().get(),name,global))
			{
				state.reset();
				refID.reset();
				return false;
			}
			SetReference(prog.GetState(), global);
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
				PushRegisteredGlobal(state,GetRefID());
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
				PState st = GetState();
				lua_State*state = st.get();
				PushRegisteredGlobal(state,GetRefID());
				int numParameters = PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,1);
				return_t rs;
				GetStacked(st,-1,rs);
				PopStacked(state);
				return rs;
			}

		};

}

#endif