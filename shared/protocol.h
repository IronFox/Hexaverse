#ifndef protocolH
#define protocolH


#include <iphlpapi.h>
#include <net/tcp.h>
#include <net/containers.h>
#include "grid.h"
#include "objects.h"
#include "stringTable.h"
#include "technite.h"


#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib, "IPHLPAPI.lib")


#define INFO_PORT			10245
#define SERVICE_PORT		10244
#define BROADCAST_PORT		10243
#define BROADCAST_FROM_PORT	10242
//#define SERVER_NAME		PRODUCT_NAME" server v"PRODUCT_VERSION

#ifdef CONNECTOR_LESS
	#define NAME_APPEND0	"c"
#else
	#define NAME_APPEND0
#endif

#ifdef NO_MUNITION
	#define NAME_APPEND1	"m"
#else
	#define NAME_APPEND1
#endif

#define NAME_APPEND2	"t"

#define SERVER_NAME		PRODUCT_NAME " v" PRODUCT_VERSION NAME_APPEND0 NAME_APPEND1 NAME_APPEND2


#include "objects/resource.h"

namespace Protocol
{











	template <typename Serializable>
	inline void						VerifySerialization(const Serializable&serializable, bool exportSize)
	{
		const serial_size_t expectedSize = serializable.GetSerialSize(exportSize);

		const size_t bufferSize = expectedSize*2+128;
		void*data = malloc(bufferSize);
		serial_size_t actualSize;
		ASSERT__(SerializeToMemory(serializable,data,(ISerializable::serial_size_t)bufferSize,exportSize,&actualSize));
		ASSERT_EQUAL__(actualSize,expectedSize);
		
		Serializable copy;
		ASSERT__(DeserializeFromMemory(copy,data,actualSize,exportSize));
		free(data);
		ASSERT__(serializable == copy);
	}
	
	template <typename Serializable>
	inline void						VerifySerialization(const Serializable&serializable)
	{
		VerifySerialization(serializable,false);
		VerifySerialization(serializable,true);
	}



	#define NET(NATIVE_TYPE)	::Package::Native<NATIVE_TYPE>
	#define NETA(TYPE)	::Protocol::NetArray<TYPE,32768>


	template<typename T, size_t MaxSize>
		class NetArray : public Array<T>
		{
		public:
			typedef Array<T>	Super;
			void				operator=(const Array<T>&other)
			{
				Super::operator=(other);
			}

			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override
			{
				count_t size;
				if (fixedSize == EmbeddedSize)
				{
					//cout << "reading embedded size "<<endl;
					if (!inStream.ReadSize(size))
						return false;
					//cout << "deciphered "<<size<<" elements"<<endl;
				}
				else
				{
					//cout << "using fixed size "<<endl;
					if (!IsISerializable(data))
						size = (count_t)(fixedSize/sizeof(T));
					else
					{
						size_t fixedElementSize = FixedSizeSerializable<T>::GetSize();
						if (fixedElementSize != 0)
						{
							size = (count_t)(fixedSize/fixedElementSize);
						}
						else
							if (!inStream.ReadSize(size))
								return false;
					}
						//FATAL__("trying to Deserialize an array containing serializable objects from a fixed size stream data section not including any element count");
				}
				if (size > MaxSize)
					return false;

				setSize(size);
				if (!IsISerializable(data))
				{
					//cout << "data is not of i-serializable type. reading plain"<<endl;
					return inStream.Read(data,(serial_size_t)contentSize());
				}

				//cout << "data is of i-serializable type. invoking deserializers"<<endl;
				for (index_t i = 0; i < elements; i++)
				{
					if (!DeserializeObject(data+i,sizeof(T),inStream,EmbeddedSize))
					{
						//cout << "deserialization of element "<<i<<" with embedded size failed"<<endl;
						return false;
					}
				}

				return true;
			}

		};

	template <typename T>
		class NetArray64k : public NetArray<T,65536>
		{};
	template <typename T>
		class NetArray256 : public NetArray<T,256>
		{};


		


