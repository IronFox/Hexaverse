using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;

namespace Consensus
{
    public class DuplicateKeyException : SystemException
    {
        public DuplicateKeyException(string message)
            : base(message)
        { }
    }

    public class SkippingElementException : SystemException
    { }

    public struct ID
    {
        private UInt32 _value;
        public UInt32 Value { get{return _value;}}
        public bool IsValid { get { return _value != 0; } }
        public readonly static ID Invalid = new ID(0);
        public readonly static ID PrimaryPlatformID = new ID(1);


        private static Dictionary<UInt32, Object> _map = new Dictionary<UInt32, Object>();
        private static UInt32 _counter = 2;

        public static void ResetCounter()
        {
            lock (_map)
                _counter = 2;
        }

        private ID(UInt32 value)
        {
            _value = value;
        }

        public override int GetHashCode()
        {
            return Value.GetHashCode();
        }

        public override bool Equals(Object obj)
        {
            return obj is ID && ((ID)obj).Value == Value;
        }

        public static bool operator ==(ID a, ID b)
        {
            return a.Value == b.Value;
        }
        public static bool operator !=(ID a, ID b)
        {
            return a.Value != b.Value;
        }

        public static bool operator ==(UInt32 a, ID b)
        {
            return a == b.Value;
        }
        public static bool operator !=(UInt32 a, ID b)
        {
            return a != b.Value;
        }
        public static bool operator ==(ID a, UInt32 b)
        {
            return a.Value == b;
        }
        public static bool operator !=(ID a, UInt32 b)
        {
            return a.Value != b;
        }

        public static ID RegisterNew(Object obj)
        {
            if (LoadingStage.IsLoading)
            {
                Debug.Fail("Consistency violation: Attempting to create new ID while loading data");
                throw new Exception("Consistency violation: Attempting to create new ID while loading data");
            }
            lock (_map)
            {
                while (_counter != Invalid.Value && _counter != PrimaryPlatformID.Value && _map.ContainsKey(_counter))
                    _counter++;
                ID result = new ID(_counter);
                _map[_counter++] = obj;
                return result;
            }
        }

        public static ID RegisterExisting(UInt32 idValue, Object obj)
        {
            lock (_map)
            {
                if (_map.ContainsKey(idValue))
                    throw new DuplicateKeyException("ID " + idValue + " already in use");

                _map[idValue] = obj;
                _counter = Math.Max(_counter, idValue + 1);
                return new ID(idValue);
            }
        }

        public static Object Find(UInt32 idValue)
        {
            lock (_map)
            {
                Object result;
                if (!_map.TryGetValue(idValue, out result))
                    return null;
                return result;
            }
        }

        public static void Remove(ID id)
        {
            lock (_map)
                _map.Remove(id.Value);
        }


        public static int Registered { get { return _map.Count; } }

        public static string ListObjects()
        {
            StringBuilder builder = new StringBuilder();
            lock (_map)
            {
                foreach (var pair in _map)
                {
                    builder.Append(" (").Append(pair.Key).Append(")-").Append(pair.Value.ToString());
                }
            }
            return builder.ToString();
        }
        public override string ToString()
        {
            return "ID("+Value.ToString()+")";
        }

        public static implicit operator string(ID id)
        {
            return id.Value.ToString();
        }


    }



    public class Entity
    {
        public ID ID { get; protected set; }
        protected Entity()
        {}
        public void RegisterNewID()
        {
            Debug.Assert(!ID.IsValid);
            ID = ID.RegisterNew(this);
        }

        public void RegisterExistingID(UInt32 idValue)
        {
            Debug.Assert(!ID.IsValid);
            ID = ID.RegisterExisting(idValue, this);
        }

        public void DestroyID()
        {
            ID.Remove(ID);
            ID = ID.Invalid;
        }
    }




