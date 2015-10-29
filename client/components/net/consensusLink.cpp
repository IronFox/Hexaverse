#include <global_root.h>


#include "encryptionHelper.h"
#include "../dialog.h"
#include "../../version.h"

#include <net/tcp.h>
#include <io/file_system.h>

#include "../../../shared/logEvent.h"
#include "../../../shared/uniqueID.h"

#include "../../../shared/protocol.h"

namespace ConsensusLink
{
	namespace Detail
	{
		namespace Protocol
		{
			namespace ChannelID
			{
				enum ChannelID
				{
					Hello,				//c2s : UINT32 protocol version -> [if authenticated] s2c : (Authenticated)Protocol.Interface.AccountDetails [if not authenticated] s2c : (NotAuthenticated) [if version incompatible] s2c : VersionMismatch
					VersionMismatch,	//s2c : <signal>
					RequestNewAccount,	//c2s : <signal> : request new account creation -> s2c : (CreatedNewAccount)Protocol.Interface.NewAccount, s2c : (Authenticated)Protocol.Interface.AccountDetails
					CreatedNewAccount,	//s2c : Protocol.Interface.NewAccount
					Authenticated,		//s2c : Protocol.Interface.AccountDetails
					NotAuthenticated,	//s2c : <signal>
					SetAccountName,		//c2s : string -> [if valid] s2c : (Authenticated)Protocol.Interface.AccountDetails, [otherwise] s2c : (BadNickname)string
					BadAccountName,		//s2c : string

					Count

				};
			}

			static const UINT32 ContentVersion = 12;
			static const UINT32 ProtocolVersion = (UINT32)ChannelID::Count | (ContentVersion << 16);


			NETSTRUCT2(AccountDetails
				,Package::NetString<64>, accountName
				,NETA(BYTE), guid
			);

			NETSTRUCT3(NewAccount
				,AccountDetails, details
				,NETA(BYTE), certificate
				,NETA(BYTE), certificateKey
			);
			


		}


		namespace Receiver
		{
			void	CreatedNewAccount(Protocol::NewAccount&acc);
			void	Authenticated(Protocol::AccountDetails&acc);
			void	BadAccountName(Package::NetString<256>&msg);
		}

		namespace Channels
		{
			TCP::ObjectSender<NET(UINT32),Protocol::ChannelID::Hello>	hello;
			TCP::ObjectSender<Package::NetString<32>,Protocol::ChannelID::SetAccountName>	setAccountName;


			TCP::Channel<Protocol::NewAccount,0, Protocol::ChannelID::CreatedNewAccount>	createdNewAccount(Receiver::CreatedNewAccount);
			TCP::Channel<Protocol::AccountDetails,0, Protocol::ChannelID::Authenticated>	authenticated(Receiver::Authenticated);
			TCP::Channel<Package::NetString<256>,0, Protocol::ChannelID::BadAccountName>	badAccountName(Receiver::BadAccountName);
		}





		mbedtls_ssl_config			config;
		mbedtls_entropy_context		entropy;
		mbedtls_ctr_drbg_context	secureRandom;
		mbedtls_x509_crt			loadedCertificate;
		mbedtls_pk_context			loadedCertificateKey;

		TCP::Client		client;

		namespace Config
		{
			Array<BYTE>		certificate,
							certificateKey;
			String			accountName;
			TUniqueID		accountID;
		}


		bool				created = false;
		bool				connect = false;




		String	ErrorToString(int rc)
		{
			char buffer[0x100];
			mbedtls_strerror(rc,buffer,sizeof(buffer));
			return String(buffer)+" (="+String(rc)+")";
		}



		void Debug( void *ctx, int level, const char *file, int line, const char *str )
		{
			((void) level);
			_CRT_UNUSED(ctx);
			LogEvent(EventType::Debug,nullptr,"In "+String(file)+", line "+String(line)+": '"+str+"'");

			//fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
			//fflush(  (FILE *) ctx  );
		}

		void Disconnect();

