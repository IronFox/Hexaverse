#include <global_root.h>
#include "encryptionHelper.h"


namespace Encryption
{

	void				MakePEM(StringBuffer&pemBuilder, const Array<BYTE>&binaryPemRSAKey, const char*password,mbedtls_ctr_drbg_context*ctx)
	{
		pemBuilder.Clear();

		pemBuilder << "-----BEGIN RSA PRIVATE KEY-----";

		Array<BYTE>	copy;
		const Array<BYTE>*source = &binaryPemRSAKey;

		if (password && password[0])
		{
			pemBuilder << nl<<"Proc-Type: 4,ENCRYPTED"<< nl;
			pemBuilder << "DEK-Info: AES-256-CBC,";

			AES	aes;
			aes.RandomizeIV(ctx);
			//AES::InitializationVector vec;
			//for (index_t i = 0; i < vec.NumBytes; i++)
			//	vec.bytes[i] = (BYTE)i;
			//aes.SetIV(vec);
			aes.SetPWD(password,strlen(password));
			if (source->GetLength() % 16)
			{
				count_t blocks = source->GetLength()/16 + 1;
				copy.SetSize(blocks*16);
				copy.Fill(0);
				copy.copyFrom(source->pointer(),source->GetLength());
			}
			else
				copy = *source;
			source = &copy;
			aes.Encrypt(copy.pointer(),copy.GetContentSize());


			pemBuilder << BinaryToString(aes.GetIV().bytes,AES::InitializationVector::NumBytes);

			pemBuilder << nl;

		}


		size_t written = 0;
		int rc;

		rc = mbedtls_base64_encode( nullptr, 0, &written, source->pointer(), source->GetContentSize() );

		Array<BYTE>	base64(written);

		//const BYTE*bytes = acc.certificateKey.pointer();
		written = 0;
		rc = mbedtls_base64_encode( base64.pointer(), base64.GetContentSize(), &written, source->pointer(), source->GetContentSize() );

		for (index_t i = 0; i < written; i++)
		{
			if (!(i%64))
				pemBuilder << nl;

			pemBuilder << (char)base64[i];
		}
		if (!(written % 64))
			pemBuilder << nl;

		pemBuilder << "-----END RSA PRIVATE KEY-----" << nl;
		pemBuilder << '\0';	//mbedtls can be very stupid

	}

	void				AES::Crypt(void*data, size_t dataSize, int direction)
	{
		if (!(dataSize%16))
		{
			InitializationVector tempIV = this->iv;
			//unsigned char		iv[ARRAYSIZE(aes_iv)];
			//memcpy(iv,aes_iv,ARRAYSIZE(aes_iv));
			int rc = mbedtls_aes_crypt_cbc( &aes_ctx, direction, dataSize,
							tempIV.bytes, (const unsigned char*)data, (unsigned char*)data );
			MakeZero(tempIV.bytes,InitializationVector::NumBytes);
			ASSERT__(rc==0);
			return;
		}
		size_t tempSize = ((dataSize/16)+1)*16;
		ASSERT_GREATER__(tempSize,dataSize);
		Array<BYTE>	temp(tempSize);
		temp.Fill(0);
		memcpy(temp.pointer(),data,dataSize);
		Crypt(temp.pointer(),tempSize,direction);
		memcpy(data,temp.pointer(),dataSize);
		MakeZero(temp.pointer(),temp.GetLength());
	}




	
	void				AES::RandomizeIV(mbedtls_ctr_drbg_context*ctx)
	{
		randomIV = true;
		anyIV = true;
		mbedtls_ctr_drbg_random(ctx,iv.bytes,InitializationVector::NumBytes);

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
		mbedtls_md5_update( &md5_ctx, iv.bytes,  8 );
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
		mbedtls_md5_update( &md5_ctx, iv.bytes,  8 );
		mbedtls_md5_finish( &md5_ctx, md5sum );

		use_len = 16;
		if( aes_keylen_bytes < 32 )
			use_len = aes_keylen_bytes - 16;

		memcpy( ((BYTE*)aes_key) + 16, md5sum, use_len );

		mbedtls_md5_free( &md5_ctx );
		MakeZero( md5sum, 16 );
	}

}
