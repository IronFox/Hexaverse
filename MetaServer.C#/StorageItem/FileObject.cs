using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Xml.Linq;

namespace Consensus
{
    public static class MIME
    {
        /*
         * source: 
         * http://stackoverflow.com/questions/58510/using-net-how-can-you-find-the-mime-type-of-a-file-based-on-the-file-signature
         * 
         */
        [DllImport(@"urlmon.dll", CharSet = CharSet.Auto)]
        private extern static System.UInt32 FindMimeFromData(
            System.UInt32 pBC,
            [MarshalAs(UnmanagedType.LPStr)] System.String pwzUrl,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pBuffer,
            System.UInt32 cbSize,
            [MarshalAs(UnmanagedType.LPStr)] System.String pwzMimeProposed,
            System.UInt32 dwMimeFlags,
            out System.UInt32 ppwzMimeOut,
            System.UInt32 dwReserverd
        );


        private static readonly Dictionary<string, string> MIMETypesDictionary = new Dictionary<string, string>
          {
            {"ai", "application/postscript"},
            {"aif", "audio/x-aiff"},
            {"aifc", "audio/x-aiff"},
            {"aiff", "audio/x-aiff"},
            {"asc", "text/plain"},
            {"atom", "application/atom+xml"},
            {"au", "audio/basic"},
            {"avi", "video/x-msvideo"},
            {"bcpio", "application/x-bcpio"},
            {"bin", "application/octet-stream"},
            {"bmp", "image/bmp"},
            {"cdf", "application/x-netcdf"},
            {"cgm", "image/cgm"},
            {"class", "application/octet-stream"},
            {"cpio", "application/x-cpio"},
            {"cpt", "application/mac-compactpro"},
            {"csh", "application/x-csh"},
            {"css", "text/css"},
            {"dcr", "application/x-director"},
            {"dif", "video/x-dv"},
            {"dir", "application/x-director"},
            {"djv", "image/vnd.djvu"},
            {"djvu", "image/vnd.djvu"},
            {"dll", "application/octet-stream"},
            {"dmg", "application/octet-stream"},
            {"dms", "application/octet-stream"},
            {"doc", "application/msword"},
            {"docx","application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
            {"dotx", "application/vnd.openxmlformats-officedocument.wordprocessingml.template"},
            {"docm","application/vnd.ms-word.document.macroEnabled.12"},
            {"dotm","application/vnd.ms-word.template.macroEnabled.12"},
            {"dtd", "application/xml-dtd"},
            {"dv", "video/x-dv"},
            {"dvi", "application/x-dvi"},
            {"dxr", "application/x-director"},
            {"eps", "application/postscript"},
            {"etx", "text/x-setext"},
            {"exe", "application/octet-stream"},
            {"ez", "application/andrew-inset"},
            {"gif", "image/gif"},
            {"gram", "application/srgs"},
            {"grxml", "application/srgs+xml"},
            {"gtar", "application/x-gtar"},
            {"hdf", "application/x-hdf"},
            {"hqx", "application/mac-binhex40"},
            {"htm", "text/html"},
            {"html", "text/html"},
            {"ice", "x-conference/x-cooltalk"},
            {"ico", "image/x-icon"},
            {"ics", "text/calendar"},
            {"ief", "image/ief"},
            {"ifb", "text/calendar"},
            {"iges", "model/iges"},
            {"igs", "model/iges"},
            {"jnlp", "application/x-java-jnlp-file"},
            {"jp2", "image/jp2"},
            {"jpe", "image/jpeg"},
            {"jpeg", "image/jpeg"},
            {"jpg", "image/jpeg"},
            {"js", "application/x-javascript"},
            {"kar", "audio/midi"},
            {"latex", "application/x-latex"},
            {"lha", "application/octet-stream"},
            {"lzh", "application/octet-stream"},
            {"m3u", "audio/x-mpegurl"},
            {"m4a", "audio/mp4a-latm"},
            {"m4b", "audio/mp4a-latm"},
            {"m4p", "audio/mp4a-latm"},
            {"m4u", "video/vnd.mpegurl"},
            {"m4v", "video/x-m4v"},
            {"mac", "image/x-macpaint"},
            {"man", "application/x-troff-man"},
            {"mathml", "application/mathml+xml"},
            {"me", "application/x-troff-me"},
            {"mesh", "model/mesh"},
            {"mid", "audio/midi"},
            {"midi", "audio/midi"},
            {"mif", "application/vnd.mif"},
            {"mov", "video/quicktime"},
            {"movie", "video/x-sgi-movie"},
            {"mp2", "audio/mpeg"},
            {"mp3", "audio/mpeg"},
            {"mp4", "video/mp4"},
            {"mpe", "video/mpeg"},
            {"mpeg", "video/mpeg"},
            {"mpg", "video/mpeg"},
            {"mpga", "audio/mpeg"},
            {"ms", "application/x-troff-ms"},
            {"msh", "model/mesh"},
            {"mxu", "video/vnd.mpegurl"},
            {"nc", "application/x-netcdf"},
            {"oda", "application/oda"},
            {"ogg", "application/ogg"},
            {"pbm", "image/x-portable-bitmap"},
            {"pct", "image/pict"},
            {"pdb", "chemical/x-pdb"},
            {"pdf", "application/pdf"},
            {"pgm", "image/x-portable-graymap"},
            {"pgn", "application/x-chess-pgn"},
            {"pic", "image/pict"},
            {"pict", "image/pict"},
            {"png", "image/png"}, 
            {"pnm", "image/x-portable-anymap"},
            {"pnt", "image/x-macpaint"},
            {"pntg", "image/x-macpaint"},
            {"ppm", "image/x-portable-pixmap"},
            {"ppt", "application/vnd.ms-powerpoint"},
            {"pptx","application/vnd.openxmlformats-officedocument.presentationml.presentation"},
            {"potx","application/vnd.openxmlformats-officedocument.presentationml.template"},
            {"ppsx","application/vnd.openxmlformats-officedocument.presentationml.slideshow"},
            {"ppam","application/vnd.ms-powerpoint.addin.macroEnabled.12"},
            {"pptm","application/vnd.ms-powerpoint.presentation.macroEnabled.12"},
            {"potm","application/vnd.ms-powerpoint.template.macroEnabled.12"},
            {"ppsm","application/vnd.ms-powerpoint.slideshow.macroEnabled.12"},
            {"ps", "application/postscript"},
            {"qt", "video/quicktime"},
            {"qti", "image/x-quicktime"},
            {"qtif", "image/x-quicktime"},
            {"ra", "audio/x-pn-realaudio"},
            {"ram", "audio/x-pn-realaudio"},
            {"ras", "image/x-cmu-raster"},
            {"rdf", "application/rdf+xml"},
            {"rgb", "image/x-rgb"},
            {"rm", "application/vnd.rn-realmedia"},
            {"roff", "application/x-troff"},
            {"rtf", "text/rtf"},
            {"rtx", "text/richtext"},
            {"sgm", "text/sgml"},
            {"sgml", "text/sgml"},
            {"sh", "application/x-sh"},
            {"shar", "application/x-shar"},
            {"silo", "model/mesh"},
            {"sit", "application/x-stuffit"},
            {"skd", "application/x-koan"},
            {"skm", "application/x-koan"},
            {"skp", "application/x-koan"},
            {"skt", "application/x-koan"},
            {"smi", "application/smil"},
            {"smil", "application/smil"},
            {"snd", "audio/basic"},
            {"so", "application/octet-stream"},
            {"spl", "application/x-futuresplash"},
            {"src", "application/x-wais-source"},
            {"sv4cpio", "application/x-sv4cpio"},
            {"sv4crc", "application/x-sv4crc"},
            {"svg", "image/svg+xml"},
            {"swf", "application/x-shockwave-flash"},
            {"t", "application/x-troff"},
            {"tar", "application/x-tar"},
            {"tcl", "application/x-tcl"},
            {"tex", "application/x-tex"},
            {"texi", "application/x-texinfo"},
            {"texinfo", "application/x-texinfo"},
            {"tif", "image/tiff"},
            {"tiff", "image/tiff"},
            {"tr", "application/x-troff"},
            {"tsv", "text/tab-separated-values"},
            {"txt", "text/plain"},
            {"ustar", "application/x-ustar"},
            {"vcd", "application/x-cdlink"},
            {"vrml", "model/vrml"},
            {"vxml", "application/voicexml+xml"},
            {"wav", "audio/x-wav"},
            {"wbmp", "image/vnd.wap.wbmp"},
            {"wbmxl", "application/vnd.wap.wbxml"},
            {"wml", "text/vnd.wap.wml"},
            {"wmlc", "application/vnd.wap.wmlc"},
            {"wmls", "text/vnd.wap.wmlscript"},
            {"wmlsc", "application/vnd.wap.wmlscriptc"},
            {"wrl", "model/vrml"},
            {"xbm", "image/x-xbitmap"},
            {"xht", "application/xhtml+xml"},
            {"xhtml", "application/xhtml+xml"},
            {"xls", "application/vnd.ms-excel"},                        
            {"xml", "application/xml"},
            {"xpm", "image/x-xpixmap"},
            {"xsl", "application/xml"},
            {"xlsx","application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
            {"xltx","application/vnd.openxmlformats-officedocument.spreadsheetml.template"},
            {"xlsm","application/vnd.ms-excel.sheet.macroEnabled.12"},
            {"xltm","application/vnd.ms-excel.template.macroEnabled.12"},
            {"xlam","application/vnd.ms-excel.addin.macroEnabled.12"},
            {"xlsb","application/vnd.ms-excel.sheet.binary.macroEnabled.12"},
            {"xslt", "application/xslt+xml"},
            {"xul", "application/vnd.mozilla.xul+xml"},
            {"xwd", "image/x-xwindowdump"},
            {"xyz", "chemical/x-xyz"},
            {"zip", "application/zip"}
          };

        public static string GetFromFileName(string fileName)
        {
            if (MIMETypesDictionary.ContainsKey(Path.GetExtension(fileName).Remove(0, 1)))
            {
                return MIMETypesDictionary[Path.GetExtension(fileName).Remove(0, 1)];
            }
            return "unknown/unknown";
        }

        public static string GetFromFileData(byte[] data)
        {
            System.UInt32 mimetype;
            FindMimeFromData(0, null, data, (uint)data.Length, null, 0, out mimetype, 0);
            System.IntPtr mimeTypePtr = new IntPtr(mimetype);
            string mime = Marshal.PtrToStringUni(mimeTypePtr);
            Marshal.FreeCoTaskMem(mimeTypePtr);
            return mime;
        }


        public static string GetFromFile(string filename, byte[] data)
        {
            try
            {
                string result = GetFromFileData(data);
                if (result != "unknown/unknown" && result != "application/octet-stream")
                    return result;
            }
            catch
            { }
            return GetFromFileName(filename);
        }
    }



    public class FileObject : NamedEntity, IDisposable
    {
        public const int ChunkSize = 0x4000; //16kb

        public static IDNameMap<FileObject> Collection = new IDNameMap<FileObject>();

        private int _referenceCount = 0;
        public int ReferenceCount { get { return _referenceCount; } }

        public string FileName { get; private set; }
        public UInt32 FileSize { get; private set; }
        public UInt32 CRC32 { get; private set; }
        public UInt32 NumChunks
        {
            get
            {
                UInt32 chunks = FileSize / ChunkSize;
                if ((FileSize % ChunkSize) != 0)
                    chunks++;
                return chunks;
            }
        }
        public string MIMEType { get; private set; }

        public readonly bool IsUserFile;
        

        private FileObject(string name, string fileName, bool isUserFile, byte[] fileData)
        {
            Name = name;
            MIMEType = MIME.GetFromFile(fileName, fileData);
            FileName = fileName;
            IsUserFile = isUserFile;
            FileSize = (UInt32)fileData.Length;
            CRC32 = IO.CRC32.Get(fileData);
            _data = fileData;
            _chunks = new byte[NumChunks][];
        }

        private FileObject(string name, string fileName, string mimeType, UInt32 crc32, bool isUserFile)
        {
            Name = name;
            MIMEType = mimeType;
            FileName = fileName;
            IsUserFile = isUserFile;
            FileSize = (UInt32)new FileInfo(fileName).Length;
            CRC32 = crc32;
        }


        public static void Load(XElement xfile)
        {
            UInt32 idValue = xfile.GetUInt32("id");
            FileObject obj = new FileObject(
                                xfile.GetString("name"),
                                xfile.GetString("filename"),
                                xfile.GetString("mime_type"),
                                xfile.GetUInt32("crc32"),
                                xfile.GetBool("is_user_file")
                                );
            obj.FileName = xfile.GetString("filename");
            Collection.InsertExisting(idValue,obj);
        }
        public XElement ToXML()
        {
            lock (this)
            {
                XElement xfile = new XElement("file");
                xfile.Set("id", ID);
                xfile.Set("name", Name);
                xfile.Set("filename", FileName);
                xfile.Set("mime_type", MIMEType);
                xfile.Set("crc32", CRC32);
                xfile.Set("is_user_file", IsUserFile);
                return xfile;
            }
        }


        public static FileObject ReferenceLocalFile(string filename, string name = null)
        {
            if (name == null)
            {
                name = Path.GetFileName(filename).ToLower();
                name = Regex.Replace(name, @"[^a-z0-9_.]", "");
            }
            name = "local/" + name;
            
            FileObject result = Collection.FindByName(name);
            if (result != null)
                return result;
            byte[] data = File.ReadAllBytes(filename);
            result = new FileObject(name ,filename,false, data);
            Collection.InsertNew(result);
            return result;
        }

        public static FileObject ReferenceLocalImage(string filename, string name=null)
        {
            return ReferenceLocalFile(filename, name);
        }


        public override string ToString()
        {
            return ("File{ID=" + ID + ",Name=" + Name + ",FileName="+FileName+",Type="+MIMEType+"}");
        }


        public void IncrementReferenceCount()
        {
            Interlocked.Increment(ref _referenceCount);
        }


        private byte[] _data = null;
        private byte[][] _chunks = null;

        private void DumpCachedData()
        {
            lock (this)
            {
                _data = null;
                _chunks = null;
            }
        }


        public void Dispose()
        {
            if (!IsUserFile)
                throw new Exception("Trying to dispose of non-user-file");
            ForceDispose();
        }
        
        /// <summary>
        /// Forces the disposal of the the local object, even if it is a reference file.
        /// </summary>
        public void ForceDispose()
        {
            Destroy();
            Collection.Remove(this);
        }

        public void Destroy()
        {
            lock (this)
            {
                if (ID.IsValid)
                {
                    DumpCachedData();
                    if (IsUserFile)
                        File.Delete(FileName);
                }
            }
        }

        public void DecrementReferenceCount()
        {
            int newValue = Interlocked.Decrement(ref _referenceCount);
            if (newValue == 0)
            {
                if (IsUserFile)
                    Dispose();
                else
                    DumpCachedData();
            }
        }

        public UInt32 SizeOfChunk(UInt32 chunkIndex)
        {
            UInt32 chunkOffset = ChunkSize * chunkIndex;
            if (chunkOffset + ChunkSize <= FileSize)
                return ChunkSize;
            Debug.Assert(chunkOffset < FileSize);
            return FileSize - chunkOffset;
        }

        public byte[] GetChunk(UInt32 chunkIndex)
        {
            lock(this)
            {
                if (_data == null)
                {
                    _data = File.ReadAllBytes(FileName);
                    Debug.Assert((UInt32)_data.Length == FileSize);
                    _chunks = new byte[NumChunks][];
                }

                UInt32 chunkSize = SizeOfChunk(chunkIndex);
                byte[] chunk = _chunks[chunkIndex];
                if (chunk == null)
                {
                    chunk = _chunks[chunkIndex] = new byte[chunkSize];
                    Array.Copy(_data, chunkIndex * ChunkSize, chunk, 0, chunkSize);
                }
                return chunk;
            }
        }
    }
}
