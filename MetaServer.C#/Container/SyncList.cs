using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace Consensus.StorageItem
{
    public class SyncList<T>
    {
        private List<T> all = new List<T>();
        private ReaderWriterLock accessLock = new ReaderWriterLock();


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

        public void Insert(T element)
        {
            accessLock.AcquireWriterLock(1000);
                all.Add(element);
            accessLock.ReleaseWriterLock();
        }


        public void Remove(T element)
        {
            accessLock.AcquireWriterLock(1000);
                all.Remove(element);
            accessLock.ReleaseWriterLock();
        }

        public int Count { get { return all.Count; } }

        public void Clear()
        {
            accessLock.AcquireWriterLock(1000);
                all.Clear();
            accessLock.ReleaseWriterLock();
        }
    }
}
