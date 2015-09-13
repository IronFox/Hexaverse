using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;

namespace Probe
{
	public static class Network
	{
		public static class Outbound
		{
			public static Protocol.OutChannel<UInt32> Hello = new Protocol.OutChannel<uint>(Protocol.ChannelID.Hello);
			public static Protocol.OutChannel<string> NewAccount = new Protocol.OutChannel<string>(Protocol.ChannelID.NewAccount);
		}


		private static Form1 accessForm;

		public static void Setup(Form1 form)
		{
			if (accessForm != null)
				return;
			accessForm = form;
			Protocol.ChannelMap.Register<Protocol.Interface.NewAccount>(Protocol.ChannelID.NewAccount, Event.NewAccount);
			Protocol.ChannelMap.Register<Protocol.Interface.AccountDetails>(Protocol.ChannelID.Authenticated, Event.Authenticated);

		}
		


		public static class Event
		{
			public static String getThumbPrint(X509Certificate cert) {
				//MessageDigest md = MessageDigest.getInstance("SHA-1");
				//byte[] der = cert.getEncoded();
				byte[] data = cert.GetRawCertData();
				SHA1 sha1 = new SHA1CryptoServiceProvider();
				byte[] hash = sha1.ComputeHash(data);
				string hex = BitConverter.ToString(hash);
				return hex.Replace("-","");
			}

			public static String getThumb256(X509Certificate cert)
			{
				SHA256 sha = new SHA256CryptoServiceProvider();
				byte[] hashed = sha.ComputeHash(cert.GetRawCertData());
				string hex = BitConverter.ToString(hashed);
				return hex.Replace("-", "");


			}

			public static void NewAccount(Protocol.Client client, Protocol.Interface.NewAccount acc)
			{
				X509Certificate2 cert = new X509Certificate2(acc.Certificate,"",X509KeyStorageFlags.Exportable);
				
				accessForm.Log("received certificate for new account '"+acc.Details.AccountName+"' (has private key="+cert.HasPrivateKey+", thumb="+cert.Thumbprint+")");
				accessForm.Log("my thumb: "+getThumbPrint(cert));
				accessForm.Log("my thumb256: " + getThumb256(cert));
			}

			public static void Authenticated(Protocol.Client client, Protocol.Interface.AccountDetails acc)
			{
				accessForm.Log("authenticated as '"+acc.AccountName+"' ("+new Guid(acc.GUID).ToString()+")");
			}
		}


		public static void Log(string line)
		{
			accessForm.Log(line);

		}

	}



	class Client : Protocol.Client
	{

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


		protected override void OnAbnormalException(Exception e)
		{
			Network.Log(e.Message);
		}

		protected override bool IsAuthenticated
		{
			get
			{
				return true;
			}
		}

		protected override void OnClose()
		{
			Network.Log("Connection closed");
		}
		protected override void OnReadThreadExit()
		{
			Network.Log("Read-thread exiting");
		}

		


		//public static X509Certificate ValidateLocalCertificate(
		//	Object sender,
		//	string targetHost,
		//	X509CertificateCollection localCertificates,
		//	X509Certificate remoteCertificate,
		//	string[] acceptableIssuers
		//)
		//{
		//	return _certificate;

		//}

		public static Client	Connect(string url, X509Certificate2 cert, string username)
		{
			int sep = url.IndexOf(':');
			if (sep == -1)
				return null;
			TcpClient tc = new TcpClient();
			tc.Connect(url.Substring(0, sep), int.Parse(url.Substring(sep + 1)));

			Client result = new Client(tc);


			SslStream sslStream = new SslStream(tc.GetStream(), false, new RemoteCertificateValidationCallback(ValidateServerCertificate), (sender,targetHost,localCertificates,remoteCertificate,issuers)=>cert, EncryptionPolicy.RequireEncryption);
			sslStream.AuthenticateAsClient(url.Substring(0, sep),/*collection*/null, System.Security.Authentication.SslProtocols.Ssl3, false);

			result.Start(sslStream);


			Network.Outbound.Hello.SendTo(result,Protocol.Version.Combined);

			if (cert == null)
			{
				Network.Outbound.NewAccount.SendTo(result,username);
			}
			

			return result;


		}

		private Client(TcpClient cl) : base(cl)
		{ }
	}
}