		void	Destroy()
		{
			if (!created)
				return;
			Disconnect();
			mbedtls_ssl_config_free( &config );
			mbedtls_ctr_drbg_free( &secureRandom );
			mbedtls_entropy_free( &entropy );
			mbedtls_x509_crt_free( &loadedCertificate );
			mbedtls_pk_free(&loadedCertificateKey);
			created = false;
		}


		class SocketAccess : public TCP::SocketAccess
		{
		public:
			

			mbedtls_net_context		context;
			mbedtls_ssl_context		ssl;
			std::atomic<bool>		isDestroyed = true;

			/**/					SocketAccess()
			{
			}
			virtual					~SocketAccess()
			{
				CloseSocket();
			};
			virtual void			SetSocket(SOCKET socket) override
			{
				LogEvent(EventType::SessionRelated,nullptr,"Creating socket");

				isDestroyed = false;
				//mbedtls_x509_crt_init(&certificate);
				//mbedtls_pk_init(&certificateKey);
				mbedtls_net_init( &context );
				mbedtls_ssl_init(&ssl);
				mbedtls_ssl_setup(&ssl,&config);
				//mbedtls_ssl_set_hs_own_cert(&ssl,&certificate,
				context.fd = (int)socket;
				int ret;
				if( ( ret = mbedtls_ssl_set_hostname( &ssl, "Consensus Host" ) ) != 0 )
					throw IO::Network::GeneralFault( "mbedtls_ssl_set_hostname returned "+ErrorToString(ret));
				mbedtls_ssl_set_bio( &ssl, &context, mbedtls_net_send, mbedtls_net_recv, NULL );


				for (;;)
				{
					ret = mbedtls_ssl_handshake(&ssl);

					if (ret == 0)
						break;
					if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
						continue;

					throw IO::Network::GeneralFault( "mbedtls_ssl_handshake returned "+ErrorToString(ret));
				}


				const mbedtls_x509_crt* cert = mbedtls_ssl_get_peer_cert(&ssl);
				if (cert != nullptr)
				{
					const void*const certData = cert->raw.p;
					const size_t certSize = cert->raw.len;

					String bin = Bin2Str::Interpret(certData,certSize);
					LogEvent(EventType::SessionRelated,nullptr, "Consensus: Server certificate: "+bin);
				}

			}
			virtual void			CloseSocket() override
			{
				LogEvent(EventType::SessionRelated,nullptr,"Closing socket");
				bool exp = false;
				if (isDestroyed.compare_exchange_strong(exp,true))
				{
					SOCKET sock = (SOCKET)context.fd;
					mbedtls_ssl_free( &ssl );
					mbedtls_net_free( &context );
					closesocket(sock);

				}
			}

			virtual bool			IsClosed() const override {return isDestroyed;}
			virtual SocketAccess*	CloneClass() const override {return new SocketAccess();}

			virtual	int				Read(void*data, size_t dataSize) override
			{
				for (;;)
				{
					int rs = mbedtls_ssl_read( &ssl, (unsigned char*)data, dataSize );
					if (rs != MBEDTLS_ERR_SSL_WANT_READ)
						return rs;
				}
			}
			virtual int				Write(const void*data, size_t dataSize) override
			{
				const char* at = (const char*)data,
						* end = at + dataSize;
				while (at < end)
				{
					int sent = mbedtls_ssl_write( &ssl, (const unsigned char*)at, end-at );
					if (sent == MBEDTLS_ERR_SSL_WANT_WRITE)
						continue;
//					send(socketHandle,at,end-at,0);
					if (sent <= 0)
						return sent;
					at += sent;
				}
				LogEvent(EventType::SessionRelated,nullptr,"Sent "+String(dataSize)+" bytes");
//				MBEDTLS_SSL_FLUSH_BUFFERS
				return (int)dataSize;
			}
		};

