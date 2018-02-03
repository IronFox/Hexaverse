#ifndef protocolH
#define protocolH


#undef FAR
#define FAR
#include <iphlpapi.h>
#include <net/tcp.h>
#include <net/containers.h>
#include "objects/entityType.h"
#include "grid/stack.h"
#include "grid/planet.h"
#include "objects.h"
#include "stringTable.h"
#include "runtimeConfig.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib, "IPHLPAPI.lib")


#define HEARTBEAT_PORT				10245	//no point making this variable
#define BROADCAST_PORT				10246	//must be constant
#define DEFAULT_SERVER_BASE_PORT	10247

namespace PortOffset
{
	/**
	@brief Service port delta to root port
	*/
	enum delta_t
	{
		Info,
		Session,
		Control
	};
}




#ifdef ONE_COMPOUND_PER_FACTION
	#define NAME_APPEND0	"o"
#else
	#define NAME_APPEND0
#endif

#ifdef NO_MUNITION
	#define NAME_APPEND1	"m"
#else
	#define NAME_APPEND1
#endif

#ifdef NO_SATELLITES
	#define NAME_APPEND_S	"s"
#else
	#define NAME_APPEND_S
#endif

#ifdef LOWER_RESOLUTION_PLANET
	#define NAME_APPEND_L	"l"
#else
	#define NAME_APPEND_L
#endif



#define SERVER_NAME		PRODUCT_NAME " v" PRODUCT_VERSION\
							"["\
							NAME_APPEND0\
							NAME_APPEND1\
							NAME_APPEND_S\
							NAME_APPEND_L\
							"]"

#include "objects/resource.h"








namespace Protocol
{














	#define NET(NATIVE_TYPE)	NATIVE_TYPE
	//::Package::Native<NATIVE_TYPE>
	#define NETA(TYPE)	::Protocol::NetArray<TYPE,32768>


	template<typename T, size_t MaxSize>
		class NetArray : public Array<T>
		{
		public:
			static const constexpr size_t MaxElementCount = MaxSize;
			typedef Array<T>	Super;
			typedef NetArray<T,MaxSize> Self;

			void				operator=(const Ctr::Array<T>&other)
			{
				Super::operator=(other);
			}

			friend	void		Deserialize(IReadStream&s, Self&v)
			{
				count_t size = s.ReadSize();
				if (size > MaxSize)
					throw Except::Memory::SerializationFault(CLOCATION,"Ctr::Array size "+String(size)+" exceeds maximum safe size "+String(MaxSize));
				using Serialization::Deserialize;
				v.SetSize(size);
				for (index_t i = 0; i < v.Count(); i++)
					Deserialize(s,v[i]);
			}

		};

	template <typename T>
		class NetArray64k : public NetArray<T,65536>
		{};
	template <typename T>
		class NetArray256 : public NetArray<T,256>
		{};


		


