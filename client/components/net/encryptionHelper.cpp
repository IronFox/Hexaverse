#include <global_root.h>
#include "encryptionHelper.h"


namespace Encryption
{
	
	void				AES::RandomizeIV(mbedtls_ctr_drbg_context*ctx)
	{
		randomIV = true;
		anyIV = true;
		mbedtls_ctr_drbg_random(ctx,aes_iv,ARRAYSIZE(aes_iv));

	}



	void				AES::MunchPWD(const void*pwd, size_t pwdlen, void*aes_key)
	{
			//static void pem_pbkdf1( unsigned char *key, size_t keylen,
			//						unsigned char *iv,
			//						const unsigned char *pwd, size_t pwdlen )
			//{
		mbedtls_md5_context md5_ctx;
		unsigned char md5sum[16];
		size_t use_len;

		mbedtls_md5_init( &md5_ctx );

		/*
			* key[ 0..15] = MD5(pwd || IV)
			*/
		mbedtls_md5_starts( &md5_ctx );
		mbedtls_md5_update( &md5_ctx, (const BYTE*)pwd, pwdlen );
		mbedtls_md5_update( &md5_ctx, aes_iv,  8 );
		mbedtls_md5_finish( &md5_ctx, md5sum );

		if( aes_keylen_bytes <= 16 )
		{
			memcpy( aes_key, md5sum, aes_keylen_bytes );

			mbedtls_md5_free( &md5_ctx );
			MakeZero( md5sum, 16 );
			return;
		}

		memcpy( aes_key, md5sum, 16 );

		/*
			* key[16..23] = MD5(key[ 0..15] || pwd || IV])
			*/
		mbedtls_md5_starts( &md5_ctx );
		mbedtls_md5_update( &md5_ctx, md5sum,  16 );
		mbedtls_md5_update( &md5_ctx, (const BYTE*)pwd, pwdlen );
		mbedtls_md5_update( &md5_ctx, aes_iv,  8 );
		mbedtls_md5_finish( &md5_ctx, md5sum );

		use_len = 16;
		if( aes_keylen_bytes < 32 )
			use_len = aes_keylen_bytes - 16;

		memcpy( ((BYTE*)aes_key) + 16, md5sum, use_len );

		mbedtls_md5_free( &md5_ctx );
		MakeZero( md5sum, 16 );
	}

}
