#define USE_SSL


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

namespace Consensus
{




    abstract class ChannelHandler
    {
        private SerialInterface processor;
        public bool LoggedInOnly{get; private set;}


        virtual protected void OnReceive(Client sender, Object received) { throw new Exception("Expected signal but received object on channel "+ChannelID); }
        virtual protected void OnReceiveSignal(Client sender) { throw new Exception("Expected object but received signal on channel " + ChannelID); }

        public Interface.ChannelID ChannelID{get; private set;}

        public ChannelHandler(Interface.ChannelID myChannel, Type Serializable, bool loggedInOnly)
        {
            LoggedInOnly = loggedInOnly;
            ChannelID = myChannel;
            if (Serializable != typeof(void))
                processor = SerialInterface.Build(Serializable);
        }


        public void Handle(Client client, byte[] data, int dataSize)
        {
            if (dataSize == 0)
            {
                OnReceiveSignal(client);
                return;
            }
            if (processor == null)
                throw new Exception("Expected signal but received object on channel " + ChannelID);
            Object item = processor.Deserialize(data, dataSize);
            OnReceive(client, item);
        }
    }



    class GenericChannelHandler<T> : ChannelHandler
    {
        private Action<Client,T> handler;

        override protected void OnReceive(Client sender, Object received)
        {
            handler(sender, (T)received);
        }

        public GenericChannelHandler(Interface.ChannelID id, Action<Client, T> action, bool loggedInOnly): base(id,typeof(T),loggedInOnly)
        {
            handler = action;
        }


    };

    class SignalHandler : ChannelHandler
    {
        private Action<Client> handler;
        override protected void OnReceiveSignal(Client sender)
        {
            handler(sender);
        }
        public SignalHandler(Interface.ChannelID id, Action<Client> action, bool loggedInOnly)
            : base(id, typeof(void), loggedInOnly)
        {
            handler = action;
        }
    }



    public class ChannelSender
    {
        private SerialInterface processor;

        public ChannelSender(Type t)
        {
            processor = SerialInterface.Build(t);
        }


        public SerialInterface SerialInterface { get { return processor; } }

        public void SendTo(UInt32 channelID, ByteBuffer buffer, Client client, Object item)
        {
            processor.SerializePacket(channelID, item, buffer);

            client.Transfer(buffer);
            buffer.Clear();
        }
        public void SendToAll(UInt32 channelID, ByteBuffer buffer, Object item)
        {
            processor.SerializePacket(channelID, item, buffer);

            Client.VisitAll(client =>
            {
                if (client.Account != null)
                    client.Transfer(buffer);
            });
            buffer.Clear();
        }
        public void SendToAll(UInt32 channelID, ByteBuffer buffer, Object item, Func<Client, bool> filter)
        {
            processor.SerializePacket(channelID, item, buffer);

            Client.VisitAll(client =>
            {
                if (client.Account != null && filter(client))
                    client.Transfer(buffer);
            });
            buffer.Clear();
        }


    }




    public class OutChannel<T> : ChannelSender
    {
        private UInt32    channelID;
        public OutChannel(Interface.ChannelID channelID)
            : base(typeof(T))
        { this.channelID = (UInt32)channelID; }
        public void SendTo(Client client, T item)
        {
            base.SendTo((UInt32)channelID, client.Buffer, client, item);
        }
        public void SendTo(ByteBuffer useBuffer, Client client, T item)
        {
            base.SendTo((UInt32)channelID, useBuffer, client, item);
        }
        public void Broadcast(ByteBuffer useBuffer, T item)
        {
            base.SendToAll((UInt32)channelID, useBuffer, item);
        }
        public void Broadcast(ByteBuffer useBuffer, T item, Func<Client,bool> filter)
        {
            base.SendToAll((UInt32)channelID, useBuffer, item, filter);
        }

        public UInt32 ChannelID { get { return channelID; } }
    };

    public class SignalChannel
    {
        private UInt32 channelID;
        public SignalChannel(Interface.ChannelID channelID)
        { this.channelID = (UInt32)channelID; }
        public void SendTo(Client client)
        {
            SendTo(client.Buffer,client);
        }
        public void SendTo(ByteBuffer useBuffer, Client client)
        {
            useBuffer.Clear();
            useBuffer.AppendU32((UInt32)channelID);
            useBuffer.AppendU32(0);
            client.Transfer(useBuffer);
        }
        public void Broadcast(ByteBuffer useBuffer)
        {
            useBuffer.Clear();
            useBuffer.AppendU32((UInt32)channelID);
            useBuffer.AppendU32(0);
            Client.VisitAll(client =>
            {
                if (client.Account != null)
                    client.Transfer(useBuffer);
            });
        }

