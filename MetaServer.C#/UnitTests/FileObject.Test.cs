using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.IO;
using Prisma;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;


namespace UnitTests
{
    [TestClass]
    public class FileObject
    {
        [TestMethod]
        public void TestRandomFileObject()
        {
            string filename = Path.GetTempFileName();




            Random rand = new Random();
            byte[] randomData = new byte[rand.Next(Prisma.FileObject.ChunkSize, Prisma.FileObject.ChunkSize * 3)];
            rand.NextBytes(randomData);
            File.WriteAllBytes(filename, randomData);


            Prisma.FileObject file = Prisma.FileObject.ReferenceLocalFile(filename);
            Assert.AreEqual(filename, file.FileName);
            Debug.WriteLine("Detected garbage file type: " + file.MIMEType);

            Assert.AreEqual(file.FileSize, (uint)randomData.Length);

            Prisma.ID objectID = file.ID;

            int reading = 0;

            for (UInt32 chunkIndex = 0; chunkIndex < file.NumChunks; chunkIndex++)
            {
                byte[] chunk = file.GetChunk(chunkIndex);
                for (int i = 0; i < chunk.Length; i++)
                    Assert.AreEqual(chunk[i], randomData[reading++]);
            }
            Assert.AreEqual(reading, randomData.Length);

            file.ForceDispose();
            Assert.AreEqual(file.ID, Prisma.ID.Invalid);
            Assert.IsNull(Prisma.FileObject.Collection.Lookup(objectID));

            File.Delete(filename);

        }

        [TestMethod]
        public void TestPngFileObject()
        {
            string filename = Path.GetTempFileName();

            using (Bitmap b = new Bitmap(50, 50))
            {
                using (Graphics g = Graphics.FromImage(b))
                {
                    g.Clear(Color.Green);
                }
                b.Save(filename, ImageFormat.Png);
            }

            Prisma.FileObject file = Prisma.FileObject.ReferenceLocalFile(filename);
            Assert.AreEqual(filename, file.FileName);
            Assert.IsTrue(file.MIMEType == "image/png" || file.MIMEType == "image/x-png");

            Assert.AreEqual(file.FileSize, (uint)new FileInfo(filename).Length);

            Prisma.ID objectID = file.ID;

            file.ForceDispose();
            Assert.AreEqual(file.ID, Prisma.ID.Invalid);
            Assert.IsNull(Prisma.FileObject.Collection.Lookup(objectID));

            File.Delete(filename);

        }

        [TestMethod]
        public void TestJPEGFileObject()
        {
            string filename = Path.GetTempFileName();

            using (Bitmap b = new Bitmap(50, 50))
            {
                using (Graphics g = Graphics.FromImage(b))
                {
                    g.Clear(Color.Blue);
                }
                b.Save(filename, ImageFormat.Jpeg);
            }

            Prisma.FileObject file = Prisma.FileObject.ReferenceLocalFile(filename);
            Assert.AreEqual(filename, file.FileName);
            Assert.IsTrue(file.MIMEType == "image/jpeg" || file.MIMEType == "image/pjpeg");
            Assert.AreEqual(file.FileSize, (uint)new FileInfo(filename).Length);

            Prisma.ID objectID = file.ID;

            file.ForceDispose();
            Assert.AreEqual(file.ID, Prisma.ID.Invalid);
            Assert.IsNull(Prisma.FileObject.Collection.Lookup(objectID));

            File.Delete(filename);

        }
    }
}
