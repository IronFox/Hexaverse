#ifndef componentLuaInterfaceH
#define componentLuaInterfaceH

#include <global_string.h>
struct lua_State;

int LuaRun (int argc, const char *argv[]);


namespace Lua
{

	void	PushRegisteredFunction(lua_State*, int refID);
	void	CallPushedFunction(lua_State*,int numParameters, int numResults);
	int		LookupFunction(lua_State *,const String&name);
	int		DuplicateReference(lua_State *, int ref);
	void	EraseReference(lua_State *, int ref);

	void PushParameter(lua_State *state, long long i);
	inline void PushParameter(lua_State *state, int i) {PushParameter(state,(long long)i);}
	void PushParameter(lua_State *state, double i);
	void PushParameter(lua_State *state, void*p);

	void GetStacked(lua_State *state, int addr, bool&rs);
	void GetStacked(lua_State *state, int addr, String&rs);
	void GetStacked(lua_State *state, int addr, void*&rs);
	void GetStacked(lua_State *state, int addr, long long&rs);
	void GetStacked(lua_State *state, int addr, double&rs);

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
			PushParameter(state,p0);
			return 1;
		}

	template <typename T0, typename ...T>
		static inline int PushParameters(lua_State *state, const T0&p0, const T&...p)
		{
			PushParameter(state,p0);
			return 1 + PushParameters(state,p...);
		}


	template<typename S>
		class Function;

	template <typename return_t, typename ...args_t>
		class Function<return_t(args_t...)>
		{
			lua_State *state=nullptr;
			int refID=-1;


		public:
			/**/		Function()
			{}

			/**/		Function(lua_State*state, const String&functionName):state(state)
			{
				refID = LookupFunction(state,functionName);
			}
			/**/		Function(const Function&other):state(other.state)
			{
				refID = DuplicateReference(state,other.refID);
			}
			/**/		~Function()
			{
				if (state)
					EraseReference(state,refID);
			}

			void		operator=(const Function&other)
			{
				if (state)
					EraseReference(state,refID);
				state = other.state;
				refID = DuplicateReference(state,other.refID);
			}

			bool		IsSet() const {return state != nullptr;}
			bool		IsEmpty() const {return state == nullptr;}

			void		swap(Function&other)
			{
				swp(state,other.state);
				swp(refID,other.refID);
			}

			return_t	operator()(args_t...parameters)	const
			{
				PushRegisteredFunction(state,refID);
				int numParameters = PushParameters(state,parameters...);
				CallPushedFunction(state,numParameters,1);
				return_t rs;
				GetStacked(state,-1,rs);
				return rs;
			}

		};

}

#endif