    public class IDMap<T>
        where T : Entity
    {
        protected List<T> all = new List<T>();
        protected Dictionary<UInt32, T> map = new Dictionary<UInt32, T>();
        protected ReaderWriterLock accessLock = new ReaderWriterLock();

        public void VisitElements(Action<T> action)
        {
            accessLock.AcquireReaderLock(1000);
            foreach (T element in all)
            {
                try
                {
                    action(element);
                }
                catch (SkippingElementException)
                { }
                catch (Exception ex)
                {
                    Program.ReportUnexpected(null, ex);
                }
            }
            accessLock.ReleaseReaderLock();
        }

        public T[] ToArray()
        {
            accessLock.AcquireReaderLock(1000);
                T[] result = all.ToArray();
            accessLock.ReleaseReaderLock();
            return result;
        }

        public T Lookup(ID id)
        {
            return Lookup(id.Value);
        }
        public T Lookup(UInt32 id)
        {
            accessLock.AcquireReaderLock(1000);
                T result;
                bool success = map.TryGetValue(id, out result);
            accessLock.ReleaseReaderLock();
            if (success)
                return result;
            return null;
        }


        public virtual bool InsertNew(T element)
        {
            accessLock.AcquireWriterLock(1000);
                element.RegisterNewID();
                all.Add(element);
                map[element.ID.Value] = element;
            accessLock.ReleaseWriterLock();
            return true;
        }
        public virtual void InsertExisting(UInt32 idValue, T element)
        {
            accessLock.AcquireWriterLock(1000);
                if (map.ContainsKey(idValue))
                {
                    accessLock.ReleaseWriterLock();
                    throw new DuplicateKeyException("ID " + idValue + " already in use");
                }
                try
                {
                    element.RegisterExistingID(idValue);
                }
                catch
                {
                    accessLock.ReleaseWriterLock();
                    throw;
                }
                Debug.Assert(element.ID.Value == idValue);
                map[element.ID.Value] = element;
                all.Add(element);
            accessLock.ReleaseWriterLock();
        }

        public virtual void Remove(UInt32 idValue)
        {
            accessLock.AcquireWriterLock(1000);
                T element;
                if (map.TryGetValue(idValue, out element))
                {
                    Debug.Assert(element.ID.Value == idValue);
                    element.DestroyID();
                    map.Remove(idValue);
                    all.Remove(element);
                }
            accessLock.ReleaseWriterLock();
        }
        public virtual void Remove(T element)
        {
            accessLock.AcquireWriterLock(1000);
                map.Remove(element.ID.Value);
                all.Remove(element);
                element.DestroyID();
            accessLock.ReleaseWriterLock();
        }


        public int Count { get { return all.Count; } }

        public virtual void Clear()
        {
            accessLock.AcquireWriterLock(1000);
                try
                {
                    foreach (T element in all)
                        ID.Remove(element.ID);
                    all.Clear();
                    map.Clear();
                }
                catch
                {
                    accessLock.ReleaseWriterLock();
                    throw;
                }
            accessLock.ReleaseWriterLock();
        }
    }
















    public class NamedEntity : Entity
    {
        public string Name { get; protected set; }

        protected NamedEntity()
        {}

    }


    public class IDNameMap<T> : IDMap<T>
        where T: NamedEntity
    {
        protected Dictionary<string, UInt32> nameMap = new Dictionary<string, UInt32>();

        
      
        private static string preProcessName(string name)
        {
            return name.ToLowerInvariant();
        }

        public override bool InsertNew(T element)
        {
            accessLock.AcquireWriterLock(1000);
                String name = preProcessName(element.Name);
                if (nameMap.ContainsKey(name))
                {
                    accessLock.ReleaseWriterLock();
                    return false;
                }
                element.RegisterNewID();
                all.Add(element);
                map[element.ID.Value] = element;
                nameMap[name] = element.ID.Value;
            accessLock.ReleaseWriterLock();
            return true;
        }
        public override void InsertExisting(UInt32 idValue, T element)
        {
            accessLock.AcquireWriterLock(1000);
                if (map.ContainsKey(idValue))
                {
                    accessLock.ReleaseWriterLock();
                    throw new DuplicateKeyException("ID " + idValue + " already in use");
                }
                String name = preProcessName(element.Name);
                if (nameMap.ContainsKey(name))
                {
                    accessLock.ReleaseWriterLock();
                    throw new DuplicateKeyException("Name '" + element.Name + " already in use in this context");
                }
                try
                {
                    element.RegisterExistingID(idValue);
                }
                catch
                {
                    accessLock.ReleaseWriterLock();
                    throw;
                }
                Debug.Assert(element.ID.Value == idValue);
                map[element.ID.Value] = element;
                nameMap[name] = element.ID.Value;
                all.Add(element);
            accessLock.ReleaseWriterLock();
        }

        public override void Remove(UInt32 idValue)
        {
            accessLock.AcquireWriterLock(1000);
                T element;
                if (map.TryGetValue(idValue, out element))
                {
                    Debug.Assert(element.ID.Value == idValue);
                    element.DestroyID();
                    map.Remove(idValue);
                    all.Remove(element);
                    String name = preProcessName(element.Name);
                    nameMap.Remove(name);
                }
            accessLock.ReleaseWriterLock();
        }
        public override void Remove(T element)
        {
            accessLock.AcquireWriterLock(1000);
                if (map.Remove(element.ID.Value))
                {
                    all.Remove(element);
                    String name = preProcessName(element.Name);
                    nameMap.Remove(name);
                    element.DestroyID();
                }
            accessLock.ReleaseWriterLock();
        }

        public T FindByName(string name)
        {
            accessLock.AcquireReaderLock(1000);
            try
            {
                T result = map[nameMap[preProcessName(name)]];
                accessLock.ReleaseReaderLock();
                return result;
            }
            catch (KeyNotFoundException)
            {
                accessLock.ReleaseReaderLock();
                return null;
            }
            catch
            {
                accessLock.ReleaseReaderLock();
                throw;
            }
        }


        public override void Clear()
        {
            accessLock.AcquireWriterLock(1000);
            try
            {
                foreach (T element in all)
                    ID.Remove(element.ID);
                all.Clear();
                map.Clear();
                nameMap.Clear();
            }
            catch
            {
                accessLock.ReleaseWriterLock();
                throw;
            }
            accessLock.ReleaseWriterLock();
        }
    }

}