	template <typename T0, typename T1, typename T2, typename T3>
		class MultiSerializable4
		{
		protected:
			T0	var0;
			T1	var1;
			T2	var2;
			T3	var3;
		public:
			typedef MultiSerializable4<T0,T1,T2,T3>	Self;
			friend void Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				Deserialize(s,v.var0);
				Deserialize(s,v.var1);
				Deserialize(s,v.var2);
				Deserialize(s,v.var3);
			}
			friend void Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				Serialize(s,v.var0);
				Serialize(s,v.var1);
				Serialize(s,v.var2);
				Serialize(s,v.var3);
			}
			void	swap(MultiSerializable4&other)
			{
				using std::swap;
				swap(var0,other.var0);
				swap(var1,other.var1);
				swap(var2,other.var2);
				swap(var3,other.var3);
			}
			void	Copy(const MultiSerializable4&other)
			{
				var0 = other.var0;
				var1 = other.var1;
				var2 = other.var2;
				var3 = other.var3;
			}
		};

	#define M_TYPES4	typename T0, typename T1, typename T2, typename T3
	#define M_TYPES5	M_TYPES4, typename T4
	#define M_TYPES6	M_TYPES5, typename T5
	#define M_TYPES7	M_TYPES6, typename T6
	#define M_TYPES8	M_TYPES7, typename T7
	#define M_TYPES9	M_TYPES8, typename T8
	#define M_TYPES10	M_TYPES9, typename T9
	#define M_TYPES11	M_TYPES10, typename T10
	#define M_TYPES12	M_TYPES11, typename T11
	#define M_TYPES13	M_TYPES12, typename T12

	#define M_T4	T0, T1, T2, T3
	#define M_T5	M_T4, T4
	#define M_T6	M_T5, T5
	#define M_T7	M_T6, T6
	#define M_T8	M_T7, T7
	#define M_T9	M_T8, T8
	#define M_T10	M_T9, T9
	#define M_T11	M_T10, T10
	#define M_T12	M_T11, T11
	#define M_T13	M_T12, T12

	#define M_T(COUNT)		M_T##COUNT
	#define	M_TYPES(COUNT)	M_TYPES##COUNT
	#define M_CLASS(COUNT)	MultiSerializable##COUNT
	#define M_Ti(COUNT)		T##COUNT
	#define M_CONCAT(A,B)	A##B

	#include "multi_serializable.inc.h"

	MAKE_MUTLI_SERIALIZABLE_CLASS(4,5)
	MAKE_MUTLI_SERIALIZABLE_CLASS(5,6)
	MAKE_MUTLI_SERIALIZABLE_CLASS(6,7)
	MAKE_MUTLI_SERIALIZABLE_CLASS(7,8)
	MAKE_MUTLI_SERIALIZABLE_CLASS(8,9)
	MAKE_MUTLI_SERIALIZABLE_CLASS(9,10)
	MAKE_MUTLI_SERIALIZABLE_CLASS(10,11)
	MAKE_MUTLI_SERIALIZABLE_CLASS(11,12)
	MAKE_MUTLI_SERIALIZABLE_CLASS(12,13)


	#define NETSTRUCT2(NAME,  T0, NAME0, T1, NAME1)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			NAME()	{}\
			NAME(const T0&a, const T1&b):NAME0(a),NAME1(b){}\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};


	#define NETSTRUCT3(NAME,  T0, NAME0, T1, NAME1, T2, NAME2)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			NAME()	{}\
			NAME(const T0&NAME0, const T1&NAME1, const T2&NAME2):NAME0(NAME0),NAME1(NAME1),NAME2(NAME2){}\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
				Deserialize(s,v.NAME2);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
				Serialize(s,v.NAME2);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);swap(NAME2,other.NAME2);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

			
			
			
			
	#define NETSTRUCT4(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
				Deserialize(s,v.NAME2);\
				Deserialize(s,v.NAME3);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
				Serialize(s,v.NAME2);\
				Serialize(s,v.NAME3);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);swap(NAME2,other.NAME2);swap(NAME3,other.NAME3);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};
			
	#define NETSTRUCT5(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
				Deserialize(s,v.NAME2);\
				Deserialize(s,v.NAME3);\
				Deserialize(s,v.NAME4);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
				Serialize(s,v.NAME2);\
				Serialize(s,v.NAME3);\
				Serialize(s,v.NAME4);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);swap(NAME2,other.NAME2);swap(NAME3,other.NAME3);swap(NAME4,other.NAME4);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};
			
	#define NETSTRUCT6(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			T5		NAME5;\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
				Deserialize(s,v.NAME2);\
				Deserialize(s,v.NAME3);\
				Deserialize(s,v.NAME4);\
				Deserialize(s,v.NAME5);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
				Serialize(s,v.NAME2);\
				Serialize(s,v.NAME3);\
				Serialize(s,v.NAME4);\
				Serialize(s,v.NAME5);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);swap(NAME2,other.NAME2);swap(NAME3,other.NAME3);swap(NAME4,other.NAME4);swap(NAME5,other.NAME5);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

			
	#define NETSTRUCT7(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6)	\
		class NAME\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			T5		NAME5;\
			T6		NAME6;\
			friend void Deserialize(IReadStream&s, NAME&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,v.NAME0);\
				Deserialize(s,v.NAME1);\
				Deserialize(s,v.NAME2);\
				Deserialize(s,v.NAME3);\
				Deserialize(s,v.NAME4);\
				Deserialize(s,v.NAME5);\
				Deserialize(s,v.NAME6);\
			}\
			friend void Serialize(IWriteStream&s, const NAME&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,v.NAME0);\
				Serialize(s,v.NAME1);\
				Serialize(s,v.NAME2);\
				Serialize(s,v.NAME3);\
				Serialize(s,v.NAME4);\
				Serialize(s,v.NAME5);\
				Serialize(s,v.NAME6);\
			}\
			void		swap(NAME&other)	{using std::swap; swap(NAME0,other.NAME0);swap(NAME1,other.NAME1);swap(NAME2,other.NAME2);swap(NAME3,other.NAME3);swap(NAME4,other.NAME4);swap(NAME5,other.NAME5);swap(NAME6,other.NAME6);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

	#define NETSTRUCT8(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7)	\
		class NAME : public ::Protocol::MultiSerializable8<T0,T1,T2,T3,T4,T5,T6,T7>\
		{\
		public:\
			typedef MultiSerializable8<T0,T1,T2,T3,T4,T5,T6,T7>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

	#define NETSTRUCT10(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7,T8, NAME8,T9, NAME9)	\
		class NAME : public ::Protocol::MultiSerializable10<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9>\
		{\
		public:\
			typedef MultiSerializable10<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			T8&		NAME8;\
			T9&		NAME9;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7),NAME8(Super::var8),NAME9(Super::var9)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

	#define NETSTRUCT11(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7,T8, NAME8,T9, NAME9,T10, NAME10)	\
		class NAME : public ::Protocol::MultiSerializable11<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>\
		{\
		public:\
			typedef MultiSerializable11<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			T8&		NAME8;\
			T9&		NAME9;\
			T10&	NAME10;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7),NAME8(Super::var8),NAME9(Super::var9),NAME10(Super::var10)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

	#define NETSTRUCT12(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7,T8, NAME8,T9, NAME9,T10, NAME10,T11, NAME11)	\
		class NAME : public ::Protocol::MultiSerializable12<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11>\
		{\
		public:\
			typedef MultiSerializable12<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			T8&		NAME8;\
			T9&		NAME9;\
			T10&	NAME10;\
			T11&	NAME11;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7),NAME8(Super::var8),NAME9(Super::var9),NAME10(Super::var10),NAME11(Super::var11)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
			friend void swap(NAME&a, NAME&b) {a.swap(b);}\
		};

	#define NETSTRUCT9(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7,T8, NAME8)	\
		class NAME : public ::Protocol::MultiSerializable9<T0,T1,T2,T3,T4,T5,T6,T7,T8>\
		{\
		public:\
			typedef MultiSerializable9<T0,T1,T2,T3,T4,T5,T6,T7,T8>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			T8&		NAME8;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7),NAME8(Super::var8)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
		};
			

	#define NETSTRUCT13(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6, T7, NAME7,T8, NAME8,T9, NAME9,T10, NAME10,T11, NAME11,T12, NAME12)	\
		class NAME : public ::Protocol::MultiSerializable13<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11,T12>\
		{\
		public:\
			typedef MultiSerializable13<T0,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11,T12>	Super;\
			T0&		NAME0;\
			T1&		NAME1;\
			T2&		NAME2;\
			T3&		NAME3;\
			T4&		NAME4;\
			T5&		NAME5;\
			T6&		NAME6;\
			T7&		NAME7;\
			T8&		NAME8;\
			T9&		NAME9;\
			T10&	NAME10;\
			T11&	NAME11;\
			T12&	NAME12;\
			NAME():NAME0(Super::var0),NAME1(Super::var1),NAME2(Super::var2),NAME3(Super::var3),NAME4(Super::var4),NAME5(Super::var5),\
					NAME6(Super::var6),NAME7(Super::var7),NAME8(Super::var8),NAME9(Super::var9),NAME10(Super::var10),NAME11(Super::var11),\
					NAME12(Super::var12)\
					{}\
			void	operator=(const NAME&other)	{Super::Copy(other);}\
		};
			
	
	

	NETSTRUCT2(Timestamp
		, NET(bool), rightNow
		, NET(float), atTime
	)

	typedef Package::NetString<32>	ClientName,Hello;

	//NETSTRUCT2(ClientState
	//	, Package::NetString<64>, playerName
	//	, NET(UINT32), state	//place holder for client state. currently this is ignored, and always interpreted as a hello
	//)

	NETSTRUCT3(HelloResponse
		, NET(bool), success
		, UINT32, gameFlags
		, Package::NetString<256>, errorMessage	//in case stateAccepted is false
	)




	
	template <typename T0>
		class Optional
		{
		protected:
			BYTE	active;
			T0		option0;

		public:
			typedef Optional<T0>	Self;
			/**/				Optional():active(0)	{}

			void				swap(Optional&other)
			{
				swp(active,other.active);
				option0.swap(other.option0);
			}

	
			friend	void					Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				s.WritePrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Serialize(s,v.option0);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Optional<>: "+String(v.active));
				}
			}
			friend	void					Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				s.ReadPrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Deserialize(s,v.option0);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Optional<>: "+String(v.active));
				}
			}

			const T0*		Get0() const
			{
				return active == 1 ? &option0 : nullptr;
			}

			T0&				Set0()
			{
				active = 1;
				return option0;
			}
		};


	template <typename T0, typename T1>
		class Selective2 : public Optional<T0>
		{
		protected:
			typedef Optional<T0>	Super;
			typedef Selective2<T0,T1>	Self;

			T1		option1;
		public:
			void				swap(Selective2&other)
			{
				Super::swap(other);
				option1.swap(other.option1);
			}

			
			friend	void					Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				s.WritePrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Serialize(s,option0);
						break;
					case 2:
						Serialize(s,option1);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(active));
				}
			}

			friend	void					Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				s.ReadPrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Deserialize(s,option0);
						break;
					case 2:
						Deserialize(s,option1);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(active));
				}
			}

			const T1*		Get1() const
			{
				return active == 2 ? &option1 : nullptr;
			}

			T1&				Set1()
			{
				active = 2;
				return option1;
			}
		};


		
	template <typename T0, typename T1, typename T2>
		class Selective3 : public Selective2<T0,T1>
		{
		protected:
			typedef Selective2<T0,T1>	Super;
			typedef Selective3<T0,T1,T2>	Self;
			T2		option2;

		public:
			void				swap(Selective3&other)
			{
				Super::swap(other);
				option2.swap(other.option2);
			}
				
			friend	void					Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				s.WritePrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Serialize(s,v.option0);
						break;
					case 2:
						Serialize(s,v.option1);
						break;
					case 3:
						Serialize(s,v.option2);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(v.active));
				}
			}

			friend	void					Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				s.ReadPrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Deserialize(s,v.option0);
						break;
					case 2:
						Deserialize(s,v.option1);
						break;
					case 3:
						Deserialize(s,v.option2);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(v.active));
				}
			}

			const T2*		Get2() const
			{
				return active == 3 ? &option2 : nullptr;
			}

			T2&				Set2()
			{
				active = 3;
				return option2;
			}
		};


	
	template <typename T0, typename T1, typename T2, typename T3>
		class Selective4 : public Selective3<T0,T1,T2>
		{
		protected:
			typedef Selective3<T0,T1,T2>	Super;
			typedef Selective4<T0,T1,T2,T3>	Self;
			T3		option3;

		public:
			void				swap(Selective4&other)
			{
				Super::swap(other);
				option3.swap(other.option3);
			}
			friend	void					Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				s.WritePrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Serialize(s,v.option0);
						break;
					case 2:
						Serialize(s,v.option1);
						break;
					case 3:
						Serialize(s,v.option2);
						break;
					case 4:
						Serialize(s,v.option3);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(v.active));
				}
			}

			friend	void					Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				s.ReadPrimitive(v.active);
				switch (v.active)
				{
					case 0:
						break;
					case 1:
						Deserialize(s,v.option0);
						break;
					case 2:
						Deserialize(s,v.option1);
						break;
					case 3:
						Deserialize(s,v.option2);
						break;
					case 4:
						Deserialize(s,v.option3);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(v.active));
				}
			}

			const T3*		Get3() const
			{
				return active == 4 ? &option3 : nullptr;
			}

			T3&				Set3()
			{
				active = 4;
				return option3;
			}
		};






