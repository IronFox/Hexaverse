#ifndef componentLuaInterfaceH
#define componentLuaInterfaceH

#include <global_string.h>
struct lua_State;

int LuaRun (int argc, const char *argv[]);


namespace Lua
{

	void	PushRegisteredFunction(lua_State*, int refID);
	void	CallPushedFunction(lua_State*,int numParameters, int numResults);
	int		LookupGlobal(lua_State *,const String&name);
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
			PushParameter(state,p0);
			return 1;
		}

	template <typename T0, typename ...T>
		static inline int PushParameters(lua_State *state, const T0&p0, const T&...p)
		{
			PushParameter(state,p0);
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


	class Reference
	{
	private:
		lua_State *state=nullptr;
		int refID=-1;

	protected:
		/**/	Reference(){}
		/**/	Reference(lua_State*state, const String&name):state(state)
		{
			refID = LookupGlobal(state,name);
		}
		/**/	Reference(const Reference&other):state(other.state)
		{
			refID = DuplicateReference(state,other.refID);
		}
		/**/	Reference(Reference&&other):state(other.state),refID(other.refID)
		{
			other.state = nullptr;
		}
		virtual		~Reference()
		{
			if (state)
				EraseReference(state,refID);
		}

		bool		IsSet() const {return state != nullptr;}
		bool		IsEmpty() const {return state == nullptr;}

		void		swap(Reference&other)
		{
			swp(state,other.state);
			swp(refID,other.refID);
		}

		void		operator=(const Reference&other)
		{
			if (state)
				EraseReference(state,refID);
			state = other.state;
			refID = DuplicateReference(state,other.refID);
		}

		void		operator=(Reference&&other)
		{
			adoptData(other);
		}

		void		adoptData(Reference&other)
		{
			if (state)
				EraseReference(state,refID);
			state = other.state;
			refID = other.refID;
			other.state = nullptr;
		}

		int			GetRefID() const {return refID;}
		lua_State*	GetState() const {return state;}
	};


	template<typename S>
		class Function;

	template <typename return_t, typename ...args_t>
		class Function<return_t(args_t...)> : public Reference
		{
			typedef Reference	Super;

		public:
			/**/		Function(){}
			/**/		Function(lua_State*state, const String&functionName):Super(state,functionName){}
			/**/		Function(const Function&other):Super(other){}






			return_t	operator()(args_t...parameters)	const
			{
				lua_State*state = GetState();
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