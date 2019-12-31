#pragma once

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

			/**/				NetArray()	{}
			/**/				NetArray(const ArrayRef<T>&other)
			{
				operator=(other);
			}

			void				operator=(const ArrayRef<T>&other)
			{
				ASSERT_LESS_OR_EQUAL__(other.Count(),MaxSize);
				Super::operator=(other);
			}

			void				operator=(const ArrayRef<const T>&other)
			{
				ASSERT_LESS_OR_EQUAL__(other.Count(),MaxSize);
				Super::operator=(other);
			}

#ifdef __ARRAY_RVALUE_REFERENCES__
			void				operator=(ArrayRef<T>&&other)
			{
				ASSERT_LESS_OR_EQUAL__(other.Count(), MaxSize);
				Super::operator=(std::move(other));
			}
#endif

			void				SetSize(count_t newSize)
			{
				ASSERT_LESS_OR_EQUAL__(newSize,MaxSize);
				Super::SetSize(newSize);
			}

			friend	void		Deserialize(IReadStream&s, Self&v)
			{
				count_t size = s.ReadSize();
				if (size > MaxSize)
					throw Except::Memory::SerializationFault(CLOCATION,"Array<"+String(typeid(T).name())+ ">: Deserialized element count "+String(size)+" exceeds maximum permitted element count "+String(MaxSize));
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

	template <typename T0, typename T1, typename T2, typename T3, typename T4>
		class Selective5 : public Selective4<T0,T1,T2,T3>
		{
		protected:
			typedef Selective4<T0,T1,T2,T3>	Super;
			typedef Selective5<T0,T1,T2,T3,T4>	Self;
			T4		option4;

		public:
			void				swap(Selective5&other)
			{
				Super::swap(other);
				option4.swap(other.option4);
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
					case 5:
						Serialize(s,v.option4);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective5<>: "+String(v.active));
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
					case 5:
						Deserialize(s,v.option4);
						break;
					default:
						throw Except::Program::DataConsistencyFault(CLOCATION,"Invalid number of active elements in Selective2<>: "+String(v.active));
				}
			}

			const T4*		Get4() const
			{
				return active == 5 ? &option4 : nullptr;
			}

			T4&				Set4()
			{
				active = 5;
				return option4;
			}
		};

}
