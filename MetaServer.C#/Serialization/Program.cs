using Protocol;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Serialization
{
	class Program
	{

		struct Struct1
		{
			public int intField;
		}
		struct Struct2
		{
			public byte initialFixed;
			public Struct1[] innerVariable;
			public double terminalFixed;
		}
		struct Struct3
		{
			public byte initialFixed;
			public Struct1[] innerVariable;
			public string terminalVariable;
		}

		class Class0
		{
			public string initialVariable;
			public Int64 terminalFixed;
		}
		class Class1
		{
			public Class0 nestedComplex;
			public string terminalVariable;
		}

		private static void TestActual<T>()
		{
			SerialInterface serial = SerialInterface.Build(typeof(T));
			//try
			{
				Random b = new Random();
				int seed = b.Next();
				Parallel.For(0, 1000, i =>
				{
					Random random = new Random(seed + i * 2901567);
					ByteBuffer buffer = new ByteBuffer(1024);
					T raw = (T)serial.CreateRandom(random);
					T deserialized;
					try
					{
						serial.Serialize(raw, buffer);
					}
					catch (Exception e)
					{
						Debug.Fail(e.ToString());
					}
					try
					{
						deserialized = (T)serial.Deserialize(buffer.GetArray(), buffer.Length);
						serial.AssertBitwiseEqual(raw, deserialized);
					}
					catch (Exception e)
					{
						Debug.Fail(e.ToString());
						//						throw;
					}
				}
				);
			}
			//catch (Exception e)
			//{
			//	Assert.Fail(e.ToString());
			//}
		}

		private static void TestType<T>()
		{
			TestActual<T>();
			TestActual<T[]>();
		}


		static void Main(string[] args)
		{
			TestType<int>();
			TestType<float>();
			TestType<string>();
			TestType<bool>();
			//			TestType<Struct0>();
			TestType<Struct1>();
			TestType<Struct2>();
			TestType<Struct3>();
			TestType<Class0>();
			TestType<Class1>();

		}
	}
}