        public UInt32 ChannelID { get { return channelID; } }
    };

    class StaticSender<T>
    {
        private static ChannelSender sender = new ChannelSender(typeof(T));

        public static void SendTo(Interface.ChannelID channelID, Client client, ByteBuffer buffer,  T item)
        {
            sender.SendTo((UInt32)channelID, buffer, client, item);
        }
    }



    class ChannelMap
    {
        private static ConcurrentDictionary<Interface.ChannelID, ChannelHandler> channels = new ConcurrentDictionary<Interface.ChannelID, ChannelHandler>();

        public static ChannelHandler LookUp(Interface.ChannelID channelID)
        {
            if (!channels.ContainsKey(channelID))
                return null;
            return channels[channelID];
        }

        public static void Register<T>(Interface.ChannelID channelID, Action<Client, T> OnReceive, bool requireLogIn = true)
        {
            ChannelHandler channel = new GenericChannelHandler<T>(channelID, OnReceive, requireLogIn);
            channels[channelID] = channel;
        }

        public static void RegisterSignal(Interface.ChannelID channelID, Action<Client> onReceive, bool requireLogIn = true)
        {
            ChannelHandler channel = new SignalHandler(channelID, onReceive, requireLogIn);
            channels[channelID] = channel;
        }
    }





    public class Client
    {
        private TcpClient tcpClient;
        private static List<Client> allClients = new List<Client>();
        private Thread threadHandle;
        //private NetworkStream stream;
        #if !USE_SSL
            private Socket socket;
        #endif
        public ByteBuffer Buffer = new ByteBuffer(1024);
        const int SafePacketSize = 64000000;
        public Account Account = null;
        private byte currentSerial = 0, currentStructureSerial = 0;
        private static RNGCryptoServiceProvider provider = new RNGCryptoServiceProvider();
        //private long lastFrameSent;

        private ManualResetEvent updateReceivedEvent = new ManualResetEvent(true),
                                structureUpdateReceivedEvent = new ManualResetEvent(true);

        public ConcurrentDictionary<UInt16, bool> HasContract = new ConcurrentDictionary<UInt16, bool>();


        #if USE_SSL
            private SslStream sslStream;
        #endif





        private byte nextSerial()
        {
            lock (this)
            {
                currentSerial++;
                updateReceivedEvent.Reset();
                //Console.Out.WriteLine("locked " + currentSerial);
                return currentSerial;
            }
        }

        private byte nextStructureSerial()
        {
            lock (this)
            {
                currentStructureSerial++;
                structureUpdateReceivedEvent.Reset();
                //Console.Out.WriteLine("locked " + currentSerial);
                return currentStructureSerial;
            }
        }
        

        public void SignalUpdateConfirmation(byte value)
        {
            lock (this)
            {
                //Console.Out.WriteLine("unlocked" + value);
                //Debug.Assert(value == currentSerial); //lets ignore this for now
                updateReceivedEvent.Set();
            }
        }

