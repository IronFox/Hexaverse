#include <global_root.h>

#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#include <mbedtls/base64.h>
#include <net/tcp.h>

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
					Hello,				//c2s : UINT32 protocol version -> [if authenticated] s2c : (Authenticated)Protocol.Interface.AccountDetails
					RequestNewAccount,	//c2s : <signal> : request new account creation -> s2c : (CreatedNewAccount)Protocol.Interface.NewAccount, s2c : (Authenticated)Protocol.Interface.AccountDetails
					CreatedNewAccount,	//s2c : Protocol.Interface.NewAccount
					Authenticated,		//s2c : Protocol.Interface.AccountDetails
					SetAccountName,		//c2s : string -> [if valid] s2c : (Authenticated)Protocol.Interface.AccountDetails, [otherwise] s2c : (BadNickname)string
					BadAccountName,		//s2c : string

					Count
				};
			}

			static const UINT32 ContentVersion = 11;
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





		//mbedtls_net_context netContext;
		mbedtls_ssl_config config;
		mbedtls_entropy_context entropy;
		mbedtls_ctr_drbg_context ctr_drbg;
		//mbedtls_pk_info_t	pkInfo;

		TCP::Client		client;


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

		void	Destroy()
		{
			client.Disconnect();
			mbedtls_ssl_config_free( &config );
			mbedtls_ctr_drbg_free( &ctr_drbg );
			mbedtls_entropy_free( &entropy );
		}


		class SocketAccess : public TCP::SocketAccess
		{
		public:
			mbedtls_x509_crt		certificate;

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
				bool exp = false;
				if (isDestroyed.compare_exchange_strong(exp,true))
				{
					mbedtls_ssl_free( &ssl );
					mbedtls_net_free( &context );
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
				return (int)dataSize;
			}
		};

		void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
		}

		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventType::SessionRelated,nullptr,"Consensus: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
					Channels::hello.SendTo(client,Protocol::ProtocolVersion);
					client.SendSignal(Protocol::ChannelID::RequestNewAccount);
				break;

			}
		}

		void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
		}

		void OnSignal(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Signal on channel "+String(channel));
		}


		Timer::Time	lastConnectionAttempt = timer.Now();
		String		lastHost;

		void	Reconnect()
		{
			lastConnectionAttempt = timer.Now();
			client.ConnectAsync(lastHost);
		}


		void	Init(const String&host)
		{
			mbedtls_debug_set_threshold(1);
			mbedtls_ssl_config_init( &config );
			mbedtls_entropy_init( &entropy );
			//mbedtls_x509_crt_init( &cacert );
			mbedtls_ctr_drbg_init( &ctr_drbg );


			int ret;
			if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
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

			mbedtls_ssl_conf_rng( &config, mbedtls_ctr_drbg_random, &ctr_drbg );
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


			lastHost = host;
			Reconnect();
		}

		namespace Receiver
		{
			void	CreatedNewAccount(Protocol::NewAccount&acc)
			{
				mbedtls_x509_crt cert;
				mbedtls_x509_crt_init( &cert );

				mbedtls_pk_context		certificateKey;
				mbedtls_pk_init(&certificateKey);

				
				int rc = mbedtls_x509_crt_parse_der(&cert,acc.certificate.pointer(),acc.certificate.GetContentSize());
				LogEvent(EventType::SessionRelated,nullptr,"Got certificate for new account "+acc.details.accountName+". mbedtls_x509_crt_parse returned "+ErrorToString(rc));


				StringBuffer pemBuilder;
				pemBuilder << "-----BEGIN RSA PRIVATE KEY-----";

				Array<BYTE>	base64(acc.certificateKey.GetContentSize()*2);

				//const BYTE*bytes = acc.certificateKey.pointer();
				size_t written = 0;
				rc = mbedtls_base64_encode( base64.pointer(), base64.GetContentSize(), &written, acc.certificateKey.pointer(), acc.certificateKey.GetContentSize() );

				for (index_t i = 0; i < written; i++)
				{
					if (!(i%64))
						pemBuilder << nl;

					pemBuilder << (char)base64[i];
				}
				if (!(written % 64))
					pemBuilder << nl;

				pemBuilder << "-----END RSA PRIVATE KEY-----" << nl;

				String pem = pemBuilder.ToStringRef();


				
				/*
				-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFDjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIS2qgprFqPxECAggA
MBQGCCqGSIb3DQMHBAgD1kGN4ZslJgSCBMi1xk9jhlPxP3FyaMIUq8QmckXCs3Sa
9g73NQbtqZwI+9X5OhpSg/2ALxlCCjbqvzgSu8gfFZ4yo+Xd8VucZDmDSpzZGDod
A .... MANY LINES LIKE THAT .... .... MANY LINES LIKE THAT .... 
X0R+meOaudPTBxoSgCCM51poFgaqt4l6VlTN4FRpj+c/WZeoMM/BVXO+nayuIMyH
blK948UAda/bWVmZjXfY4Tztah0CuqlAldOQBzu8TwE7WDwo5S7lo5u0EXEoqCCq
H0ga/iLNvWYexG7FHLRiq5hTj0g9mUPEbeTXuPtOkTEb/0ckVE2iZH9l7g5edmUZ
GEs=
-----END ENCRYPTED PRIVATE KEY-----
				*/



				rc = mbedtls_pk_parse_key(&certificateKey,(const unsigned char*)pem.c_str(),pem.length()+1,nullptr,0);
				LogEvent(EventType::SessionRelated,nullptr,"Got private key. mbedtls_pk_parse_key returned "+ErrorToString(rc));


				LogEvent(EventType::SessionRelated,nullptr,"Certificate is "+Bin2Str::Interpret(acc.certificate.pointer(),acc.certificate.GetContentSize()));

				mbedtls_x509_crt_free(&cert);
				mbedtls_pk_free(&certificateKey);
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



	}


	void		Create(const String&host)
	{
		Detail::Init(host);

	}
	void		Destroy()
	{
		Detail::Destroy();
	}
	
	void		Resolve()
	{
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

}