		void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
		}

		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventType::Debug,nullptr,"Consensus: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
					ASSERT__(Channels::hello.SendTo(client,Protocol::ProtocolVersion));
				break;

			}
			Dialog::SignalSituationChange();
		}

		void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
		}

		void OnSignal(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::Debug,nullptr,"Consensus: "+origin.ToString()+": Signal on channel "+String(channel));
			switch (channel)
			{
				case Protocol::ChannelID::NotAuthenticated:
					client.SendSignal(Protocol::ChannelID::RequestNewAccount);
					LogEvent(EventType::SessionRelated,nullptr,"Not authenticated: Requested new account");
				break;
				case Protocol::ChannelID::VersionMismatch:
					LogEvent(EventType::OperationAborted,nullptr,"Server signalled protocol version mismatch. Unable to connect to Consensus network");
					connect = false;
					client.Disconnect();
				break;

			}
			Dialog::SignalSituationChange();
		}


		Timer::Time	lastConnectionAttempt = timer.Now();

		namespace Config
		{
			String		lastHost;
			bool		connectOnCreate = false;
			bool		askToEncryptPrivateKey = false;
			FileSystem::Folder	certificateFolder;

			void		SecureWrite(const String&filename, const void*data, size_t dataSize)
			{
				SHA256::HashContainer payloadHash;
				SHA256::Hash(data,dataSize,payloadHash);

				FileStream outStream;
				outStream.OpenCreate(filename.c_str());
				outStream.Write(payloadHash.bytes,payloadHash.NumBytes);
				outStream.Write(data,dataSize);
				outStream.Close();
				FileSystem::Copy(filename,filename+"2",true);

				LogEvent(EventType::SessionRelated,nullptr,"File written: "+filename);
				LogEvent(EventType::SessionRelated,nullptr,"Copied to: "+filename+"2");

			}

			void		SaveCertificate(const char*password)
			{
				//ultimately we will want to generate an encrypted PEM file here. but until someone actually asks for it, a binary custom format may be easier and safer in terms of bit consistency. pem has no hashing of any sort

				Buffer0<BYTE,Engine::Keyboard::SecureStrategy>	payload;

//				ByteStream payload;

				if (password && password[0])
				{
					payload << (BYTE)1;

					SHA256::HashContainer hashed;
					SHA256::Hash(Config::certificateKey.pointer(),Config::certificateKey.length(),hashed);
					payload.Append(hashed.bytes,SHA256::HashContainer::NumBytes);


					AES	aes;
					aes.RandomizeIV(&secureRandom);
					aes.SetPWD(password,strlen(password));
					
					Array<BYTE> copy = Config::certificateKey;
					aes.Encrypt(copy.pointer(),copy.GetContentSize());
					payload.Append(copy.pointer(),copy.GetContentSize());
					AES::Wipe(copy.pointer(),copy.GetContentSize());

					//memcpy(payload.AppendRow(copy.length()),copy.pointer(),copy.length());
					//payload << copy;
				}
				else
				{
					payload << (BYTE)0;
					payload.Append(certificateKey.pointer(),certificateKey.GetContentSize());
				}


				String keyFileName = (certificateFolder.GetLocation() + FOLDER_SLASH + "private.key");
				String certFileName = (certificateFolder.GetLocation() + FOLDER_SLASH + "public.cert");

				SecureWrite(keyFileName,payload.pointer(),payload.GetContentSize());
				SecureWrite(certFileName,certificate.pointer(),certificate.GetContentSize());
			}
		}

		void	Reconnect()
		{
			connect = true;
			lastConnectionAttempt = timer.Now();
			client.ConnectAsync(Config::lastHost);
		}
		void	Disconnect()
		{
			client.Disconnect();
			connect = false;
		}

		void	Authenticate();


		void	Create()
		{
			if (created)
				Destroy();
			connect = false;
			mbedtls_debug_set_threshold(1);
			mbedtls_ssl_config_init( &config );
			mbedtls_entropy_init( &entropy );
			mbedtls_ctr_drbg_init( &secureRandom );
			mbedtls_x509_crt_init( &loadedCertificate );
			mbedtls_pk_init(&loadedCertificateKey);

			
			int ret;
			if( ( ret = mbedtls_ctr_drbg_seed( &secureRandom, mbedtls_entropy_func, &entropy,
                           nullptr,
                           0 ) ) != 0 )
			{
				FATAL__( " failed\n  ! mbedtls_ctr_drbg_seed returned "+String(ret) );
				return;
			}

			if( ( ret = mbedtls_ssl_config_defaults( &config,
							MBEDTLS_SSL_IS_CLIENT,
							MBEDTLS_SSL_TRANSPORT_STREAM,
							MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
			{
				FATAL__( " failed\n  ! mbedtls_ssl_config_defaults returned "+String(ret) );
				return;
			}

			mbedtls_ssl_conf_authmode( &config, MBEDTLS_SSL_VERIFY_NONE );

			mbedtls_ssl_conf_rng( &config, mbedtls_ctr_drbg_random, &secureRandom );
			mbedtls_ssl_conf_dbg( &config, &Debug, nullptr );
			



			client.SetSocketAccess<SocketAccess>();
			client.onDeserializationFailed = OnDeserializationFailed;
			client.onEvent = OnEvent;
			client.onIgnorePackage = OnIgnorePackage;
			client.onSignal = OnSignal;
			client.SetAsync(false);

			client.InstallChannel(Channels::createdNewAccount);
			client.InstallChannel(Channels::badAccountName);
			client.InstallChannel(Channels::authenticated);

			client.OpenSignalChannel(Protocol::ChannelID::VersionMismatch,0);
			client.OpenSignalChannel(Protocol::ChannelID::NotAuthenticated,0);

			created = true;
		}

	/*	void	Connect(const String&host)
		{
			Config::lastHost = host;
			Reconnect();
		}*/

		namespace Receiver
		{
			
			void	CreatedNewAccount(Protocol::NewAccount&acc)
			{
				Config::certificate.adoptData(acc.certificate);
				Config::certificateKey.adoptData(acc.certificateKey);
				Config::accountName.adoptData(acc.details.accountName);
				ASSERT_EQUAL__(acc.details.guid.GetLength(),TUniqueID::NumBytes);
				memcpy(Config::accountID.bytes,acc.details.guid.pointer(),TUniqueID::NumBytes);

				if (Config::certificateFolder.IsValidLocation())
				{
					if (!Config::askToEncryptPrivateKey)
					{
						Config::SaveCertificate(nullptr);
					}
					else
					{
						Array<BYTE>	certificateCopy = Config::certificate;
						Dialog::AskYesNoHelp("The server sent a new account certificate for the Consensus network. Do you wish to encrypt the received private key using a password?",
							"The certificate you just received is the key to your new account. It will be stored persistently in "+Config::certificateFolder.GetLocation()+".\n"
							"Anyone with access to your files will be able to use it and log in with full control over your account. To prevent this from happening, using a password is recommended, but not required.\n"
							"However, if you forget your password, then access to your account will be irreversably lost, same as if you had lost the certificate itself.\n"
							"If you do set a password, you will be asked for this password every time you start " PRODUCT_NAME ". The password can be changed or removed later on.",
							[certificateCopy](bool doEncrypt)
							{
								if (doEncrypt)
								{
									Dialog::QueryPassword("Please specify the password to use for account certificate encryption. It can be any length. Leave empty if you changed your mind, and don't want to encrypt your certificate.",
									[](const char*password)
									{
										Config::SaveCertificate(password);
									},
									[certificateCopy]()->Dialog::CheckResult
									{
										return Config::certificate == certificateCopy ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
									},true);
								}
							},
							[certificateCopy]()->Dialog::CheckResult
							{
								return Config::certificate == certificateCopy ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
							}
						);

					}
				}


				LogEvent(EventType::SessionRelated,nullptr,"Created new account "+Config::accountName);
				LogEvent(EventType::SessionRelated,nullptr,"Disconnecting client to try authenticating with new certificate");
				client.Disconnect();
				Authenticate();
			}

			void	Authenticated(Protocol::AccountDetails&acc)
			{
				LogEvent(EventType::SessionRelated,nullptr,"Authenticated as "+acc.accountName);
			}

			void	BadAccountName(Package::NetString<256>&msg)
			{
				LogEvent(EventType::SessionRelated,nullptr,"Bad account name "+msg);
			}


		}



		void	Authenticate()
		{
			int rc = mbedtls_x509_crt_parse_der(&loadedCertificate,Config::certificate.pointer(),Config::certificate.GetContentSize());
			if (rc != 0)
			{
				LogEvent(EventType::InternalFault,nullptr,"mbedtls_x509_crt_parse returned "+ErrorToString(rc));
				LogEvent(EventType::InternalFault,nullptr,"Closing down consensus link");
				Disconnect();
				return;
			}

			String password = "hello world";

			StringBuffer pemBuilder;
			MakePEM(pemBuilder,Config::certificateKey,password.c_str(),&secureRandom);


				
			//theoretically could parse binary pem here, but mbedtls does not expose vital functions for that. i could expose them manually, or implement an adequate wrapper function in mbedtls,
			//but for the sake of maintainability, and due to low performance loss because of one-time conversion overhead, it might be better to persistently use this detour instead
			rc = mbedtls_pk_parse_key(&loadedCertificateKey,(const unsigned char*)pemBuilder.pointer(),pemBuilder.GetFillLevel(),(const BYTE*)password.c_str(),password.length());
			if (rc != 0)
			{
				LogEvent(EventType::InternalFault,nullptr,"mbedtls_pk_parse_key returned "+ErrorToString(rc));
				LogEvent(EventType::InternalFault,nullptr,"Closing down consensus link");
				Disconnect();
				return;
			}


			//LogEvent(EventType::SessionRelated,nullptr,"Certificate loaded: "+Bin2Str::Interpret(certificate.pointer(),certificate.GetContentSize()));


			{
				LogEvent(EventType::SessionRelated,nullptr,"Setting local certificate");
				int ret = mbedtls_ssl_conf_own_cert(&config,&loadedCertificate,&loadedCertificateKey);
				if (ret != 0)
				{
					LogEvent(EventType::InternalFault,nullptr,"mbedtls_ssl_conf_own_cert returned "+ErrorToString(ret));
					LogEvent(EventType::InternalFault,nullptr,"Closing down consensus link");
					Disconnect();
				}
			}
		}



	}


	void		Create()
	{
		Detail::Create();
		if (Detail::Config::connectOnCreate)
		{
			Detail::Reconnect();
		}
	}

	bool	IsCreated()
	{
		return Detail::created;
	}

	void		ConnectNow()
	{
		if (Detail::created && !Detail::connect && !Detail::client.IsAttemptingToConnect() && !Detail::client.IsConnected())
			Detail::Reconnect();
	}

	void		Disconnect()
	{
		Detail::Disconnect();
	}

	void		Destroy()
	{
		Detail::Destroy();
	}
	
	bool		IsConnectingOrConnected()
	{
		return Detail::connect;
	}

	bool		StillTryingToConnect()
	{
		return Detail::created && Detail::connect && !Detail::client.IsConnected();
	}

	void		Resolve()
	{
		if (!Detail::connect)
			return;
		if (!Detail::client.IsAttemptingToConnect() && !Detail::client.IsConnected() && timer.GetSecondsSince(Detail::lastConnectionAttempt) > 5.f)
		{
			Detail::Reconnect();
		}

		Detail::client.Resolve();
	}
	
	bool		IsConnected()
	{
		return Detail::client.IsConnected();
	}




	void		Configure(const String&host, bool connectOnCreate, bool askToEncryptPrivateKey)
	{
		Detail::Config::lastHost = host;
		Detail::Config::connectOnCreate = connectOnCreate;
		Detail::Config::askToEncryptPrivateKey = askToEncryptPrivateKey;
		Detail::Config::certificateFolder = FileSystem::Folder(FileSystem::CreateDirectory("./config/certificates/"+TCP::StripPort(host)));
	}






		


}