        public void SignalStructureUpdateConfirmation(byte value)
        {
            lock (this)
            {
                //Console.Out.WriteLine("unlocked" + value);
                //Debug.Assert(value == currentSerial); //lets ignore this for now
                structureUpdateReceivedEvent.Set();
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
			Environment.SetEnvironmentVariable("OPENSSL_CONF", ossl+".cfg");
									//set OPENSSL_CONF=c:\OpenSSL-Win32\bin\openssl.cfg   

			string output;
			bool rc = Execute(ossl, command, out output);
			Debug.Assert(rc, output);

		}

		public static X509Certificate2 MakeCertificate(string owner, int numberOfValidDays=365)
		{
			DirectoryInfo outDir = GetTemporaryDirectory();

			RandomNumberGenerator rng = new RNGCryptoServiceProvider();
			byte[] binary = new byte[32];
			rng.GetBytes(binary);
			string pass = System.Convert.ToBase64String(binary);

			string cfgPath = Path.Combine(outDir.FullName,"cfg");

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







			string keyPath = Path.Combine(outDir.FullName,"temp.key");
			string csrPath = Path.Combine(outDir.FullName,"temp.csr");
			string crtPath = Path.Combine(outDir.FullName,"temp.crt");
			string pfxPath = Path.Combine(outDir.FullName,"temp.pfx");


			string passPath = Path.Combine(outDir.FullName,"pass");
			StreamWriter writer = new StreamWriter(passPath);
			writer.Write(pass);
			writer.Close();

			File.Copy(passPath,passPath+"2");


			OpenSSL("genpkey -des3 -algorithm RSA -out " + keyPath + " -outform PEM -pkeyopt rsa_keygen_bits:2048 -pass file:" + passPath);
			OpenSSL("req -new -key "+keyPath+" -passin file:"+passPath+" -out "+csrPath +" -config "+cfgPath);
			OpenSSL("x509 -req -days "+numberOfValidDays.ToString()+" -in "+csrPath+" -passin file:"+passPath+" -signkey "+keyPath+" -out "+crtPath);
			OpenSSL("pkcs12 -export -out "+pfxPath+" -passout pass: -inkey "+keyPath+" -passin file:"+passPath+" -in "+crtPath);
//			OpenSSL("pkcs12 -export -out " + pfxPath + " -passout file:" + passPath + "2 -inkey " + keyPath + " -passin file:" + passPath + " -in " + crtPath);

			byte[] pfx = File.ReadAllBytes(pfxPath);

			Directory.Delete(outDir.FullName,true);

			//outPass = pass;
			//data = pfx;
            return new X509Certificate2(pfx);
		}



        public static void InitCertificate()
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
        {
            tcpClient = TCPClient;
            tcpClient.NoDelay = true;
		}

		public bool Start()
		{
            //stream = tcpClient.GetStream();
            #if !USE_SSL
                socket = tcpClient.Client;
                socket.NoDelay = true;
                RemoteEnd = socket.RemoteEndPoint.ToString();
            #else
                        RemoteEnd = tcpClient.Client.RemoteEndPoint.ToString();
            #endif
            //SocketAddress addr = socket.RemoteEndPoint.Serialize();
            //Console.WriteLine(addr.Size);
            GateKey = RemoteEnd;
            GateKey = GateKey.Substring(0, GateKey.LastIndexOf(':'));




            #if USE_SSL
				sslStream = new SslStream(tcpClient.GetStream(), false, new RemoteCertificateValidationCallback (ValidateClientCertificate), null, EncryptionPolicy.RequireEncryption);
                sslStream.AuthenticateAsServer(_certificate,true,System.Security.Authentication.SslProtocols.Ssl3,false);
				if (!sslStream.IsEncrypted)
				{ 
					sslStream.Close();
					tcpClient.Close();
					tcpClient = null;
					sslStream = null;
					return false;
				}
			#endif
            //Interface.Out.Channel.PasswordSalt.SendTo(this, PasswordSalt);
            
            threadHandle = new Thread(new ThreadStart(this.ThreadMain));
            threadHandle.Start();
			return true;
        }



		public bool Authenticate(string accountName, string accountUUID)
		{
			if (sslStream.IsMutuallyAuthenticated)
			{
				Program.Log(this, Significance.Important, "SSL Authentication concluded: " + sslStream.CipherAlgorithm.ToString() + ", strength " + sslStream.CipherStrength);
				Account acc = Account.Collection.GetValue(sslStream.RemoteCertificate.GetCertHashString());
				if (acc != null && acc.Name == accountName && acc.UUID == accountUUID)
				{
					if (acc.IsLoggedIn)
					{
						Client cl = acc.LoggedInFrom;
						if (cl != null)
							cl.ForceDisconnect();
					}
					Account = acc;
					Account.Activate(this);
					Program.Log(this, Significance.Unusual, "Logged in as: " + Account.ToString());
					return true;
				}
				else
				{
					if (acc == null)
						Program.Log(this, Significance.ClientFatal, "Client certificate not recognized: " + sslStream.RemoteCertificate.GetCertHashString());
					else
					{
						Program.Log(this, Significance.ClientFatal, "Client certificate recognized but identity mismatch");
					}
					ForceDisconnect();
					return false;
				}
			}
			Program.Log(this, Significance.ClientFatal, "Client certificate not provided");
			ForceDisconnect();
			return false;
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

        public void Transfer(ByteBuffer data)
        {
            #if USE_SSL
				if (sslStream == null)
					return;
			#else
            if (socket == null)
            {
                return; //probably closing down
            }
			#endif
            try
            {
                #if USE_SSL
                    lock (sslStream)
                    {
                        sslStream.Write(data.GetArray(), 0, data.Length);
                        sslStream.Flush();
                        return;
                    }
                #else
                    lock (socket)
                    {
                        socket.Send(data.GetArray(), data.Length, SocketFlags.None);
                        return;
                    }
                #endif
            }
            catch (IOException)
            { }
            catch (SocketException)
            { }
            catch (ObjectDisposedException)
            { }
            catch (Exception ex)
            {
                Program.ReportUnexpected(this, ex);
            }
            close();
        }

        //public void Send<T>(Interface.ChannelID channelID, T obj)
        //{
        //    StaticSender<T>.SendTo(channelID, Stream, Buffer, obj);
        //}

        public string RemoteEnd { get; private set; }
        public string GateKey { get; private set; }


        private void close()
        {
            lock (this)
            {
                if (tcpClient != null)
                {
                    #if USE_SSL
                        sslStream.Close();
                        sslStream = null;
                    #endif
                    tcpClient.Close();
                    tcpClient = null;
                    #if !USE_SSL
                        socket.Close();
                        socket = null;
                    #endif
                    if (!Program.ShuttingDown)  //won't be able to lock this after all but joining, causing a deadlock
                    {
                        if (Account != null)
                        {
                            Account.Deactivate();
                            Account = null;
                        }
                        lock (allClients)
                        {
                            allClients.Remove(this);
                        }
                    }
                }
            }
            updateReceivedEvent.Set();
        }

        private void ThreadMain()
        {
            Program.Log(this, Significance.Common, "Client connected");

            UInt32 channel = 0, packetSize = 0;
            ChannelHandler handler = null;

            bool readingHeader = true;

            try
            {
                byte[]  buffer = new byte[tcpClient.ReceiveBufferSize],
                        decodeBuffer = new byte[tcpClient.ReceiveBufferSize];
                DataQueue queue = new DataQueue(tcpClient.ReceiveBufferSize);

                while (tcpClient != null)
                {
                    #if USE_SSL
                        int read = sslStream.Read(buffer, 0, tcpClient.ReceiveBufferSize);
                    #else
                        int read = socket.Receive(buffer);
                    #endif
                    queue.Append(buffer, read);

                    if (readingHeader)
                    {
                        if (queue.Length >= 8)
                        {
                            channel = queue.PopUInt32();
                            packetSize = queue.PopUInt32();
                            readingHeader = false;
                            if (packetSize > SafePacketSize)
                            {
                                Program.Log(this, Significance.ClientFatal, "Received unsafe packet size (" + packetSize + ") on channel " + channel);
                                close();
                                continue;
                            }
                            handler = ChannelMap.LookUp((Interface.ChannelID)channel);
                            if (handler == null)
                            {
                                Program.Log(this, Significance.ClientFatal, "Received packet on unexpected channel " + channel);
                                close();
                                continue;
                            }
                            if (handler.LoggedInOnly && Account == null)
                            {
                                Program.Log(this, Significance.ClientFatal, "Received packet on channel " + (Interface.ChannelID)channel + ", which is reserved for authenticated in clients");
                                close();
                                continue;
                            }
                        }
                    }

                    if (!readingHeader)
                        if (queue.Length >= packetSize)
                        {
                            if (decodeBuffer.Length < packetSize)
                                decodeBuffer = new byte[packetSize];
                            queue.PopData(decodeBuffer, (int)packetSize);
                            handler.Handle(this, decodeBuffer, (int)packetSize);
                            readingHeader = true;
                            handler = null;
                        }
                }
            }
            catch (ObjectDisposedException ) //socket has been closed
            { }
            catch (SocketException ) //probably safe, tpp
            { }
            catch (BlockException exception)
            {
                Program.Log(this, Significance.ClientFatal, exception.Message);
            }
            catch (IOException ) //this is fired if the ssl connection is terminated
            {}
            catch (Exception ex)    //should not happen
            {
                Program.ReportUnexpected(this, ex);   //so report it
            }
            Program.Log(this,Significance.Low, "Ending read thread");
            Program.Log(this,Significance.Common,"Client connection lost or closed");
            close();
        }


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

        static public implicit operator string(Client client)
        {
            return client.ToString();
        }

        private long localReferenceTime;
        private UInt32 remoteReferenceTime;
        private bool hasRemoteReferenceTime = false;

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

        public void ForceDisconnect()
        {
            close();
            threadHandle.Join();
        }




    }
}
