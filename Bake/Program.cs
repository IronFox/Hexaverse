using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Bake
{

	class CommonPath
	{
		public DirectoryInfo Folder { get; private set; }

		protected void Setup(string folderPath)
		{
			Folder = new DirectoryInfo(folderPath);
		}

		public static string MakePath(string root, IEnumerable<string> segments)
		{
			return Path.Combine(root, string.Join(Path.DirectorySeparatorChar.ToString(), segments));
		}
		public static string MakePath(DirectoryInfo root, IEnumerable<string> segments)
		{
			return MakePath(root.FullName, segments);
		}

		public static string[] GetSegments(string path)
		{
			return path.Trim().Split(new char[] { '/', '\\' });
		}
	}

	class SourcePath : CommonPath
	{
		public readonly string Filter;
		public SourcePath(string filePath)
		{
			var segments = GetSegments(filePath);
			Setup(MakePath(".", segments.Take(segments.Length - 1)));

			if (!Folder.Exists)
				throw new IOException("Specified input folder '"+Folder.FullName+"' does not exist");
			Filter = segments[segments.Length - 1];
		}

	}

	class DestPath : CommonPath
	{
		public DestPath(string filePath)
		{
			var segments = GetSegments(filePath);
			Setup(MakePath(Program.globalDest, segments.Take(segments.Length - 1)));
			if (!Folder.Exists)
			{
				Setup(Directory.CreateDirectory(Folder.FullName).FullName);
				if (!Folder.Exists)
					throw new IOException("Unable to create output folder '" + Folder.FullName + "'");
			}
		}


	}

	class Program
	{
		public static DirectoryInfo globalDest;

		static int Main(string[] args)
		{
			try
			{
				if (args.Length != 2)
					throw new ArgumentException("Usage: Bake.exe <recipe file[.txt]> <oven directory>");
				string at = Directory.GetCurrentDirectory();
				Console.WriteLine("Reading recipe...");

				string recipeAt = args[0];
				if (!File.Exists(recipeAt))
				{
					if (!recipeAt.EndsWith(".txt"))
					{
						recipeAt += ".txt";
						if (!File.Exists(recipeAt))
							throw new FileNotFoundException("Input recipe does not exist", recipeAt);
					}
					else
						throw new FileNotFoundException("Input recipe does not exist", recipeAt);
				}

				string oven = args[1];
				if (Directory.Exists(oven))
				{
					Console.WriteLine("Clearing previous oven at '" + oven + "'...");
					Directory.Delete(oven, true);
				}
				if (File.Exists(oven))
					throw new ArgumentException("File found at target oven location '"+oven+"'");


				string[] recipe = File.ReadAllLines(recipeAt);

				Console.WriteLine("Creating target '"+oven+"' directory...");
				globalDest = Directory.CreateDirectory(oven);
				Console.WriteLine("Processing recipe...");
				foreach (string line in recipe)
				{
					if (line.Trim().Length == 0)
						continue;

					var arrowSegments = line.Split(new string[] { "->", "=>" }, StringSplitOptions.RemoveEmptyEntries);
					if (arrowSegments.Length > 2)
						throw new ArgumentException("Declaration in '" + line + "' has more than one arrow");
					var sourcePath = new SourcePath(arrowSegments[0]);  //source
					var destPath = new DestPath(arrowSegments[arrowSegments.Length - 1]);
					bool useArrow = arrowSegments.Length > 1;


					FileInfo[] files = sourcePath.Folder.GetFiles(sourcePath.Filter);
					if (files.Length == 0)
						throw new FileNotFoundException("Zero files to match recipe element '" + line + "'");
					foreach (FileInfo f in files)
					{
						string target = Path.Combine(destPath.Folder.FullName, f.Name);
						File.Copy(f.FullName, target);
						Console.WriteLine(target);
					}
				}
				Console.WriteLine("Done.");
			}
			catch (Exception e)
			{
				Console.Error.WriteLine(e);
				return -1;
			}
			return 0;
		}
	}
}
