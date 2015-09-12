using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Security.Cryptography;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Net;
using System.Net.Sockets;
using System.IO;
using System.Diagnostics;


namespace Probe
{
	public partial class Form1 : Form
	{
		public Form1()
		{
			InitializeComponent();
		}

		private static X509Certificate2 _certificate;

		static DirectoryInfo GetTemporaryDirectory()
		{
			string tempDirectory;
			do
			{
				tempDirectory = Path.Combine(Path.GetTempPath(), Path.GetRandomFileName());
			}
			while (Directory.Exists(tempDirectory));
			return Directory.CreateDirectory(tempDirectory);
		}



		static bool Execute(string cmd, string arguments, out string result)
		{
			Console.WriteLine(cmd + arguments);
			Process p = new Process();
			p.StartInfo.UseShellExecute = false;
			p.StartInfo.RedirectStandardOutput = true;
			p.StartInfo.FileName = cmd;
			p.StartInfo.Arguments = arguments;
			p.StartInfo.CreateNoWindow = true;
			if (!p.Start())
			{
				result = "Process start failed";
				return false;
			}
			result = p.StandardOutput.ReadToEnd();
			p.WaitForExit();
			return p.ExitCode == 0;
		}

		static void OpenSSL(string command)
		{
			string ossl = @"C:\OpenSSL-Win32\bin\openssl";
			Environment.SetEnvironmentVariable("OPENSSL_CONF", ossl + ".cfg");
			//set OPENSSL_CONF=c:\OpenSSL-Win32\bin\openssl.cfg   

			string output;
			bool rc = Execute(ossl, command, out output);
			Debug.Assert(rc, output);

		}

		public static X509Certificate2 GenSelfSignedCert(string owner, out string outPass, int numberOfValidDays = 365)
		{
			DirectoryInfo outDir = GetTemporaryDirectory();

			RandomNumberGenerator rng = new RNGCryptoServiceProvider();
			byte[] binary = new byte[32];
			rng.GetBytes(binary);
			string pass = System.Convert.ToBase64String(binary);

			string cfgPath = Path.Combine(outDir.FullName, "cfg");

			using (StreamWriter cfg = new StreamWriter(cfgPath))
			{

				cfg.WriteLine("[ req ]");
				cfg.WriteLine("default_bits           = 2048");
				cfg.WriteLine("default_keyfile        = keyfile.pem");
				cfg.WriteLine("distinguished_name     = req_distinguished_name");
				cfg.WriteLine("attributes             = req_attributes");
				cfg.WriteLine("prompt                 = no");
				cfg.WriteLine("output_password        = " + pass);
				cfg.WriteLine("[ req_distinguished_name ]");
				cfg.WriteLine("C                      = DE");
				cfg.WriteLine("ST                     = []");
				cfg.WriteLine("L                      = []");
				cfg.WriteLine("O                      = []");
				cfg.WriteLine("OU                     = []");
				cfg.WriteLine("CN                     = " + owner);
				cfg.WriteLine("emailAddress           = []");
				cfg.WriteLine("[ req_attributes ]");
				cfg.WriteLine("challengePassword      = []");

			}







			string keyPath = Path.Combine(outDir.FullName, "temp.key");
			string csrPath = Path.Combine(outDir.FullName, "temp.csr");
			string crtPath = Path.Combine(outDir.FullName, "temp.crt");
			string pfxPath = Path.Combine(outDir.FullName, "temp.pfx");


			string passPath = Path.Combine(outDir.FullName, "pass");
			StreamWriter writer = new StreamWriter(passPath);
			writer.Write(pass);
			writer.Close();

			File.Copy(passPath, passPath + "2");


			OpenSSL("genpkey -des3 -algorithm RSA -out " + keyPath + " -outform PEM -pkeyopt rsa_keygen_bits:2048 -pass file:" + passPath);
			OpenSSL("req -new -key " + keyPath + " -passin file:" + passPath + " -out " + csrPath + " -config " + cfgPath);
			OpenSSL("x509 -req -days " + numberOfValidDays.ToString() + " -in " + csrPath + " -passin file:" + passPath + " -signkey " + keyPath + " -out " + crtPath);
			OpenSSL("pkcs12 -export -out " + pfxPath + " -passout file:" + passPath + "2 -inkey " + keyPath + " -passin file:" + passPath + " -in " + crtPath);

			byte[] pfx = File.ReadAllBytes(pfxPath);

			Directory.Delete(outDir.FullName, true);

			outPass = pass;
			return new X509Certificate2(pfx, pass);
		}

		SslStream	sslStream;
		TcpClient	client;

		public static bool Callback(Object sender, X509Certificate cert, X509Chain chain, SslPolicyErrors sslPolicyErrors)
		{
			MessageBox.Show(sslPolicyErrors.ToString());
			return true;
		}

		private static bool ValidateServerCertificate(
				  object sender,
				  X509Certificate certificate,
				  X509Chain chain,
				  SslPolicyErrors sslPolicyErrors)
		{
			if (sslPolicyErrors == SslPolicyErrors.None || sslPolicyErrors == SslPolicyErrors.RemoteCertificateChainErrors)
				return true;

			Console.WriteLine("Certificate error: {0}", sslPolicyErrors);

			// Do not allow this client to communicate with unauthenticated servers. 
			return false;
		}

		

		public static X509Certificate ValidateLocalCertificate(
			Object sender,
			string targetHost,
			X509CertificateCollection localCertificates,
			X509Certificate remoteCertificate,
			string[] acceptableIssuers
		)
		{
			return _certificate;	

		}

		private void connect_Click(object sender, EventArgs e)
		{
			client = new TcpClient();
			string url = host.Text;
			int sep = url.IndexOf(':');

			ServicePointManager.ServerCertificateValidationCallback = new RemoteCertificateValidationCallback(Callback);

			try
			{ 
			
				client.Connect(url.Substring(0,sep),int.Parse( url.Substring(sep+1)));
				client.NoDelay = true;

				if (_certificate == null)
				{ 
					Log("Creating new ceritificate...");
					string pass;
					X509Certificate2 cert = GenSelfSignedCert("ironfox", out pass);
//					X509Certificate c0 = cert;
					_certificate = cert;
					Log("Certificate created: "+cert.Thumbprint);
				}

				sslStream = new SslStream(client.GetStream(), false, new RemoteCertificateValidationCallback(ValidateServerCertificate), new LocalCertificateSelectionCallback(ValidateLocalCertificate), EncryptionPolicy.RequireEncryption);
				//Debug.Assert(cert.());
				//X509CertificateCollection collection = new X509CertificateCollection();
				//collection.Add(_certificate);
				//Debug.Assert(collection[0] == _certificate);
				sslStream.AuthenticateAsClient(url.Substring(0, sep),/*collection*/null,System.Security.Authentication.SslProtocols.Ssl3,false);
				
				//				sslStream.

				logOut.Text += "\r\nSSL Authentication concluded: " + sslStream.CipherAlgorithm.ToString() + ", strength " + sslStream.CipherStrength;
				logOut.Text += "\r\nauth: "+sslStream.IsMutuallyAuthenticated;
				//				Program.Log(this, Significance.Common, sslStream.RemoteCertificate.GetCertHashString());
			}
			catch (Exception ex)
			{
				logOut.Text += "\r\n" + ex;

			}
		}

		void Log(string line)
		{
			logOut.Text += "\r\n" + line;


		}
	}
}