	template <typename T0, typename T1, typename T2, typename T3>
		class MultiSerializable4 : public SerializableObject
		{
		protected:
			T0	var0;
			T1	var1;
			T2	var2;
			T3	var3;
		public:
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override
			{
				return var0.GetSerialSize(true)+var1.GetSerialSize(true) + var2.GetSerialSize(true) + var3.GetSerialSize(true);
			}
			virtual	bool						HasFixedSize() const	override
			{
				return var0.HasFixedSize() && var1.HasFixedSize() && var2.HasFixedSize() && var3.HasFixedSize();
			}
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override
			{
				return var0.Serialize(outStream,true) && var1.Serialize(outStream,true) && var2.Serialize(outStream,true) && var3.Serialize(outStream,true);
			}
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override
			{
				if (!var0.Deserialize(inStream,EmbeddedSize))
					DBG_FATAL__("Failed to Deserialize var0");
				if (!var1.Deserialize(inStream,EmbeddedSize))
					DBG_FATAL__("Failed to Deserialize var1");
				if (!var2.Deserialize(inStream,EmbeddedSize))
					DBG_FATAL__("Failed to Deserialize var2");
				if (!var3.Deserialize(inStream,EmbeddedSize))
					DBG_FATAL__("Failed to Deserialize var3");
				return true;
				//return	var0.Deserialize(inStream,EmbeddedSize) && var1.Deserialize(inStream,EmbeddedSize) && var2.Deserialize(inStream,EmbeddedSize) && var3.Deserialize(inStream,EmbeddedSize);
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
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			NAME()	{}\
			NAME(const T0&a, const T1&b):NAME0(a),NAME1(b){}\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				if (NAME1.HasFixedSize())\
					return NAME0.GetSerialSize(exportSize) + NAME1.GetSerialSize(true);\
				if (NAME0.HasFixedSize())\
					return NAME0.GetSerialSize(true) + NAME1.GetSerialSize(exportSize);\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				if (NAME1.HasFixedSize())\
					return 	NAME0.Serialize(outStream,exportSize) && NAME1.Serialize(outStream,true);\
				if (NAME0.HasFixedSize())\
					return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,exportSize);\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				if (fixedSize != EmbeddedSize)\
				{\
					if (NAME0.HasFixedSize())\
					{\
						if (!NAME0.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const0 Mode: Unable to Deserialize var0/2 "+String(#NAME0));\
							return false;\
						}\
						fixedSize -= NAME0.GetSerialSize(true);\
						if (!NAME1.Deserialize(inStream,fixedSize))\
						{\
							DBG_FATAL__("Const0 Mode: Unable to Deserialize var1/2 "+String(#NAME1)+" with fixed size "+String(fixedSize));\
							return false;\
						}\
						return true;\
					}\
					if (NAME1.HasFixedSize())\
					{\
						if (!NAME0.Deserialize(inStream,fixedSize-NAME1.GetSerialSize(true)))\
						{\
							DBG_FATAL__("Const1 Mode: Unable to Deserialize var0/2 "+String(#NAME0)+" with fixed size "+String(fixedSize-NAME1.GetSerialSize(true)));\
							return false;\
						}\
						if (!NAME1.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const1 Mode: Unable to Deserialize var1/2 "+String(#NAME1));\
							return false;\
						}\
						return true;\
					}\
				}\
				if (!NAME0.Deserialize(inStream,EmbeddedSize))\
				{\
					DBG_FATAL__("Unable to Deserialize var0/2 "+String(#NAME0));\
					return false;\
				}\
				if (!NAME1.Deserialize(inStream,EmbeddedSize))\
				{\
					DBG_FATAL__("Unable to Deserialize var1/2 "+String(#NAME1));\
					return false;\
				}\
				return true;\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);}\
		};


	#define NETSTRUCT3(NAME,  T0, NAME0, T1, NAME1, T2, NAME2)	\
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				if (NAME1.HasFixedSize() && NAME2.HasFixedSize())\
					return NAME0.GetSerialSize(exportSize) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true);\
				if (NAME0.HasFixedSize() && NAME2.HasFixedSize())\
					return NAME0.GetSerialSize(true) + NAME1.GetSerialSize(exportSize) + NAME2.GetSerialSize(true);\
				if (NAME0.HasFixedSize() && NAME1.HasFixedSize())\
					return NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(exportSize);\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true);\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);NAME2.swap(other.NAME2);}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize() && NAME2.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				if (NAME1.HasFixedSize() && NAME2.HasFixedSize())\
					return NAME0.Serialize(outStream,exportSize) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true);\
				if (NAME0.HasFixedSize() && NAME2.HasFixedSize())\
					return NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,exportSize) && NAME2.Serialize(outStream,true);\
				if (NAME0.HasFixedSize() && NAME1.HasFixedSize())\
					return NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,exportSize);\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				if (fixedSize != EmbeddedSize)\
				{\
					if (NAME0.HasFixedSize() && NAME2.HasFixedSize())\
					{\
						if (!NAME0.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const02 Mode: Unable to Deserialize var0/3 "+String(#NAME0));\
							return false;\
						}\
						fixedSize -= NAME0.GetSerialSize(true);\
						fixedSize -= NAME2.GetSerialSize(true);\
						if (!NAME1.Deserialize(inStream,fixedSize))\
							DBG_FATAL__("Const02 Mode: Unable to Deserialize var1/3 "+String(#NAME1)+" with fixed size="+String(fixedSize));\
						if (!NAME2.Deserialize(inStream,EmbeddedSize))\
							DBG_FATAL__("Const02 Mode: Unable to Deserialize var2/3 "+String(#NAME2));\
						return true;\
					}\
					if (NAME1.HasFixedSize() && NAME2.HasFixedSize())\
					{\
						serial_size_t elementSize = fixedSize-NAME1.GetSerialSize(true)-NAME2.GetSerialSize(true);\
						if (!NAME0.Deserialize(inStream,elementSize))\
						{\
							DBG_FATAL__("Const12 Mode: Unable to Deserialize var0/3 "+String(#NAME0)+" with fixed size="+String(elementSize));\
							return false;\
						}\
						if (!NAME1.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const12 Mode: Unable to Deserialize var1/3 "+String(#NAME1));\
							return false;\
						}\
						if (!NAME2.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const12 Mode: Unable to Deserialize var2/3 "+String(#NAME2));\
							return false;\
						}\
						return true;\
					}\
					if (NAME0.HasFixedSize() && NAME1.HasFixedSize())\
					{\
						serial_size_t elementSize = fixedSize-NAME0.GetSerialSize(true)-NAME1.GetSerialSize(true);\
						if (!NAME0.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const01 Mode: Unable to Deserialize var0/3 "+String(#NAME0));\
							return false;\
						}\
						if (!NAME1.Deserialize(inStream,EmbeddedSize))\
						{\
							DBG_FATAL__("Const01 Mode: Unable to Deserialize var1/3 "+String(#NAME1));\
							return false;\
						}\
						if (!NAME2.Deserialize(inStream,elementSize))\
						{\
							DBG_FATAL__("Const01 Mode: Unable to Deserialize var2/3 "+String(#NAME2)+" with fixed size="+String(elementSize));\
							return false;\
						}\
						return true;\
					}\
				}\
				if (!NAME0.Deserialize(inStream,EmbeddedSize))\
				{\
					DBG_FATAL__("Embedded mode: Unable to Deserialize var0/3 "+String(#NAME0));\
					return false;\
				}\
				if (!NAME1.Deserialize(inStream,EmbeddedSize))\
				{\
					DBG_FATAL__("Embedded mode: Unable to Deserialize var1/3 "+String(#NAME1));\
					return false;\
				}\
				if (!NAME2.Deserialize(inStream,EmbeddedSize))\
				{\
					DBG_FATAL__("Embedded mode: Unable to Deserialize var2/3 "+String(#NAME2));\
					return false;\
				}\
				return true;\
			}\
		};

			
			
			
			
	#define NETSTRUCT4(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3)	\
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true) + NAME3.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize() && NAME2.HasFixedSize() && NAME3.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true) && NAME3.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				return	NAME0.Deserialize(inStream,EmbeddedSize) && NAME1.Deserialize(inStream,EmbeddedSize)&& NAME2.Deserialize(inStream,EmbeddedSize)&& NAME3.Deserialize(inStream,EmbeddedSize);\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);NAME2.swap(other.NAME2);NAME3.swap(other.NAME3);}\
		};
			
	#define NETSTRUCT5(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4)	\
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true) + NAME3.GetSerialSize(true)+ NAME4.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize() && NAME2.HasFixedSize() && NAME3.HasFixedSize() && NAME4.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true) && NAME3.Serialize(outStream,true)&& NAME4.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				return	NAME0.Deserialize(inStream,EmbeddedSize) && NAME1.Deserialize(inStream,EmbeddedSize)\
						&& NAME2.Deserialize(inStream,EmbeddedSize)&& NAME3.Deserialize(inStream,EmbeddedSize)\
						&& NAME4.Deserialize(inStream,EmbeddedSize);\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);NAME2.swap(other.NAME2);NAME3.swap(other.NAME3);NAME4.swap(other.NAME4);}\
		};
			
	#define NETSTRUCT6(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5)	\
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			T5		NAME5;\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true) + NAME3.GetSerialSize(true)+ NAME4.GetSerialSize(true)+ NAME5.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize() && NAME2.HasFixedSize() && NAME3.HasFixedSize() && NAME4.HasFixedSize() && NAME5.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true) && NAME3.Serialize(outStream,true)&& NAME4.Serialize(outStream,true)&& NAME5.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				return	NAME0.Deserialize(inStream,EmbeddedSize) && NAME1.Deserialize(inStream,EmbeddedSize)\
						&& NAME2.Deserialize(inStream,EmbeddedSize)&& NAME3.Deserialize(inStream,EmbeddedSize)\
						&& NAME4.Deserialize(inStream,EmbeddedSize)&& NAME5.Deserialize(inStream,EmbeddedSize);\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);NAME2.swap(other.NAME2);NAME3.swap(other.NAME3);NAME4.swap(other.NAME4);NAME5.swap(other.NAME5);}\
		};

			
	#define NETSTRUCT7(NAME,  T0, NAME0, T1, NAME1, T2, NAME2, T3, NAME3, T4, NAME4, T5, NAME5, T6, NAME6)	\
		class NAME : public SerializableObject\
		{\
		public:\
			T0		NAME0;\
			T1		NAME1;\
			T2		NAME2;\
			T3		NAME3;\
			T4		NAME4;\
			T5		NAME5;\
			T6		NAME6;\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				return 	NAME0.GetSerialSize(true) + NAME1.GetSerialSize(true) + NAME2.GetSerialSize(true) + NAME3.GetSerialSize(true)+ NAME4.GetSerialSize(true)+ NAME5.GetSerialSize(true)+ NAME6.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return NAME0.HasFixedSize() && NAME1.HasFixedSize() && NAME2.HasFixedSize() && NAME3.HasFixedSize() && NAME4.HasFixedSize() && NAME5.HasFixedSize() && NAME6.HasFixedSize();\
			}\
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				return 	NAME0.Serialize(outStream,true) && NAME1.Serialize(outStream,true) && NAME2.Serialize(outStream,true) && NAME3.Serialize(outStream,true)&& NAME4.Serialize(outStream,true)&& NAME5.Serialize(outStream,true)&& NAME6.Serialize(outStream,true);\
			}\
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				return	NAME0.Deserialize(inStream,EmbeddedSize) && NAME1.Deserialize(inStream,EmbeddedSize)\
						&& NAME2.Deserialize(inStream,EmbeddedSize)&& NAME3.Deserialize(inStream,EmbeddedSize)\
						&& NAME4.Deserialize(inStream,EmbeddedSize)&& NAME5.Deserialize(inStream,EmbeddedSize)&& NAME6.Deserialize(inStream,EmbeddedSize);\
			}\
			void		swap(NAME&other)	{NAME0.swap(other.NAME0);NAME1.swap(other.NAME1);NAME2.swap(other.NAME2);NAME3.swap(other.NAME3);NAME4.swap(other.NAME4);NAME5.swap(other.NAME5);NAME6.swap(other.NAME6);}\
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
			
	
	CONSTRUCT_ENUMERATION2(ServerMode, Idle, Active);


	NETSTRUCT2(Timestamp
		, NET(bool), rightNow
		, NET(float), atTime
	)


	NETSTRUCT3(ClientState
		, Package::NetString<64>, playerName
		, NET(TUniqueID), factionToken
		, NET(UINT32), state	//gonna fill this out later
	)

	NETSTRUCT2(Result
		, NET(bool), success
		, Package::NetString<256>, errorMessage	//in case stateAccepted is false
	)




	
	template <typename T0>
		class Optional : public SerializableObject
		{
		protected:
			BYTE	active;
			T0		option0;

		public:
			/**/				Optional():active(0)	{}

			void				swap(Optional&other)
			{
				swp(active,other.active);
				option0.swap(other.option0);
			}

			virtual	serial_size_t			GetSerialSize(bool exportSize) const override
			{
				switch (active)
				{
					case 0:
						return 1;
					case 1:
						return option0.GetSerialSize(exportSize)+1;
					default:
						FATAL__("Invalid state");
				}
				return 1;
			}
		
			virtual	bool					Serialize(IWriteStream&outStream, bool exportSize) const override
			{
				if (!outStream.WritePrimitive(active))
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Serialize(outStream,exportSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
			}
			virtual	bool					Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
			{
				if (fixedSize != EmbeddedSize)
					fixedSize --;
				if (!inStream.ReadPrimitive(active) || active > 2)
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Deserialize(inStream,fixedSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
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

			T1		option1;
		public:
			void				swap(Selective2&other)
			{
				Super::swap(other);
				option1.swap(other.option1);
			}

			virtual	serial_size_t			GetSerialSize(bool exportSize) const override
			{
				switch (active)
				{
					case 0:
						return 1;
					case 1:
						return option0.GetSerialSize(exportSize)+1;
					case 2:
						return option1.GetSerialSize(exportSize)+1;
					default:
						FATAL__("Invalid state");
				}
				return 1;
			}
		
			virtual	bool					Serialize(IWriteStream&outStream, bool exportSize) const override
			{
				if (!outStream.WritePrimitive(active))
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Serialize(outStream,exportSize);
					case 2:
						return option1.Serialize(outStream,exportSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
			}
			virtual	bool					Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
			{
				if (fixedSize != EmbeddedSize)
					fixedSize --;
				if (!inStream.ReadPrimitive(active) || active > 2)
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Deserialize(inStream,fixedSize);
					case 2:
						return option1.Deserialize(inStream,fixedSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
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
			T2		option2;

		public:
			void				swap(Selective3&other)
			{
				Super::swap(other);
				option2.swap(other.option2);
			}

			virtual	serial_size_t			GetSerialSize(bool exportSize) const override
			{
				switch (active)
				{
					case 0:
						return 1;
					case 1:
						return option0.GetSerialSize(exportSize)+1;
					case 2:
						return option1.GetSerialSize(exportSize)+1;
					case 3:
						return option2.GetSerialSize(exportSize)+1;
					default:
						FATAL__("Invalid state");
				}
				return 1;
			}
		
			virtual	bool					Serialize(IWriteStream&outStream, bool exportSize) const override
			{
				if (!outStream.WritePrimitive(active))
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Serialize(outStream,exportSize);
					case 2:
						return option1.Serialize(outStream,exportSize);
					case 3:
						return option2.Serialize(outStream,exportSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
			}
			virtual	bool					Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
			{
				if (fixedSize != EmbeddedSize)
					fixedSize --;
				if (!inStream.ReadPrimitive(active) || active > 3)
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Deserialize(inStream,fixedSize);
					case 2:
						return option1.Deserialize(inStream,fixedSize);
					case 3:
						return option2.Deserialize(inStream,fixedSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
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
			T3		option3;

		public:
			void				swap(Selective4&other)
			{
				Super::swap(other);
				option3.swap(other.option3);
			}

			virtual	serial_size_t			GetSerialSize(bool exportSize) const override
			{
				switch (active)
				{
					case 0:
						return 1;
					case 1:
						return option0.GetSerialSize(exportSize)+1;
					case 2:
						return option1.GetSerialSize(exportSize)+1;
					case 3:
						return option2.GetSerialSize(exportSize)+1;
					case 4:
						return option3.GetSerialSize(exportSize)+1;
					default:
						FATAL__("Invalid state");
				}
				return 1;
			}
		
			virtual	bool					Serialize(IWriteStream&outStream, bool exportSize) const override
			{
				if (!outStream.WritePrimitive(active))
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Serialize(outStream,exportSize);
					case 2:
						return option1.Serialize(outStream,exportSize);
					case 3:
						return option2.Serialize(outStream,exportSize);
					case 4:
						return option3.Serialize(outStream,exportSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
			}
			virtual	bool					Deserialize(IReadStream&inStream, serial_size_t fixedSize) override
			{
				if (fixedSize != EmbeddedSize)
					fixedSize --;
				if (!inStream.ReadPrimitive(active) || active > 4)
					return false;
				switch (active)
				{
					case 0:
						return true;
					case 1:
						return option0.Deserialize(inStream,fixedSize);
					case 2:
						return option1.Deserialize(inStream,fixedSize);
					case 3:
						return option2.Deserialize(inStream,fixedSize);
					case 4:
						return option3.Deserialize(inStream,fixedSize);
					default:
						FATAL__("Invalid state");
				}
				return false;
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






	static_assert(sizeof(Grid::CellStack::TTypeDeltaBlock) == sizeof(UINT32) + sizeof(Grid::CellStack::TTypeDelta), "Struct alignment out of whack" );
	static_assert(sizeof(Grid::CellStack::TStructCountDeltaBlock) == sizeof(UINT32) + sizeof(Grid::CellStack::TStructCountDelta), "Struct alignment out of whack" );



	class LCellIdI : public Grid::LCellIdI, public SerializableObject
	{
	public:
		void	swap(Grid::LCellIdI&other)
		{
			std::swap((Grid::LCellIdI&)*this, other);
		}


		LCellIdI() {}
		LCellIdI(const Grid::LCellIdI&source):Grid::LCellIdI(source){}

		virtual	serial_size_t			GetSerialSize(bool /*exportSize*/) const override
		{
			if (*this == Invalid)
				return 1;
			return GetSerialSizeOfSize(stackID) + 1;
		}
		
		virtual	bool					Serialize(IWriteStream&outStream, bool /*exportSize*/) const override
		{
			if (*this == Invalid)
			{
				BYTE bLayer = 0xFF;
				return outStream.Write(&bLayer,1);
			}
			UINT32 layer = Grid::CellStack::ToLayer(height);
			static_assert(Grid::CellStack::Range <= 255, "Protocol not designed for more than 255 layers");
			BYTE bLayer = (BYTE)layer;
			return outStream.Write(&bLayer,1) && outStream.WriteSize(stackID);
		}

		virtual	bool					Deserialize(IReadStream&inStream, serial_size_t /*fixedSize*/) override
		{
			BYTE bLayer;
			if (!inStream.Read(&bLayer,1))
				return false;
			if (bLayer == 0xFF)
			{
				*this = Invalid;
				return true;
			}
			if (!inStream.ReadSize(stackID))
				return false;
			height = Grid::CellStack::ToHeight((UINT)bLayer);
			return true;
		}

		bool						operator==(const Grid::LCellIdI&other) const
		{
			return other == *this;
		}
		bool						operator!=(const Grid::LCellIdI&other) const
		{
			return other != *this;
		}


		
		void						operator=(const Grid::LCellIdI&other)
		{
			((Grid::LCellIdI&)*this) = other;
		}
	};

	struct SerialUnsigned : public SerializableObject
	{
		serial_size_t	value = 0;
		
		void			swap(SerialUnsigned&other)
		{
			std::swap(value,other.value);
		}
		
		virtual	serial_size_t			GetSerialSize(bool /*exportSize*/) const override
		{
			return GetSerialSizeOfSize(value);
		}
		
		virtual	bool					Serialize(IWriteStream&outStream, bool /*exportSize*/) const override
		{
			return outStream.WriteSize(value);
		}

		virtual	bool					Deserialize(IReadStream&inStream, serial_size_t /*fixedSize*/) override
		{
			return inStream.ReadSize(value);
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
	typedef NetArray<Grid::CellStack::TStructCountDeltaBlock,4000000>	StructCountDeltaArray;

	typedef NetArray<Grid::CellStack::TTechniteFactionDeltaBlock,4000000>	TechniteFactionDeltaArray;
	NETSTRUCT5(WorldDelta
		, Timestamp, timestamp
		, NET(BYTE), familyAndLastFlag	//flag stored in 0x80, family mask is 0x7f
		, TypeDeltaArray, data0
		, StructCountDeltaArray, data1
		, TechniteFactionDeltaArray, data2
	)



	NETSTRUCT6(EntityState
		, Timestamp, timestamp
		, NET(TUniqueID), id
		, LCellIdI, location
		, SerialUnsigned, health
		, SerialUnsigned, empDamageLevel
		, NET(bool), unstable
	)

	NETSTRUCT9(MobileTask
		, NET(UINT8), type
		, NET(UINT8), mode
		, NET(bool), visualizePathToFocus
		, NETA(LCellIdI), foci
		, Package::NetString<256>, name
		, NET(USHORT), roundsToJobCompletion
		, NET(USHORT), mineralCostLeft
		, NETA(LCellIdI), buildPath
		//, Package::NetString<256>, visualizationStructureName
		, NET(ConstName), visualizationStructureName
	)

	//class ResourceVector : public Objects::ResourceVector, public SerializableObject
	//{
	//public:
	//	void	swap(ResourceVector&other)
	//	{
	//		ResourceVector tmp = *this;
	//		((ResourceVector&)*this) = other;
	//		other = tmp;
	//	}
	//	static	serial_size_t			GetSerialSize(const Objects::ResourceVector&vec)
	//	{
	//		serial_size_t rs = 1;	//flag byte
	//		static_assert(ResourceVector::NumEntries <= 4, "Adjust protocol");
	//		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
	//		{
	//			if (vec.level[i] != 0)
	//				rs += GetSerialSizeOfSize(abs(vec.level[i]));	//negative values are flagged accordingly
	//		}
	//		return rs;
	//	}


	//	virtual	serial_size_t			GetSerialSize(bool /*exportSize*/) const override
	//	{
	//		return GetSerialSize(*this);
	//	}
	//	static	bool					Serialize(IWriteStream&outStream, const Objects::ResourceVector&vec)
	//	{
	//		BYTE flag = 0;
	//		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
	//		{
	//			if (vec.level[i] != 0)
	//			{
	//				flag |= (1 << (i*2));
	//				if (vec.level[i] < 0)
	//					flag |= (1 << (i*2+1));
	//			}
	//		}
	//		if (!outStream.Write(&flag,1))
	//			return false;
	//		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
	//		{
	//			if (vec.level[i] != 0)
	//			{
	//				if (!outStream.WriteSize((serial_size_t)abs(vec.level[i])))
	//					return false;
	//			}
	//		}
	//		return true;
	//	}
	//	virtual	bool					Serialize(IWriteStream&outStream, bool /*exportSize*/) const override
	//	{
	//		return Serialize(outStream,*this);
	//	}
	//	static	bool					Deserialize(IReadStream&inStream, Objects::ResourceVector&vec)
	//	{
	//		BYTE flags;
	//		if (!inStream.Read(&flags,1))
	//			return false;

	//		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
	//		{
	//			bool exists = ((flags >> (i*2))&1) != 0;
	//			if (!exists)
	//			{
	//				vec.level[i] = 0;
	//				continue;
	//			}
	//			int val;
	//			if (!inStream.ReadSize(val))
	//				return false;
	//			bool neg = ((flags >> (i*2+1))&1) != 0;
	//			if (neg)
	//				val = -val;
	//			vec.level[i] = val;
	//		}
	//		return true;
	//	}
	//	virtual	bool					Deserialize(IReadStream&inStream, serial_size_t /*fixedSize*/) override
	//	{
	//		return Deserialize(inStream,*this);

	//	}


	//	bool		operator==(const Objects::ResourceVector&vec) const
	//	{
	//		return vec == *this;
	//	}
	//	bool		operator!=(const Objects::ResourceVector&vec) const
	//	{
	//		return vec != *this;
	//	}

	//	void		operator=(const Objects::ResourceVector&vec)
	//	{
	//		((Objects::ResourceVector&)*this) = vec;
	//	}
	//	void		operator=(const Objects::NaturalResourceVector&vec)
	//	{
	//		((Objects::ResourceVector&)*this) = vec;
	//	}	
	//};

	class PositiveResourceVector : public Objects::ResourceVector, public SerializableObject
	{
	public:
		void	swap(Objects::ResourceVector&other)
		{
			std::swap((Objects::ResourceVector&)*this,other);
		}
		static	serial_size_t			GetSerialSize(const Objects::ResourceVector&vec)
		{
			serial_size_t rs = 1;	//flag byte
			static_assert(Objects::ResourceVector::NumEntries <= 8, "Adjust protocol");
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			{
				if (vec.level[i] > 0)
					rs += GetSerialSizeOfSize(abs(vec.level[i]));	//negative values are set to zero
			}
			return rs;
		}


		virtual	serial_size_t			GetSerialSize(bool /*exportSize*/) const override
		{
			return GetSerialSize(*this);
		}
		static	bool					Serialize(IWriteStream&outStream, const Objects::ResourceVector&vec)
		{
			BYTE flag = 0;
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			{
				if (vec.level[i] > 0)
				{
					flag |= (1 << i);
				}
				else
				{
					DBG_ASSERT_EQUAL1__(vec.level[i], 0,Objects::Resource::Reinterpret(i).ToString());
				}
			}
			if (!outStream.Write(&flag,1))
				return false;
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			{
				if (vec.level[i] > 0)
				{
					if (!outStream.WriteSize((serial_size_t)(vec.level[i])))
						return false;
				}
			}
			return true;
		}
		virtual	bool					Serialize(IWriteStream&outStream, bool /*exportSize*/) const override
		{
			return Serialize(outStream,*this);
		}
		static	bool					Deserialize(IReadStream&inStream, Objects::ResourceVector&vec)
		{
			BYTE flags;
			if (!inStream.Read(&flags,1))
				return false;

			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			{
				bool exists = ((flags >> i)&1) != 0;
				if (!exists)
				{
					vec.level[i] = 0;
					continue;
				}
				UINT32 val;
				if (!inStream.ReadSize(val))
					return false;
				vec.level[i] = val;
			}
			return true;
		}
		virtual	bool					Deserialize(IReadStream&inStream, serial_size_t /*fixedSize*/) override
		{
			return Deserialize(inStream,*this);

		}


		bool		operator==(const Objects::ResourceVector&vec) const
		{
			return vec == *this;
		}
		bool		operator!=(const Objects::ResourceVector&vec) const
		{
			return vec != *this;
		}

		void		operator=(const Objects::ResourceVector&vec)
		{
			((Objects::ResourceVector&)*this) = vec;
		}
		void		operator=(const Objects::NaturalResourceVector&vec)
		{
			((Objects::ResourceVector&)*this) = vec;
		}	
	};
	//class FullResourceVector : public Objects::FullResourceVector, public SerializableObject
	//{
	//public:
	//	void	swap(Objects::FullResourceVector&other)
	//	{
	//		std::swap(((Objects::FullResourceVector&)*this),other);
	//	}	

	//	//FullResourceVector(const Objects::FullResourceVector&vec):Objects::FullResourceVector(vec)
	//	//{}

	//	virtual	serial_size_t			GetSerialSize(bool /*exportSize*/) const override
	//	{
	//		return	ResourceVector::GetSerialSize(supply)
	//			+	ResourceVector::GetSerialSize(demand)
	//			+	ResourceVector::GetSerialSize(balance)
	//			+	ResourceVector::GetSerialSize(storage)
	//			+	ResourceVector::GetSerialSize(storageMax);
	//	}

	//	virtual	bool					Serialize(IWriteStream&outStream, bool /*exportSize*/) const override
	//	{
	//		return	ResourceVector::Serialize(outStream,supply)
	//			&&	ResourceVector::Serialize(outStream,demand)
	//			&&	ResourceVector::Serialize(outStream,balance)
	//			&&	ResourceVector::Serialize(outStream,storage)
	//			&&	ResourceVector::Serialize(outStream,storageMax);
	//	}
	//	
	//	virtual	bool					Deserialize(IReadStream&inStream, serial_size_t /*fixedSize*/) override
	//	{
	//		return	ResourceVector::Deserialize(inStream,supply)
	//			&&	ResourceVector::Deserialize(inStream,demand)
	//			&&	ResourceVector::Deserialize(inStream,balance)
	//			&&	ResourceVector::Deserialize(inStream,storage)
	//			&&	ResourceVector::Deserialize(inStream,storageMax);
	//	}
	//
	//	void		operator=(const Objects::FullResourceVector&vec)
	//	{
	//		((Objects::FullResourceVector&)*this) = vec;
	//	}
	//};



	namespace VehicleFlags
	{
		enum flag_t
		{
			Automated = 0x1,
			Hibernating = 0x2,
			ConnectedToGround = 0x4,
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

	typedef NetArray<ConstName,Objects::MaxProductionTasks> ProductionDesc;
	NETSTRUCT2(ProductionTask
		, ProductionDesc, parameters
		, NET(TUniqueID), id
	)
	NETSTRUCT3(ProducerState
		, NETA(ProductionTask), productionTasks
		, NET(UINT32), productionDone
		, LCellIdI, gatheringPoint
	)


	#ifndef CONNECTOR_LESS
		NETSTRUCT5(MyMobileEntity
			, NET(UINT16), constructionRoundCounter
			, PositiveResourceVector, resourceVector
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#else
		NETSTRUCT4(MyMobileEntity
			, NET(UINT16), constructionRoundCounter
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#endif

	struct NoData : public SerializableObject
	{
		virtual	bool						HasFixedSize() const	override
		{
			return true;
		}

		virtual	serial_size_t				GetSerialSize(bool exportSize) const	override
		{
			return 0;
		}
		virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override
		{
			return true;
		}
		virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override
		{
			return true;
		}

		void swap(NoData&)	{}
	};



	template <typename T, count_t MaxElementCount>
		class SmartPODReferenceField : public NetArray<T,MaxElementCount>
		{
			typedef NetArray<T,MaxElementCount>	Super;
		private:
			const T*		sourceField=nullptr;
			count_t			sourceCount=0;
			bool			hasAssignedSource = false;

			void			_Assign(const T*field, count_t maxCount, index_t offset)
			{
				sourceField = field + offset;
				if (maxCount > offset)
					sourceCount = std::min(maxCount - offset,MaxElementCount);
				else
					sourceCount = 0;
				hasAssignedSource = true;
			}

		public:
			template <typename Strategy>
				void		AssignSection(const BasicBuffer<T,Strategy>&field, index_t offset = 0)
				{
					_Assign(field.pointer(),field.Count(),offset);
				}
			void			AssignSection(const ArrayData<T>&field, index_t offset = 0)
			{
				_Assign(field.pointer(),field.Count(),offset);
			}

			virtual	bool			HasFixedSize() const	override
			{
				return false;
			}

			virtual	serial_size_t	GetSerialSize(bool exportSize) const	override
			{
				if (hasAssignedSource)
				{
					serial_size_t memberSize = (serial_size_t)(sizeof(T)*sourceCount);
					if (exportSize)
						memberSize += GetSerialSizeOfSize((serial_size_t)sourceCount);
					return memberSize;
				}
				return Super::GetSerialSize(exportSize);
			}
			virtual	bool		Serialize(IWriteStream&outStream, bool exportSize) const	override
			{
				if (hasAssignedSource)
				{
					if (exportSize && !outStream.WriteSize((serial_size_t)sourceCount))
						return false;
					return outStream.WritePrimitives(sourceField,sourceCount);
				}
				return Super::Serialize(outStream,exportSize);
			}
			virtual	bool		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override
			{
				hasAssignedSource = false;
				return Super::Deserialize(inStream,fixedSize);
			}
		};


	static const count_t	MaxTechnitesPerChunk=10000;
	typedef SmartPODReferenceField<Technites::Technite::TCompactState,MaxTechnitesPerChunk>	TechniteField;
	typedef SmartPODReferenceField<Technites::Technite::TCompactInstruction,MaxTechnitesPerChunk>	TechniteInstructionField;


	namespace TechniteChunkFlags
	{
		enum flag_t
		{
			IsFirst = 0x1,
			IsLast = 0x2
		};
	};

	NETSTRUCT2(TechniteChunk
		, NET(UINT8), flags
		, TechniteField, technites
		);

	NETSTRUCT2(TechniteInstructions
		, NET(UINT32), offset
		, TechniteInstructionField, instructions
	);

	typedef NetArray<TVec3<BYTE>,0xFF>	TechniteColors;
	static_assert(sizeof(TVec3<BYTE>)==3,"Size expectations not met");

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
		, LCellIdI, fromLocation
		, LCellIdI, toLocation
		, NET(float), timeNeeded
	)

	NETSTRUCT7(AttackTrajectory
		, Timestamp, timestamp
		, NET(BYTE), trajectoryType
		, NET(float), flightTime
		, NET(TVec3<>), sourcePosition
		, NET(ConstName), turretType
		, NET(float), turretRange
		, LCellIdI, traceTo
	)

	NETSTRUCT4(EntityTurretRotation
		, Timestamp, timestamp
		, NET(float), rotationTime
		, NET(TUniqueID), id
		, NET(float3), toWorld
	)

	NETSTRUCT4(Explosion
		, Timestamp, timestamp
		, NET(TVec3<>), center
		, NET(float), radius
		, NET(bool), isEMP
	)

	NETSTRUCT3(DestroyEntity
		, Timestamp, timestamp
		, NET(TUniqueID), id
		, NET(UINT8), type
	)



	NETSTRUCT2(AttackTask
		, LCellIdI, target
		, NET(TUniqueID), id
	)


	//NETSTRUCT2(ProductionTask
	//	, NETA(Package::NetString<64>), parameters
	//	, NET(int), roundsToCompletion
	//)

	NETSTRUCT2(MineState
		, PositiveResourceVector, naturalResources
		, NET(bool), linkedToCore
	)
	typedef NET(int)	NexusPortState;
	NETSTRUCT2(TowerState
		, NETA(AttackTask), attackTasks
		, NET(float), attackActionLeft
	)

	typedef Selective4<MineState,ProducerState,NexusPortState,TowerState>
		SelectiveStructureState;




	NETSTRUCT4(StructureState
		, EntityState, entityState
		, PositiveResourceVector, resourceVector
		, NET(BYTE), flags
		, SelectiveStructureState, selective
	)



	NETSTRUCT4(NewMobileEntity
		, MobileState, state
		, NET(TUniqueID), owner
		, NET(ConstName), turretName
		, NET(ConstName), baseName
	)

	NETSTRUCT3(StructureConstruction
		, StructureState, state
		, NET(TUniqueID), owner
		, NET(ConstName), typeName
	)



	NETSTRUCT4(EntityDirectCommand
		, NET(TUniqueID), id
		, NET(UINT16),	mode
		, LCellIdI, target
		, NET(bool), leftClick
	)


	NETSTRUCT4(ChoiceIntersection
		, NET(TUniqueID), id
		, LCellIdI, target
		, NET(UINT16), mode
		, Optional<NET(TUniqueID)>, deployingWhat
	)
	
	NETSTRUCT3(StandardJob
		, NET(TUniqueID), id
		, NET(ConstName), jobName
		, NET(ConstName), parameterName
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

	typedef NetArray<ConstName,4>	ConstructionParameters;
	NETSTRUCT2(ConstructionBegin
		, NET(TUniqueID), plantID
		, ConstructionParameters, parameters
	)

	NETSTRUCT2(EnableStructure
		, NET(TUniqueID), id
		, NET(bool), enable
		)


	NETSTRUCT2(ControlMessage
		, Package::NetString<256>, message
		, NET(BYTE), type
	)



	NETSTRUCT4(PhaseChange
		, NET(UINT8), newPhase
		, NET(UINT32), roundNumber
		, NET(UINT32), techniteSubRoundNumber
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
			RegularFaction = 0x1,
			TechniteFaction = 0x2,	//can be combined
		};
	};


	NETSTRUCT3(ClientFactionResponse
		, NET(UINT32), startFactionTypes
		, NET(CryptographicID), factionToken
		, NET(Grid::TSharedPlanetConfig), planetConfig
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

	NETSTRUCT2(Automate
		, NET(TUniqueID), entityID
		, NET(bool), doAutomate
	)

	NETSTRUCT3(MarkerReferenceEntity
		, NET(TUniqueID), id
		, NET(ConstName), type0
		, NET(ConstName), type1
	)

	NETSTRUCT2(ControlMarker
		, NET(float), radius
		, NET(BYTE), subType
	)

	static_assert(Objects::Marker::Type::NumberOfPossibilities == 4, "Please adjust marker logic (MarkerData)");
	typedef Selective4<MarkerReferenceEntity, MarkerReferenceEntity, NoData, ControlMarker>	MarkerData;

	NETSTRUCT4(Marker
		, NET(TUniqueID), id
		, LCellIdI, location		
		, MarkerData, data
		, NET(UINT32), createdInRound
	)

	NETSTRUCT2(TouchedMarker
		, NET(TUniqueID), id
		, NET(UINT32), newRoundNumber
	)

	NETSTRUCT3(IdentifyUser
		, NET(TUniqueID), id
		, NET(BYTE), techniteGridFactionID
		, Package::NetString<32>, name
	)

	NETSTRUCT2(FactionID
		, NET(TUniqueID), id
		, NET(BYTE), techniteGridID
	)



	typedef std::pair<UINT16,UINT16>	TUnitLimit;

	typedef NETA(TUnitLimit)	UnitLimits;



	namespace ChannelID
	{
		//c2s: client to server
		//s2c: server to client

		enum id_t
		{
			Unused,

			Ping,				//c2s: <signal> sent once every two seconds, connection terminated after 10 seconds of inactivity
			Pong,				//s2c: <signal> sent as a response to Ping

			ControlMessage,		//s2c: ControlMessage
			GameProgress,		//s2c: GameProgress

			PhaseChange,		//s2c: PhaseChange

			SetClientState,		//c2s: ClientState -> s2c: Result
			ClientFaction,		//c2s: ClientFactionRequest -> s2c: ClientFactionResponse
			SetFactionID,		//s2c: FactionID

			WorldDelta,			//s2c : WorldDelta

			Explosion,			//s2c : Explosion

			NewMobileEntity,	//s2c: NewMobileEntity
			MobileState,		//s2c: MobileState
			MobileEntityMotion,		//s2c: MobileEntityMotion
			AttackTrajectory,		//s2c: AttackTrajectory
			EntityTurretRotation,	//s2c: EntityTurretRotation

			ChangeEntityOwner,	//s2c: ChangeEntityOwner

			RemoveEntity,		//c2s: TUniqueID
			RestoreCoreLink,	//c2s: TUniqueID

			InstantiateStructure,	//s2c: StructureConstruction
			UpdateStructure,		//s2c: StructureState

			DestroyEntity,		//s2c: DestroyEntity

			ChoiceIntersection,	//c2s: ChoiceIntersection
			RequestRoundEnd,	//c2s: <signal>

			EntityDirectCommand,//c2s: EntityDirectCommand

			DropLastTask,		//c2s: TUniqueID
			DropAllTasks,		//c2s: TUniqueID

			SignalNewJob,		//c2s: StandardJob
			SignalTerrainMod,	//c2s: TerrainMod

			ConstructionBegin,	//c2s: ConstructionBegin
			ConstructionAbort,	//c2s: ConstructionAbort

			EnableStructure,	//c2s: EnableStructure

			Automate,			//c2s: Automate

			ResourceUpdate,		//s2c: ResourceUpdate

			CreateMarker,		//c2s/s2c: Marker (non-dialogue)
			MarkerCreationFailedLimitReached,	//s2c: ClientMarker
			DestroyMarker,		//c2s/s2c: TUniqueID
			TouchMarker,		//c2s: TouchedMarker

			IdentifyUser,		// [c2s: TUniqueID -> ] s2c: IdentifyUser
			ChangeMyName,		// c2s: String
			FactionOnline,		// s2c: TUniqueID
			FactionOffline,		// s2c: TUniqueID
			FactionRequestedNewRound,	// s2c: TUniqueID

			LockEMPZones,		// s2c : UINT16
			UplinkBandwidth,	// s2c : int, can be negative to indicate it's disabled
			UnitLimits,			// s2c : UnitLimits

			ChatMessage,		// c2s: NetString<256>


			TechniteChunk,				// s2c: TechniteChunk
			TechniteInstructions,	// c2s: TechniteInstructions
			TechniteColors,			//s2c: TechniteColors


			//remote control channels (these require user level 2+)
			RC_UpdateWorld,				//c2s: Grid::TExtendedPlanetConfig
			RC_Lockdown,				//c2s: bool -> s2c: bool
			RC_Kick,					//c2s: pair<TUniqueID,bool>	(kick / kick and ban)
			RC_KickAllNonPrivileged,	//c2s: <signal>
		};
	}





	struct THostBroadcastPacket
	{
		TUniqueID	hostID;
		char		version[64];
	};
	static_assert(sizeof(THostBroadcastPacket) == sizeof(TUniqueID) + 64, "Struct packing not appropriate for host broadcast packet");


	namespace GameFlag
	{
		enum flag_t
		{
			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,


			SupportRegularFactions = 0x10,
			SupportTechniteFactions = 0x20,
			SupportHybridFactions = 0x40,


		};


	}



	NETSTRUCT8(ServerInfo
		, NET(THostBroadcastPacket), hostInfo
		, NET(UINT16), numClients
		, NET(UINT16), numPlayers
		, NET(UINT32), roundNumber
		, NET(TUniqueID), worldID	//used to manage factionTokens
		, NET(BYTE), activePhase
		, NET(UINT32), gameFlags
		, NET(BYTE), gameGoal
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

	struct WorldDeltaType
	{
		enum delta_type_t
		{
			/** *ProgressiveInit*
			Works on the assumption that the receiving faction client starts from an empty terrain.
			Initiates the client state to the current world state where visible, and initial world state where not.
			This type should only be used on completely new factions.
			*/
			ProgressiveInit,	
			ProgressiveUpdate,	//!< Works on the assumption that the recepient has a previous state, which should also be updated if visible
			FactionCopy,		//!< Creates a copy of the faction state, assuming the recepient knows only an empty terrain. Does not update anything
			GlobalCopy,			//!< Creates a copy of the actual world state, assuming the recepient knows only an empty terrain. Does not update anything
			GlobalInitialCopy,	//!< Creates a copy of the initial world state, assuming the recepient knows only an empty terrain. Does not update anything
		};
	};

	/**
	Generates a world delta for storage/client notification
	//@param faction Reference to the respective client faction (must be set if deltaType is ProgressiveInit, ProgressiveUpdate, or FactionCopy)
	@param[out] delta Resulting delta
	@param familyID Index of the grid family that the delta should be computed of
	@param rightNow Indicates that the enclosed timestamp should be flagged as 'rightNow', as opposed to using the grid timestamp
	@param last Indicates that the result should include the 'last'-flag, which means no deltas will follow this one for the current sequence
	@param deltaType Type of delta to compute (see delta_type_t for details)
	@param includeStructCountDelta Indicates that the result should also contain a struct count delta to the known struct count
	*/
	void			GenerateWorldDelta(Grid::VisibilityState*vs, Grid::WorldData*ws, Protocol::WorldDelta&delta, BYTE familyID, bool rightNow, bool last, WorldDeltaType::delta_type_t deltaType, bool includeStructCountDelta=true);

}




#endif
