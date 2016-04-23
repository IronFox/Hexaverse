//#define USE_SSL


using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Collections;
using System.Collections.Concurrent;
using System.IO;
using System.Security.Cryptography;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Net;
using Protocol;

namespace Consensus
{
	public static partial class Extensions
	{
		public static void SendToAll(this Protocol.ChannelSender sender, UInt32 channelID, Protocol.ByteBuffer buffer, Object item)
		{
			sender.Processor.SerializePacket(channelID, item, buffer);

			Client.VisitAll(client =>
			{
				if (client.Account != null)
					client.Transfer(buffer);
			});
			buffer.Clear();
		}
		public static void SendToAll(this Protocol.ChannelSender sender, UInt32 channelID, Protocol.ByteBuffer buffer, Object item, Func<Client, bool> filter)
		{
			sender.Processor.SerializePacket(channelID, item, buffer);

			Client.VisitAll(client =>
			{
				if (client.Account != null && filter(client))
					client.Transfer(buffer);
			});
			buffer.Clear();
		}

		public static void Broadcast<T>(this Protocol.OutChannel<T> channel, Protocol.ByteBuffer useBuffer, T item)
		{
			channel.SendToAll((UInt32)channel.ChannelID, useBuffer, item);
		}
		public static void Broadcast<T>(this Protocol.OutChannel<T> channel, Protocol.ByteBuffer useBuffer, T item, Func<Client, bool> filter)
		{
			channel.SendToAll((UInt32)channel.ChannelID, useBuffer, item, filter);
		}

		public static void Broadcast(this Protocol.SignalChannel channel, Protocol.ByteBuffer useBuffer)
		{
			useBuffer.Clear();
			useBuffer.AppendU32((UInt32)channel.ChannelID);
			useBuffer.AppendU32(0);
			Client.VisitAll(client =>
			{
				if (client.Account != null)
					client.Transfer(useBuffer);
			});
		}
	}








	public class Client : Protocol.Client
	{
		private static List<Client> allClients = new List<Client>();
		//private NetworkStream stream;
		public Faction Account = null;
		private static RNGCryptoServiceProvider provider = new RNGCryptoServiceProvider();
		//private long lastFrameSent;

		//private ManualResetEvent updateReceivedEvent = new ManualResetEvent(true),
		//						structureUpdateReceivedEvent = new ManualResetEvent(true);

		//public ConcurrentDictionary<UInt16, bool> HasContract = new ConcurrentDictionary<UInt16, bool>();



