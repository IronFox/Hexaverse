#ifndef uniqueIDH
#define uniqueIDH

#include <global_string.h>
#include "niceName.h"

namespace Bin2Str
{

	inline void _ToHex(char&out, BYTE v)
	{
		if (v < 10)
			out = '0' + v;
		else
			out = 'a' + (v - 10);
	}

	inline BYTE _ToByte(char c)
	{
		if (c >= '0' && c <= '9')
			return (BYTE)(c - '0');
		c = Template::toupper(c);
		if (c >= 'A' && c <= 'F')
			return (BYTE)(10 + (c - 'A'));
		throw Except::Program::ParameterFault("Unable to decode byte-string character '" + String(c) + "': Invalid hexadecimal character");
	}

	inline void _ByteToHex(char*&out, BYTE v)
	{
		_ToHex(*out++, v>>4);
		_ToHex(*out++, v&0xF);
	}

	inline BYTE _HexToByte(char c0, char c1)
	{
		return (_ToByte(c0) << 4) | _ToByte(c1);
	}

	inline String Convert(const void*data, size_t dataSize)
	{
		String result(TStringLength(dataSize*2));
		char*out = result.mutablePointer();
		for (index_t i = 0; i < dataSize; i++)
			_ByteToHex(out,((const BYTE*)data)[i]);
		return result;
	}
	inline String Convert(const Array<BYTE>&v)
	{
		return Convert(v.pointer(),v.size());
	}
	inline void Convert(void*outData, size_t outDataSize, const char*source, size_t sourceLen)
	{
		if (sourceLen != outDataSize * 2)
		{
			throw Except::Program::ParameterFault("Unable to decode byte-string '" + String(source,sourceLen) + "': Invalid length (" + String(sourceLen) + "). Expected "+String(outDataSize*2)+" characters");
		}
		BYTE*outBytes = (BYTE*)outData;
		for (index_t i = 0; i < outDataSize; i++)
		{
			char a0 = source[i * 2];
			char a1 = source[i * 2 + 1];
			outBytes[i] = _HexToByte(a0, a1);
		}
	}

	inline void Convert(void*outData, size_t outDataSize, const String&source)
	{
		Convert(outData,outDataSize,source.c_str(),source.length());
	}

}


struct TUniqueID
{
	static const count_t NumBytes = 16;
	BYTE		bytes[NumBytes];

	static hash_t	hash(const TUniqueID&id)
	{
		return stdMemHash(id.bytes,sizeof(id.bytes));
	}

	String		ToFullString() const
	{
		return Bin2Str::Convert(bytes,NumBytes);
	}

	String		ToString(count_t numBytes=4) const
	{
		return Bin2Str::Convert(bytes,numBytes);
	}

	String		ToNiceString() const
	{
		static_assert(NumBytes >= sizeof(UINT64),"Revise logic");
		return NiceName::Get(*(const UINT64*)bytes);
	}

	bool		operator==(const TUniqueID&other)	const
	{
		return !memcmp(bytes,other.bytes,sizeof(bytes));
	}
	bool		operator!=(const TUniqueID&other)	const
	{
		return 0 != memcmp(bytes,other.bytes,sizeof(bytes));
	}
	int			CompareTo(const TUniqueID&other) const
	{
		return memcmp(bytes,other.bytes,NumBytes);
	}
	bool		operator<(const TUniqueID&other) const
	{
		return CompareTo(other) < 0;
	}
	bool		operator<=(const TUniqueID&other) const
	{
		return CompareTo(other) <= 0;
	}
	bool		operator>(const TUniqueID&other) const
	{
		return CompareTo(other) > 0;
	}
	bool		operator>=(const TUniqueID&other) const
	{
		return CompareTo(other) >= 0;
	}

	void		Generate()
	{
		UUID	uuid;
		UuidCreate(&uuid);
		memcpy(bytes,&uuid,sizeof(UUID));
	}

	static TUniqueID	Empty()
	{
		TUniqueID rs;
		memset(rs.bytes,0,sizeof(rs.bytes));
		return rs;
	}

};
static_assert(sizeof(TUniqueID) == 16, "Struct packing not appropriate for unique ID");


class CryptographicID
{
	BYTE	bytes[32];

public:
	/**/	CryptographicID()
	{
		memset(bytes,0,sizeof(bytes));
	}

	bool	IsSet() const
	{
		for (index_t i = 0; i < ARRAYSIZE(bytes); i++)
			if (bytes[i])
				return true;
		return false;
	}
	bool	IsEmpty() const {return !IsSet();}

	void	Generate()
	{
		static HCRYPTPROV provider = NULL;
		if (provider == NULL)
			ASSERT1__(CryptAcquireContext (&provider,NULL,NULL,PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT),GetLastError());
		ASSERT__(CryptGenRandom(provider,ARRAYSIZE(bytes),bytes));
	}

	void	Write(FILE*f) const
	{
		size_t written = fwrite(bytes,sizeof(bytes),1,f);
		ASSERT_EQUAL__(written,1);
	}

	void	Read(FILE*f)
	{
		size_t read = fread(bytes,sizeof(bytes),1,f);
		ASSERT_EQUAL__(read,1);
	}

	static hash_t	hash(const CryptographicID&id)
	{
		return stdMemHash(id.bytes,sizeof(id.bytes));
	}

	String		ToString(count_t numBytes=4) const
	{
		return Bin2Str::Convert(bytes,/*sizeof(bytes)*/numBytes);
	}

	String		ToFullString() const
	{
		return Bin2Str::Convert(bytes, sizeof(bytes));
	}

	void		Decode(const String&source)
	{
		Bin2Str::Convert(bytes, sizeof(bytes), source);


	}

	bool		operator==(const CryptographicID&other)	const
	{
		return !memcmp(bytes,other.bytes,sizeof(bytes));
	}
	bool		operator!=(const CryptographicID&other)	const
	{
		return 0 != memcmp(bytes,other.bytes,sizeof(bytes));
	}

};




#endif
