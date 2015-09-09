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
    public static class World
    {

        private static Mutex _lock = new Mutex();
        //private static object _lock = new object();

        public static void Load(string filename)
        {
            if (ID.Registered != 0)
            {
                Exception ex = new Exception("Invalid program state prior to loading. "+ID.Registered+" object(s) remain registered: "+ID.ListObjects());
                Program.ReportFatal(null,ex);
                throw ex;
            }

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
                            if (child.Name == "structure")
                                Structure.Load(child);
                            else
                                if (child.Name == "account")
                                    Account.Load(child);
                                else
                                    if (child.Name == "repository")
                                        ViewServer.LoadRepository(child);
                                    else
                                        if (child.Name == "avatar")
                                            AvatarInstance.Load(child);
                                        else
                                            if (child.Name == "file")
                                                FileObject.Load(child);
                                            else
                                                throw new Exception("Unexpected entity type encountered: " + child.Name);
                        }
                        catch (Exception e)
                        {
                            Program.ReportUnexpected(null,e,Significance.Unusual);
                        }
                    }
                }

                PrimaryPlatform primary = PrimaryPlatform.Singleton;
                Structure.Collection.InsertExisting(ID.PrimaryPlatformID.Value, primary);
                primary.UpdateDescription();    //new ID
                Structure.BVH.Insert(primary, primary.BoundingVolume);

                LoadingStage.Exit();



                Structure.Collection.VisitElements(platform => platform.PostLoadLink());
                AvatarInstance.Collection.VisitElements(avatar => avatar.PostLoadLink());
                Account.Collection.VisitElements(account => account.PostLoadLink());

                ViewServer.FinishLoading();

                //clean up oddities from last run:
                List<FileObject> unreferenced = new List<FileObject>();
                FileObject.Collection.VisitElements(file => { if (file.ReferenceCount == 0 && file.IsUserFile) unreferenced.Add(file); });
                foreach (FileObject file in unreferenced)
                {
                    Program.Log(null, Significance.Unusual, "Diposing of unreferenced file " + file);
                    try
                    {
                        file.Dispose();
                    }
                    catch (Exception ex)
                    {
                        Program.ReportUnexpected(null, ex, Significance.Unusual);
                    }
                }
            }
            catch
            {
                LoadingStage.Abort();
                try
                {
                    PrimaryPlatform.DestroySingleton();
                    Structure.Collection.VisitElements(structure => structure.Destroy());
                    Structure.Collection.Clear();
                    Structure.RealtimeStructures.Clear();
                    Structure.BVH.Clear();
                    ViewServer.ClearData();
                    Account.Collection.Clear();
                    AvatarInstance.Collection.Clear();
                    FileObject.Collection.Clear();
                    if (ID.Registered != 0)
                        throw new Exception("Invalid program state after clearing. " + ID.Registered + " object(s) remain registered: " + ID.ListObjects());
                    ID.ResetCounter();
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
                Load("world.xml");

                PrimaryPlatform.Singleton.Icon = FileObject.ReferenceLocalImage("images/up.png", "primary_icon");

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
            Account.Collection.VisitElements(acc => 
                {
                    Program.Log(null, Significance.Common, "  " + acc);
                    foreach (var av in acc.Avatars)
                        Program.Log(null, Significance.Common, "    " + av.ID);
                });
            Program.Log(null, Significance.Common, Structure.Collection.Count + " Structure(s) loaded.");
            if (Structure.Collection.Count < 20)
                Structure.Collection.VisitElements(structure => Program.Log(null, Significance.Common, "  " + structure));
            Program.Log(null, Significance.Common, FileObject.Collection.Count + " File(s) loaded.");
            FileObject.Collection.VisitElements(file => Program.Log(null, Significance.Common, "  " + file));

            return !Program.ShuttingDown;   //fatal reported otherwise
        }


        private static void InnerSave()
        {
            XElement el = new XElement("world");
            Structure.Collection.VisitElements(structure => { if (structure.ExportToFile) el.Add(structure.ToXML()); });    //what do you know, exceptions are expensive if they come in the thousands...
            AvatarInstance.Collection.VisitElements(avatar => el.Add(avatar.ToXML()));
            Account.Collection.VisitElements(account => el.Add(account.ToXML()));
            FileObject.Collection.VisitElements(file => el.Add(file.ToXML()));
            ViewServer.VisitProducts(branch => el.Add(branch.ToXML()));
            el.Save("world.xml");
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
