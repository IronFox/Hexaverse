#ifndef netEncryptionHelperH
#define netEncryptionHelperH

#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#include <mbedtls/base64.h>
#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

#include <global_string.h>
#include <string/string_buffer.h>


namespace Encryption
{

	void	MakePEM(StringBuffer&, const Array<BYTE>&binaryPemRSAKey, const char*password, mbedtls_ctr_drbg_context*ctx);


	class AES
	{
	public:
		struct InitializationVector
		{
			static const size_t NumBytes = 16;
			BYTE			bytes[NumBytes];
		};
	private:
		mbedtls_aes_context aes_ctx;
		unsigned char		aes_key[32];
		size_t				aes_keylen_bytes=32;
		bool				randomIV=true,
							anyIV=false,
							pwdSet=false;
		int					lastDirection=-1;
		InitializationVector	iv;


		void				MunchPWD(const void*pwd, size_t pwdlen, void*aes_key);
		void				Crypt(void*data, size_t dataSize, int direction);
		void				Crypt(const void*sourceData, void*destData, size_t dataSize, int direction);

	public:
		static void			Wipe(void*ptr, size_t size)
		{
			volatile BYTE *p = (volatile BYTE*)ptr;
			while ( size-- )
				*p++ = 0;
		}


		/**/	AES(size_t internalKeyLenBytes=32):aes_keylen_bytes(internalKeyLenBytes)
		{
			mbedtls_aes_init( &aes_ctx );
		}

		virtual ~AES()
		{
			mbedtls_aes_free( &aes_ctx );
			Wipe( aes_key, ARRAYSIZE(aes_key) );
		}

		static void		ExpandToCompatibleSize(Array<BYTE>&field)
		{
			if (!(field.GetContentSize()%16))
				return;
			count_t blocks = field.GetContentSize()/16 + 1;
			field.resizePreserveContent(blocks * 16);
		}

		void		Decrypt(const Array<BYTE>&source, Array<BYTE>&dest)
		{
			ASSERT__(anyIV);
			ASSERT__(pwdSet);
			ASSERT__(!randomIV);
			dest.SetSize(source.GetLength());
			if (lastDirection != MBEDTLS_AES_DECRYPT)
			{
				mbedtls_aes_setkey_dec( &aes_ctx, aes_key, aes_keylen_bytes * 8 );
				lastDirection = MBEDTLS_AES_DECRYPT;
			}
			Crypt(source.pointer(),dest.pointer(),dest.GetContentSize(),MBEDTLS_AES_DECRYPT);
		}

		void		Encrypt(const Array<BYTE>&source, Array<BYTE>&dest)
		{
			ASSERT__(anyIV);
			ASSERT__(pwdSet);

			if (!(source.GetContentSize()%16))
			{
				dest.SetSize(source.GetLength());
				if (lastDirection != MBEDTLS_AES_ENCRYPT)
				{
					lastDirection = MBEDTLS_AES_ENCRYPT;
					mbedtls_aes_setkey_enc( &aes_ctx, aes_key, aes_keylen_bytes * 8 );
				}
				Crypt(source.pointer(),dest.pointer(),dest.GetContentSize(),MBEDTLS_AES_ENCRYPT);
				return;
			}
			size_t tempSize = ((source.GetContentSize()/16)+1)*16;
			ASSERT_GREATER__(tempSize,source.GetContentSize());
			Array<BYTE>	temp(tempSize);
			temp.Fill(0);
			temp.copyFrom(source.pointer(),source.GetLength());
			dest.SetSize(tempSize);
			if (lastDirection != MBEDTLS_AES_ENCRYPT)
			{
				lastDirection = MBEDTLS_AES_ENCRYPT;
				mbedtls_aes_setkey_enc( &aes_ctx, aes_key, aes_keylen_bytes * 8 );
			}
			Crypt(temp.pointer(),dest.pointer(),dest.GetContentSize(),MBEDTLS_AES_ENCRYPT);
			Wipe(temp.pointer(),temp.GetLength());
		}


		void		Decrypt(void*data, size_t dataSize)
		{
			ASSERT__(anyIV);
			ASSERT__(pwdSet);
			ASSERT__(!randomIV);
			if (lastDirection != MBEDTLS_AES_DECRYPT)
			{
				mbedtls_aes_setkey_dec( &aes_ctx, aes_key, aes_keylen_bytes * 8 );
				lastDirection = MBEDTLS_AES_DECRYPT;
			}
			Crypt(data,dataSize,MBEDTLS_AES_DECRYPT);
		}
		void		Encrypt(void*data, size_t dataSize)
		{
			ASSERT__(anyIV);
			ASSERT__(pwdSet);
			if (lastDirection != MBEDTLS_AES_ENCRYPT)
			{
				lastDirection = MBEDTLS_AES_ENCRYPT;
				mbedtls_aes_setkey_enc( &aes_ctx, aes_key, aes_keylen_bytes * 8 );
			}
			Crypt(data,dataSize,MBEDTLS_AES_ENCRYPT);
		}

		const InitializationVector&		GetIV()	const
		{
			ASSERT__(anyIV);
			return iv;
		}

		void	RandomizeIV(mbedtls_ctr_drbg_context*ctx);
		void	SetIV(const InitializationVector&iv)
		{
			ASSERT__(!pwdSet);
			randomIV = false;
			anyIV = true;
			this->iv = iv;
		}

		void	SetPWD(const void*keyData, size_t keySize)
		{
			ASSERT__(anyIV);
			pwdSet = true;
			//BYTE aes_key[32];
			MunchPWD(keyData,keySize,aes_key);
		}


		void	SetPWD(const String&key)
		{
			SetPWD(key.c_str(),key.length());
		}
		



	};




}



#endif
