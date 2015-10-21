#include <global_root.h>

#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <net/tcp.h>

#include "../../../shared/logEvent.h"


namespace ConsensusLink
{
	namespace Detail
	{
		//mbedtls_net_context netContext;
		mbedtls_ssl_config config;
		mbedtls_entropy_context entropy;
		mbedtls_x509_crt cacert;
		mbedtls_ctr_drbg_context ctr_drbg;


		void Debug( void *ctx, int level, const char *file, int line, const char *str )
		{
			((void) level);
			_CRT_UNUSED(ctx);
			LogEvent(EventType::Debug,nullptr,"In "+String(file)+", line "+String(line)+": '"+str+"'");

			//fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
			//fflush(  (FILE *) ctx  );
		}

		void	Init()
		{
			mbedtls_ssl_config_init( &config );
			mbedtls_entropy_init( &entropy );
			mbedtls_x509_crt_init( &cacert );
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


		}

		void	Destroy()
		{
			mbedtls_ssl_config_free( &config );
			mbedtls_ctr_drbg_free( &ctr_drbg );
			mbedtls_entropy_free( &entropy );
		}


		class SocketAccess : public TCP::SocketAccess
		{
		public:
			mbedtls_net_context		context;
			mbedtls_ssl_context		ssl;
			std::atomic<bool>		isDestroyed = false;

			/**/					SocketAccess()
			{
				mbedtls_net_init( &context );
				mbedtls_ssl_init(&ssl);
			}
			virtual					~SocketAccess()
			{
				CloseSocket();
			};
			virtual void			SetSocket(SOCKET socket) override
			{
				context.fd = (int)socket;
				int ret;
				if( ( ret = mbedtls_ssl_set_hostname( &ssl, "Consensus Host" ) ) != 0 )
					throw IO::Network::GeneralFault( "mbedtls_ssl_set_hostname returned "+String(ret));
				mbedtls_ssl_set_bio( &ssl, &context, mbedtls_net_send, mbedtls_net_recv, NULL );
				for (;;)
				{
					ret = mbedtls_ssl_handshake(&ssl);

					if (ret == 0)
						break;
					if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
						continue;

					throw IO::Network::GeneralFault( "mbedtls_ssl_handshake returned "+String(ret));
				}


				const mbedtls_x509_crt* cert = mbedtls_ssl_get_peer_cert(&ssl);
				if (cert != nullptr)
				{
					const void*const certData = cert->raw.p;
					const size_t certSize = cert->raw.len;


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

	}


	void		Create()
	{
		Detail::Init();

	}
	void		Destroy()
	{
		Detail::Destroy();
	}
	
	void		Resolve()
	{

	}
	
	bool		IsConnected()
	{
		return false;
	}

}