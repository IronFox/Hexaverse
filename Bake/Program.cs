using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Bake
{
	class Program
	{
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
				DirectoryInfo globalDest = Directory.CreateDirectory(oven);
				Console.WriteLine("Processing recipe...");
				foreach (string element in recipe)
				{
					if (element.Trim().Length == 0)
						continue;
					string[] segments = element.Split(new char[] { '/', '\\' });
					string[] dirSegments = segments.Take(segments.Length - 1).ToArray();
					DirectoryInfo dest = globalDest,
									source = new DirectoryInfo(Path.Combine(".", string.Join(Path.DirectorySeparatorChar.ToString(), dirSegments)));
					for (int i = 0; i < segments.Length - 1; i++)
					{
						dest = Directory.CreateDirectory(Path.Combine(dest.FullName, segments[i]));
					}
					FileInfo[] files = source.GetFiles(segments[segments.Length - 1]);
					if (files.Length == 0)
						throw new FileNotFoundException("Zero files to match recipe element '" + element + "'");
					foreach (FileInfo f in files)
					{
						string target = Path.Combine(dest.FullName, f.Name);
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