//	static_assert(sizeof(Grid::CellStack::TStructCountDeltaBlock) == sizeof(UINT32) + sizeof(Grid::CellStack::TStructCountDelta), "Struct alignment out of whack" );




	struct SerialUnsigned
	{
		typedef SerialUnsigned Self;
		serial_size_t	value = 0;
		
		void			swap(SerialUnsigned&other)
		{
			std::swap(value,other.value);
		}

		friend void Serialize(IWriteStream&s, const Self&v)
		{
			s.WriteSize(v.value);
		}

		friend	void	Deserialize(IReadStream&s, Self&v)
		{
			s.ReadSize(v.value);
		}

		void		operator=(int v)
		{
			ASSERT_GREATER_OR_EQUAL__(v,0);
			value = (serial_size_t)v;
		}

		operator serial_size_t&()
		{
			return value;
		}
	};


	//NETA(x)
	typedef NetArray<Grid::CellStack::TTypeDeltaBlock,4000000>	TypeDeltaArray;
	NETSTRUCT3(WorldDelta
		, Timestamp, timestamp
		, NET(BYTE), familyAndLastFlag	//flag stored in 0x80, family mask is 0x7f
		, TypeDeltaArray, data0
	)

	namespace EntityStateFlags
	{
		enum flag_t
		{
			IsUnstable = 0x1,
			IsSleeping = 0x2,
		};
	}




	NETSTRUCT6(EntityState
		, Timestamp, timestamp
		, TUniqueID, id
		, Grid::LCellIdI, location
		, SerialUnsigned, health
		, SerialUnsigned, empDamageLevel
		, BYTE, entityStateFlags
	)

	namespace TaskFlags
	{
		enum flag_t
		{
			VisualizePathToFocus = 0x1,
			DelayedBySleep = 0x2,
		};
	}

	NETSTRUCT10(MobileTask
		, NET(UINT8), type
		, NET(UINT8), mode
		, NET(UINT8), taskFlags
		, NETA(Grid::LCellIdI), foci
		, Package::NetString<256>, name
		, NET(USHORT), continuationsToJobCompletion
		, NET(USHORT), resourceCostLeft
		, BYTE, jobAffinity
		, NETA(Grid::LCellIdI), buildPath
		, Objects::EntityType, visualizationName	//can also be DetonationSize_Regular or DetonationSize_Doomsday
	)

	namespace VehicleFlags
	{
		enum flag_t
		{
			Automated = 0x1,
			Hibernating = 0x2,
			ConnectedToGround = 0x4,
			Building = 0x8,
		};
	}
	namespace StructureFlags
	{
		enum flag_t
		{
			Enabled = 0x1,
			Hibernating = 0x2,
			//ConnectedToSpace = 0x4,//never used
		};
	}

	
	NETSTRUCT2(ProductionTask
		, Objects::EntityType, type
		, TUniqueID, id
	)
	typedef NetArray<ProductionTask,Objects::MaxTasks> ProductionTasks;
	NETSTRUCT3(ProducerState
		, ProductionTasks, productionTasks
		, UINT32, productionDone
		, Grid::LCellIdI, gatheringPoint
	)


	#ifndef ONE_COMPOUND_PER_FACTION
		NETSTRUCT5(MyMobileEntity
			, NET(UINT16), constructionRoundCounter
			, PositiveResourceVector, resourceVector
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#else
		NETSTRUCT4(MyMobileEntity
			, NET(UINT16), constructionActionCounter
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#endif

	struct NoData
	{
		friend void			Serialize(IWriteStream&s, const NoData&v){}
		friend void			Deserialize(IReadStream&s, NoData&v){}

		void swap(NoData&)	{}
		friend void swap(NoData&, NoData&)	{}
	};




	NETSTRUCT5(MobileState
		, EntityState, entityState
		, NET(UINT8), flags
		, Optional<MyMobileEntity>, myMobileEntity
		, NET(float), attackActionLeft
		, NET(float), movementActionLeft
	)

	NETSTRUCT5(MobileEntityMotion
		, Timestamp, timestamp
		, NET(TUniqueID), id
		, Grid::LCellIdI, fromLocation
		, Grid::LCellIdI, toLocation
		, NET(float), timeNeeded
	)

	NETSTRUCT8(VagueAttackTrajectory
		, Timestamp, timestamp
		, NET(float), flightTime
		, NET(M::TVec3<>), sourcePosition
		, NET(ConstName), turretType
		, NET(float), turretRange
		, NET(float), attackAngle
		, NET(float), explosionRadius
		, NET(M::TVec3<>), traceTo
	)

	NETSTRUCT6(PreciseAttackTrajectory
		, Timestamp, timestamp
		, NET(float), flightTime
		, NET(TUniqueID), attacker
		, NET(float), attackAngle
		, NET(M::TVec3<>), traceTo
		, NET(bool), isAutoAttack
	)

	NETSTRUCT4(EntityTurretRotation
		, Timestamp, timestamp
		, NET(float), rotationTime
		, NET(TUniqueID), id
		, NET(float3), toWorld
	)

	NETSTRUCT4(Explosion
		, Timestamp, timestamp
		, NET(M::TVec3<>), center
		, NET(float), radius
		, NET(bool), isEMP
	)

	NETSTRUCT3(DestroyEntity
		, Timestamp, timestamp
		, NET(TUniqueID), id
		, NET(UINT8), type
	)


	typedef Grid::LCellIdI	AttackTask;

	NETSTRUCT2(MineState
		, Objects::PositiveResourceVector, naturalResources
		, NET(bool), linkedToCore
	)
	typedef NET(int)	NexusPortState;
	NETSTRUCT2(TowerState
		, NETA(AttackTask), attackTasks
		, NET(float), attackActionLeft
	)

	typedef Selective4<MineState,ProducerState,NexusPortState,TowerState>
		SelectiveStructureState;


	namespace AllianceFlag
	{
		enum flag_t
		{
			ShareSensors = 0x1,
			ExposeEntityDetails = 0x2,
		};

	}


	NETSTRUCT2(AllianceConfigChange
		, TUniqueID, allianceID
		, UINT32, flags
	)
	
	NETSTRUCT2(AllianceMemberChange
		, TUniqueID, allianceID
		, NETA(TUniqueID), memberFactions
	)


	namespace AllianceInviteType
	{
		enum T
		{
			Invite,
			Cancel,
			Accept,
			Reject
		};
	}

	NETSTRUCT3(AllianceInvite
		, TUniqueID, allianceID
		, TUniqueID, factionID
		, BYTE, type
	)

	namespace VoteResultType
	{
		enum T
		{
			Passed,
			Rejected,
			TimedOut,
			Canceled,
		};
	}

	NETSTRUCT3(VoteCreation
		, TUniqueID, voteID
		, Package::NetString<512>, phrase
		, float, remainingSeconds
	)
	
	NETSTRUCT6(VoteResult
		, TUniqueID, voteID
		, BYTE, resultType
		, UINT32, votedInFavor
		, UINT32, votedAgainst
		, UINT32, eligibleToVote
		, UINT32, quorum
	)

	NETSTRUCT2(ClientVote
		, TUniqueID, voteID
		, bool, inFavor
	)


	NETSTRUCT4(StructureState
		, EntityState, entityState
		, Objects::PositiveResourceVector, resourceVector
		, NET(BYTE), flags
		, SelectiveStructureState, selective
	)



	NETSTRUCT3(NewMobileEntity
		, MobileState, state
		, NET(TUniqueID), owner
		, Objects::EntityType, type
	)

	NETSTRUCT3(StructureConstruction
		, StructureState, state
		, NET(TUniqueID), owner
		, Objects::EntityType, type
	)


	namespace DirectFlags
	{
		enum flag_t
		{
			LeftClick = 0x1,
			Detailed = 0x2,
			ClearSafeTasks = 0x4
		};

	}

	/**
	If directFlags contains DirectFlags::Explicit, path is considered explicit with one entry per neighboring node.
	In this case the command is rejected, if the path is not possible when applied to the known world of the respective client.
	If the path is found to be inapplicable during computation phases, an automatically determined path to the last given node is computed and applied, then dropped.
	Path must always be of at least length 1
	*/
	typedef NetArray<Grid::LCellIdI,Objects::MaxDirectPathLength>	DirectPath;
	NETSTRUCT4(EntityDirectCommand
		, NET(TUniqueID), id
		, NET(UINT16),	mode
		, DirectPath, path
		, NET(UINT8), directFlags
	)


	NETSTRUCT4(ChoiceIntersection
		, NET(TUniqueID), id
		, Grid::LCellIdI, target
		, NET(UINT16), mode
		, Optional<NET(TUniqueID)>, deployingWhat
	)
	
	NETSTRUCT3(StandardJob
		, NET(TUniqueID), ownerID
		, NET(ConstName), jobName
		, Objects::EntityType, parameterName
		)

	typedef NetArray<Grid::HCellIdI,256>	CellArray;
	NETSTRUCT3(TerrainMod
		, NET(TUniqueID), id
		, NET(UINT16), mode
		, CellArray, cells
		)

	NETSTRUCT2(ConstructionAbort
		, NET(TUniqueID), plantID
		, NET(TUniqueID), taskId
	)

	NETSTRUCT2(ConstructionBegin
		, NET(TUniqueID), plantID
		, Objects::EntityType, type
	)

	NETSTRUCT2(EnableStructure
		, NET(TUniqueID), id
		, NET(bool), enable
		)


	NETSTRUCT5(ControlMessage
		, Package::NetString<256>, message
		, BYTE, significance
		, BYTE, flavor
		, bool, isPrivate
		, TUniqueID, focus	//may be faction or entity
	)




	NETSTRUCT3(PhaseChange
		, NET(UINT8), newPhase
		, NET(RuntimeConfig::RoundNumber), roundNumber
		, NET(float), timeFactor
	)


	typedef Grid::TCellResourceCoverage		TResourceCell;

	NETSTRUCT3(ResourceUpdate
		, Timestamp, timestamp
		, NET(ConstName), resourceName
		, NETA(TResourceCell), cells
	)


	struct FactionType
	{
		enum flag_t
		{
			ContinueOnly = 0xFFFFFFFF,
			NexusFaction = 0x1,
			SpectatorFaction = 0x2,
			Reserved1 = 0x4,
			RegularFaction = 0x8,
		};
	};


	typedef NET(SHA256::HashContainer) ResourceID;


	NETSTRUCT4(GameConfig
		, NET(UINT32), gameFlags
		, NET(BYTE), gameGoal
		, NET(BYTE), worldDiscovery
		, ResourceID, researchScript
	)	

	NETSTRUCT6(GameInfo
		, NET(UINT16), numFactionsOnline
		, NET(UINT16), numFactionsCreated
		, NET(RuntimeConfig::RoundNumber), roundNumber
		, NET(TUniqueID), worldID	//used to manage factionTokens
		, NET(BYTE), activePhase
		, GameConfig, gameConfig
	)


	NETSTRUCT4(ClientFactionResponse
		, NET(UINT32), startFactionTypes
		, NET(CryptographicID), factionToken
		, NET(Grid::TSharedPlanetConfig), planetConfig
		, GameInfo, info
	)

	NETSTRUCT2(ClientFactionRequest
		, NET(UINT32), startFactionTypes
		, NET(CryptographicID), factionToken
	)

	NETSTRUCT4(GameProgress
		, Package::NetString<256>, status
		, Package::NetString<256>, warning
		, NET(bool), gameWon
		, NET(bool), gameOver
	)
	NETSTRUCT2(ChangeEntityOwner
		, NET(TUniqueID), entityID
		, NET(TUniqueID), ownerID
	)

	NETSTRUCT2(EntityStateUpdate
		, NET(TUniqueID), entityID
		, NET(bool), doSet
	)

	NETSTRUCT3(MarkerReferenceEntity
		, NET(TUniqueID), id
		, NET(TUniqueID), owner
		, Objects::EntityType, type
	)

	static_assert(Objects::Marker::Type::NumberOfPossibilities == 3, "Please adjust marker logic");
	typedef Selective3<MarkerReferenceEntity, MarkerReferenceEntity, NoData>	MarkerData;

	NETSTRUCT4(Marker
		, NET(TUniqueID), id
		, Grid::LCellIdI, location		
		, MarkerData, data
		, NET(RuntimeConfig::RoundNumber), createdInRound
	)

	NETSTRUCT2(TouchedMarker
		, NET(TUniqueID), id
		, NET(RuntimeConfig::RoundNumber), newRoundNumber
	)

	NETSTRUCT4(IdentifyFaction
		, NET(TUniqueID), id
		, TUniqueID, allianceID
		, Package::NetString<32>, name
		, NET(M::TVec3<>), color
	)


	NETSTRUCT2(QueueResearch
		, UINT32,	research
		, bool,		drop
	);

	typedef NETA(UINT32)	ResearchQueue;

	typedef std::pair<Objects::EntityType,UINT16>	TUnitLimit;

	typedef NETA(TUnitLimit)	UnitLimits;


	static_assert(Objects::Resource::N<0x100,"Resource type limit exceeded: Must change storage type of OperationLacksResources array");
	NETSTRUCT3(OperationLacksResources
		, Timestamp, timestamp
		, NET(TUniqueID), entityID
		, NETA(BYTE), resourceIndices
	)

	NETSTRUCT2(ResearchState
		, NET(TUniqueID), ownerID
		, NETA(BYTE), serializedData
	)

	NETSTRUCT2(ResourceData
		, ResourceID, id
		, NETA(BYTE), data
	)

	CONSTRUCT_ENUMERATION3(DropMode, LastTask, AllTasks,AllSafeTasks);

	NETSTRUCT2(DropTasks
		, NET(TUniqueID), targetID
		, NET(BYTE), dropMode
	)

	NETSTRUCT2(LoadPlanetByName
		, Package::NetString<256>, name
		, UINT32, configVersion
	)

	NETSTRUCT2(MyFaction
		, TUniqueID, factionID
		, AllianceConfigChange, alliance
	)

	NETSTRUCT2(ChatMessage
		, Package::NetString<256>, message
		, TUniqueID, recipient	//may be faction, alliance, or everybody (if empty)
	)
		


	namespace ChannelID
	{
		enum id_t
		{
			Unused,

			Ping,				//c2s: <signal> sent once every two seconds, connection terminated after 10 seconds of inactivity
			Pong,				//s2c: <signal> sent as a response to Ping

			ControlMessage,		//s2c: ControlMessage
			GameProgress,		//s2c: GameProgress

			PhaseChange,		//s2c: PhaseChange
			BeginGame,			//s2c: <signal> //indicates that the player faction is fully initialized and transferred, and the game should begin. Mostly important for AI, only

			Hello,				//c2s: Hello -> s2c: HelloResponse
			ClientFaction,		//c2s: ClientFactionRequest -> s2c: ClientFactionResponse
			MyFaction,			//s2c: MyFaction

			RequestEverything,	//c2s: <signal> -> [everything] //(only if spectator) 
			BeginEverything,	//s2c: <signal> //indicates that now everything will be (re)sent
			EndEverything,		//s2c: <signal> //indicates that the server is done sending everything, and normal delta updates will resume

			WorldDelta,			//s2c : WorldDelta

			Explosion,			//s2c : Explosion

			NewMobileEntity,	//s2c: NewMobileEntity
			MobileState,		//s2c: MobileState
			MobileEntityMotion,		//s2c: MobileEntityMotion
			PreciseAttackTrajectory,	//s2c: PreciseAttackTrajectory
			VagueAttackTrajectory,		//s2c: VagueAttackTrajectory
			EntityTurretRotation,	//s2c: EntityTurretRotation

			ChangeEntityOwner,	//s2c: ChangeEntityOwner

			RemoveEntity,		//c2s: TUniqueID
			RestoreCoreLink,	//c2s: TUniqueID

			InstantiateStructure,	//s2c: StructureConstruction
			UpdateStructure,		//s2c: StructureState

			DestroyEntity,		//s2c: DestroyEntity

			ChoiceIntersection,	//c2s: ChoiceIntersection
			RequestRoundEnd,	//c2s: <signal>
			RequestContinueRound,	//c2s: <signal>

			EntityDirectCommand,//c2s: EntityDirectCommand

			DropTasks,			//c2s: DropTasks

			SignalNewJob,		//c2s: StandardJob
			SignalTerrainMod,	//c2s: TerrainMod

			ConstructionBegin,	//c2s: ConstructionBegin
			ConstructionAbort,	//c2s: ConstructionAbort

			EnableStructure,	//c2s: EnableStructure

			Automate,			//c2s: EntityStateUpdate
			Sleep,				//c2s: EntityStateUpdate

			ResourceUpdate,		//s2c: ResourceUpdate

			CreateMarker,		//c2s/s2c: Marker (non-dialogue)
			MarkerCreationFailedLimitReached,	//s2c: ClientMarker
			DestroyMarker,		//c2s/s2c: TUniqueID
			TouchMarker,		//c2s: TouchedMarker

			IdentifyFaction,	// [c2s: TUniqueID -> ] s2c: IdentifyFaction
			ChangeMyName,		// c2s: ClientName
			FactionOnline,		// s2c: TUniqueID
			FactionOffline,		// s2c: TUniqueID
			FactionRequestedNewRound,	// s2c: TUniqueID
			FactionContinuedRound,		// s2c: TUniqueID
			ResearchState,		// s2c : ResearchState
			QueueResearch,		// c2s : QueueResearch -> s2c : ResearchQueue
			ResearchQueue,		// s2c : ResearchQueue
			RequestResource,	// c2s : ResourceID -> s2c : ResourceData
			ResourceData,		// s2c : ResourceData

			LockEMPZones,		// s2c : UINT16
			UplinkBandwidth,	// s2c : int, can be negative to indicate it's disabled
			UnitLimits,			// s2c : UnitLimits

			ChatMessage,		// c2s: ChatMessage

			AllianceConfigChange,	//both: AllianceConfigChange
			AllianceInvite,		//both: AllianceInvite
			LeaveAlliance,		//c2s: <signal>


			VoteCreation,		//s2c: VoteCreation
			VoteResult,			//s2c: VoteResult
			ClientVote,			//c2s: ClientVote

			PlanetConfig,		// both: Grid::ExtendedPlanetConfig
			GameConfig,			// both: GameConfig
			PlanetNames,		// s2c: NetString<256>[]
			LoadPlanetByName,	// c2s: LoadPlanetByName
			EndConfigurationMode,//c2s: <signal>

			OperationLacksResources, //s2c: OperationLacksResources

			TutorialAt,			// both: int32

			WinningFaction,		// s2c: TUniqueID (if game has winner)

			BeginStatisticsSequence,	//s2c: TUniqueID (factionID)
			EndStatisticsSequence,		//s2c: <signal>
			RoundStatistics,			//s2c: Statistics::Round
			EntityTypeStatistics,		//s2c: Statistics::EntityType
			FactionInteractionStatistics,	//s2c: Statistics::FactionInteraction
		};
	}





	struct THostBroadcastPacket
	{
		typedef THostBroadcastPacket Self;
		TUniqueID	hostID;
		char		version[64];
		UINT16		serverBasePort;


		friend void Serialize(IWriteStream&s, const Self&v)
		{
			using Serialization::Serialize;
			Serialize(s,v.hostID);
			s.Write(v.version,sizeof(v.version));
			Serialize(s,v.serverBasePort);
		}
		friend void Deserialize(IReadStream&s, Self&v)
		{
			using Serialization::Deserialize;
			Deserialize(s,v.hostID);
			s.Read(v.version,sizeof(v.version));
			Deserialize(s,v.serverBasePort);
		}
	};

	static const size_t THostBroadcastPacket_size = sizeof(THostBroadcastPacket);

	static_assert(sizeof(THostBroadcastPacket) == sizeof(TUniqueID) + 64 + 2, "Struct packing not appropriate for host broadcast packet");//make sure there are no gaps


	namespace GameFlag
	{
		enum flag_t
		{
			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,
			EMPZones = 0x10,
			SimulateLiquids = 0x20,
			ConfigurationMode = 0x40,
			TutorialMode = 0x80,
			GameHasEnded = 0x100,

			SupportSpectatorFactions = 0x1000,
			SupportRegularFactions = 0x2000,


		};

		static const UINT32 AllFactionSupport = ~(SupportSpectatorFactions-1);

	}




	NETSTRUCT2(ServerInfo
		, NET(THostBroadcastPacket), hostInfo
		, GameInfo, gameInfo
	)
	
	typedef NET(UINT64)		Ping;

	namespace InfoChannel
	{
		enum channel_t
		{
			ServerInfo,	//s2c: ServerInfo
			Ping,
		
		
		};
	
	}



}




#endif