		protected override void OnClose()
		{
			base.OnClose();
			if (!Program.ShuttingDown)  //won't be able to lock this after all but joining, causing a deadlock
			{
				Account = null;
				lock (allClients)
				{
					allClients.Remove(this);
				}
			}
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
			//p.StartInfo.CreateNoWindow = true;
			p.StartInfo.FileName = cmd;
			p.StartInfo.Arguments = arguments;
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

		public static X509Certificate2 MakeCertificate(string owner, int numberOfValidDays = 365)
		{
			DirectoryInfo outDir = GetTemporaryDirectory();

			RandomNumberGenerator rng = new RNGCryptoServiceProvider();
			byte[] binary = new byte[32];
			rng.GetBytes(binary);
			string pass = System.Convert.ToBase64String(binary);

			string cfgPath = Path.Combine(outDir.FullName, "cfg");

			using (StreamWriter cfg = new StreamWriter(cfgPath))
			{

				cfg.OutLine("[ req ]");
				cfg.OutLine("default_bits           = 2048");
				cfg.OutLine("default_keyfile        = keyfile.pem");
				cfg.OutLine("distinguished_name     = req_distinguished_name");
				cfg.OutLine("attributes             = req_attributes");
				cfg.OutLine("prompt                 = no");
				cfg.OutLine("output_password        = " + pass);
				cfg.OutLine("[ req_distinguished_name ]");
				cfg.OutLine("C                      = DE");
				cfg.OutLine("ST                     = []");
				cfg.OutLine("L                      = []");
				cfg.OutLine("O                      = []");
				cfg.OutLine("OU                     = []");
				cfg.OutLine("CN                     = " + owner);
				cfg.OutLine("emailAddress           = []");
				cfg.OutLine("[ req_attributes ]");
				cfg.OutLine("challengePassword      = []");

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
			OpenSSL("pkcs12 -export -out " + pfxPath + " -passout pass: -inkey " + keyPath + " -passin file:" + passPath + " -in " + crtPath);
			//			OpenSSL("pkcs12 -export -out " + pfxPath + " -passout file:" + passPath + "2 -inkey " + keyPath + " -passin file:" + passPath + " -in " + crtPath);

			byte[] pfx = File.ReadAllBytes(pfxPath);

			Directory.Delete(outDir.FullName, true);

			//outPass = pass;
			//data = pfx;
			return new X509Certificate2(pfx, "", X509KeyStorageFlags.Exportable);
		}



		public static void StaticInitServerCertificate()
		{
			//certificate generated using http://blog.pluralsight.com/2012/02/13/selfcert-create-a-self-signed-certificate-interactively-gui-or-programmatically-in-net/
			//code taken from http://stackoverflow.com/questions/8539516/pfx-self-signed-x509-certificate-with-password-secure
			byte[] pfx = File.ReadAllBytes("certificate.pfx");
			_certificate = new X509Certificate2(pfx, "REMOVED_PASSWORD");
			byte[] publicBytes = _certificate.RawData;

			RSACryptoServiceProvider rsa = (RSACryptoServiceProvider)_certificate.PrivateKey;
			byte[] signedData = rsa.SignData(new System.Text.UTF8Encoding().GetBytes("Test"), new SHA1CryptoServiceProvider());

			RSACryptoServiceProvider rsa2 = (RSACryptoServiceProvider)new X509Certificate2(publicBytes).PublicKey.Key;

			bool verified = rsa2.VerifyData(new System.Text.UTF8Encoding().GetBytes("Test"), new SHA1CryptoServiceProvider(), signedData);
			Debug.Assert(verified);
		}

		private static bool ValidateClientCertificate(
				  object sender,
				  X509Certificate certificate,
				  X509Chain chain,
				  SslPolicyErrors sslPolicyErrors)
		{
			//if (sslPolicyErrors == SslPolicyErrors.None || sslPolicyErrors == SslPolicyErrors.RemoteCertificateChainErrors)
			return true;

			//Console.WriteLine("Certificate error: {0}", sslPolicyErrors);

			// Do not allow this client to communicate with unauthenticated servers. 
			//return false;
		}

		private Client(TcpClient TCPClient)
			: base(TCPClient, true)
		{ }



		public bool Start()
		{
			//stream = tcpClient.GetStream();
			RemoteEnd = TcpClient.Client.RemoteEndPoint.ToString();
			//SocketAddress addr = socket.RemoteEndPoint.Serialize();
			//Console.WriteLine(addr.Size);
			GateKey = RemoteEnd;
			GateKey = GateKey.Substring(0, GateKey.LastIndexOf(':'));




			SslStream sslStream = new SslStream(TcpClient.GetStream(), false, new RemoteCertificateValidationCallback(ValidateClientCertificate), null, EncryptionPolicy.RequireEncryption);
			sslStream.AuthenticateAsServer(_certificate, true, System.Security.Authentication.SslProtocols.Tls12, false);
			if (!sslStream.IsEncrypted)
			{
				sslStream.Close();
				base.Close();
				return false;
			}

			if (sslStream.IsMutuallyAuthenticated)
			{
				Program.Log(this, Logging.Significance.Low, "SSL Authentication concluded: " + sslStream.CipherAlgorithm.ToString() + ", strength " + sslStream.CipherStrength);
				string certHash = sslStream.RemoteCertificate.GetCertHashString();
				Faction acc = Faction.Collection.GetValue(certHash);
				if (acc != null)
				{
					Account = acc;
					acc.Client = this;
					Program.Log(this, Logging.Significance.Low, "Logged in as: " + Account.ToString());
				}
				else
				{
					if (acc == null)
						Program.Log(this, Logging.Significance.Unusual, "Client certificate not recognized: " + certHash);
					else
					{
						Program.Log(this, Logging.Significance.Unusual, "Client certificate recognized but identity mismatch");
					}
					//ForceDisconnect();
					//return false;
				}
			}
			else
				Program.Log(this, Logging.Significance.Low, "New connection. Not mutually authenticated.");

			//Interface.Out.Channel.PasswordSalt.SendTo(this, PasswordSalt);

			Program.Log(this, Logging.Significance.Common, "Client connected");

			base.StartSSL(sslStream);
			return true;
		}


		protected override void OnReadThreadExit()
		{
			if (Account != null)
				Account.Client = null;
			base.OnReadThreadExit();
			Program.Log(this, Logging.Significance.Low, "Ending read thread");
			Program.Log(this, Logging.Significance.Common, "Client connection lost or closed");
		}


		protected override void OnAbnormalException(Exception e)
		{
			base.OnAbnormalException(e);
			Program.Log(this, Logging.Significance.ClientFatal, e.Message);
			//			Program.ReportUnexpected(this, ex);   //so report it

		}



		protected override bool IsAuthenticated
		{
			get
			{
				return Account != null;
			}
		}


		public static bool AddNew(TcpClient TCPClient)
		{
			Client cl = new Client(TCPClient);
			if (!cl.Start())
				return false;
			lock (allClients)
			{
				allClients.Add(cl);
			}
			return true;
		}


		public static void VisitAll(Action<Client> action)
		{
			lock (allClients)
				foreach (var client in allClients)
					action(client);
		}

		private static void ToStream(UInt32 v, Stream stream)
		{
			stream.WriteByte((byte)(v & 0xFF));
			stream.WriteByte((byte)((v >> 8) & 0xFF));
			stream.WriteByte((byte)((v >> 16) & 0xFF));
			stream.WriteByte((byte)((v >> 24) & 0xFF));
		}


		//public void Send<T>(Interface.ChannelID channelID, T obj)
		//{
		//    StaticSender<T>.SendTo(channelID, Stream, Buffer, obj);
		//}

		public string RemoteEnd { get; private set; }
		public string GateKey { get; private set; }




		// public System.Net.EndPoint EndPoint { get { return socket.RemoteEndPoint; } }

		public override string ToString()
		{
			return RemoteEnd;
			//if (tcpClient != null)
			//    try
			//    {
			//        return tcpClient.Client.RemoteEndPoint.ToString();
			//    }
			//    catch
			//    { }
			//return "?";
		}

		static public implicit operator string (Client client)
		{
			return client.ToString();
		}


#if false
        public UInt32 ProcessTimeStamp(UInt32 stamp)
        {
            long    localAge = Time.NetTime() - localReferenceTime,
                    remoteAge = stamp - remoteReferenceTime;
            if (!hasRemoteReferenceTime || remoteAge > localAge)//received before it was sent
            {
                localReferenceTime = Time.NetTime();
                remoteReferenceTime = stamp;
                hasRemoteReferenceTime = true;
                localAge = 0;
                remoteAge = 0;
            }
            else
            {
                localReferenceTime += remoteAge;
                remoteReferenceTime = stamp;
                localAge -= remoteAge;
                remoteAge = 0;
            }
            UInt32 result = remoteReferenceTime + (UInt32)localAge;
            if (minRoundTrip >= 0)
                result += (UInt32)(minRoundTrip / 2);
            return result;
        }
#endif
		internal void ShutDown()
		{
			Debug.Assert(Program.ShuttingDown);
			ForceDisconnect();
		}






		public bool VersionMatched = false;
	}
}
