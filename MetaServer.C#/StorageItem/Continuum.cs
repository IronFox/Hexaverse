using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using Consensus;
using System.Threading;

namespace Consensus
{
    public static class Continuum
    {

        private static Mutex _lock = new Mutex();
        //private static object _lock = new object();

        public static void Load(string filename)
        {
            try
            {

                LoadingStage.Enter();
                //lock (this)
                {
                    XElement el = XElement.Load(filename);
                    var elements = el.Elements();
                    foreach (var child in elements)
                    {
                        try
                        {
                            if (child.Name == "account")
                                Account.Load(child);
                            else
								throw new Exception("Unexpected entity type encountered: " + child.Name);
                        }
                        catch (Exception e)
                        {
                            Program.ReportUnexpected(null,e,Significance.Unusual);
                        }
                    }
                }

                LoadingStage.Exit();



                Account.Collection.VisitAllElements(account => account.PostLoadLink());

                //clean up oddities from last run:
            }
            catch
            {
                LoadingStage.Abort();
                try
                {
                    Account.Collection.Clear();
                }
                catch (Exception ex)
                {
                    Program.ReportFatal(null, ex);
                    throw;
                }
                throw;
            }
        }

        public static bool Load()
        {
            try
            {
                Load("continuum.xml");
            }
            catch (System.IO.FileNotFoundException)
            {
                Console.Out.WriteLine("Warning: World not found. Continuing with empty data set.");
                //let's ignore this one
            }
            catch (Exception ex)
            {
                Program.ReportFatal(null, ex);
                return false;
            }

            Program.Log(null, Significance.Common, Account.Collection.Count + " Account(s) loaded.");

            return !Program.ShuttingDown;   //fatal reported otherwise
        }


        private static void InnerSave()
        {
            XElement el = new XElement("continuum");
            Account.Collection.VisitAllElements(account => el.Add(account.ToXML()));
            el.Save("continuum.xml");
        }

        public static bool TrySave(int timeout = 0)
        {
            if (!_lock.WaitOne(timeout))
                return false;
            try
            {
                InnerSave();
                _lock.ReleaseMutex();
            }
            catch
            {
                _lock.ReleaseMutex();
                throw;
            }
            return true;
        }

        public static void Save()
        {
            if (!TrySave(5000))//5 seconds should be sufficient
                throw new Exception("Failed to acquire lock for world saving. Deadlock assumed");
        }

    }
}
