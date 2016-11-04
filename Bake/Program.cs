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
				Console.WriteLine("Clearing previous baked...");
				try
				{
					Directory.Delete("baked", true);
				}
				catch { }
				Console.WriteLine("Reading recipe...");

				string[] recipe = System.IO.File.ReadAllLines(@"bakeRecipe.txt");

				Console.WriteLine("Processing recipe...");
				DirectoryInfo globalDest = Directory.CreateDirectory("baked");
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
					{
						throw new Exception("Missing at least one file to match recipe element '" + element + "'");
					}
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
				Console.WriteLine(e);
				return -1;
			}
			return 0;
		}
	}
}
