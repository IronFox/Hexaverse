using Math3D;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Consensus
{

	public static class Extensions
	{
		public static void VisitAllElements<K, V>(this ConcurrentDictionary<K, V> dict, Action<V> op)
		{
			foreach (var entry in dict)
				op(entry.Value);

		}

		public static V GetValue<K, V>(this ConcurrentDictionary<K, V> dict, K key) where V : class
		{
			V rs;
			if (dict.TryGetValue(key, out rs))
				return rs;
			return null;
		}


		public static bool Unset<K, V>(this ConcurrentDictionary<K, V> dict, K key)
		{
			V dummy;
			return dict.TryRemove(key, out dummy);
		}

		public static IEnumerable<K> GetKeys<K, V>(this ConcurrentDictionary<K, V> dict)
		{
			foreach (var pair in dict)
				yield return pair.Key;
		}
		public static IEnumerable<V> GetValues<K, V>(this ConcurrentDictionary<K, V> dict)
		{
			foreach (var pair in dict)
				yield return pair.Value;
		}

		public static float NextFloat(this Random random)
		{
			return (float)random.NextDouble();
		}

		public static float NextIn(this Random random, Range range)
		{
			return range.Min + range.Extend * random.NextFloat();
		}


		public static void SetUInt32(this byte[] array, int address, UInt32 v)
		{
			Debug.Assert(address + 4 <= array.Length);

			array[address] = (byte)(v & 0xFF);
			array[address + 1] = (byte)((v >> 8) & 0xFF);
			array[address + 2] = (byte)((v >> 16) & 0xFF);
			array[address + 3] = (byte)((v >> 24) & 0xFF);
		}
		public static UInt32 GetUInt32(this byte[] array, int address)
		{
			Debug.Assert(address + 4 <= array.Length);

			return (UInt32)array[address]
					|
					(((UInt32)array[address + 1]) << 8)
					|
					(((UInt32)array[address + 2]) << 16)
					|
					(((UInt32)array[address + 3]) << 24);
		}
		public static void Set(this XElement element, string key, bool v)
		{
			element.SetAttributeValue(key, v ? "true" : "false");
		}
		public static void Set(this XElement element, string key, string v)
		{
			element.SetAttributeValue(key, v);
		}
		public static void Set(this XElement element, string key, float v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}
		public static void Set(this XElement element, string key, Vec3 v)
		{
			element.SetAttributeValue(key, Convert.ToString(v.x) + "," + Convert.ToString(v.y) + "," + Convert.ToString(v.z));
		}
		public static void Set(this XElement element, string key, Vec4 v)
		{
			element.SetAttributeValue(key, Convert.ToString(v.x) + "," + Convert.ToString(v.y) + "," + Convert.ToString(v.z) + "," + Convert.ToString(v.w));
		}

		public static void Set(this XElement element, string key, int v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}
		public static void Set(this XElement element, string key, uint v)
		{
			element.SetAttributeValue(key, Convert.ToString(v));
		}

		public static float GetFloat(this XElement element, string key)
		{
			return Convert.ToFloat(element.Attribute(key).Value);
		}
		public static Vec3 GetVec3(this XElement element, string key)
		{
			string val = element.Attribute(key).Value;
			string[] elements = val.Split(new char[] { ',' });
			if (elements.Length != 3)
				throw new Exception("Expected 3 elements while parsing xml element '" + key + "' (value is '" + val + "'), but got " + elements.Length);
			return new Math3D.Vec3(Convert.ToFloat(elements[0].Trim()),
							Convert.ToFloat(elements[1].Trim()),
							Convert.ToFloat(elements[2].Trim()));
		}
		public static Vec4 GetVec4(this XElement element, string key)
		{
			string val = element.Attribute(key).Value;
			string[] elements = val.Split(new char[] { ',' });
			if (elements.Length != 4)
				throw new Exception("Expected 4 elements while parsing xml element '" + key + "' (value is '" + val + "'), but got " + elements.Length);
			return new Math3D.Vec4(Convert.ToFloat(elements[0].Trim()),
							Convert.ToFloat(elements[1].Trim()),
							Convert.ToFloat(elements[2].Trim()),
							Convert.ToFloat(elements[3].Trim())
							);
		}

		public static Int32 GetInt32(this XElement element, string key)
		{
			return Convert.ToInt32(element.Attribute(key).Value);
		}
		public static bool GetBool(this XElement element, string key)
		{
			string value = element.Attribute(key).Value.ToLower();
			if (value == "true")
				return true;
			if (value == "false")
				return false;
			throw new Exception("Failed to decode '" + value + "' to bool");
		}
		public static UInt32 GetUInt32(this XElement element, string key)
		{
			return Convert.ToUInt32(element.Attribute(key).Value);
		}
		public static bool TryGetUInt32(this XElement element, string key, out UInt32 value)
		{
			XAttribute attrib = element.Attribute(key);
			if (attrib == null)
			{
				value = 0;
				return false;
			}
			value = Convert.ToUInt32(attrib.Value);
			return true;
		}
		public static string GetString(this XElement element, string key)
		{
			return element.Attribute(key).Value;
		}
		public static string GetOptionalString(this XElement element, string key)
		{
			var attrib = element.Attribute(key);
			if (attrib == null)
				return "";
			return attrib.Value;
		}







		public static void OutLine(this StreamWriter writer, string line)
		{
			writer.Write(line + "\n");
		}

	}
}
