#pragma once

#include <io/cryptographic_hash.h>
#include "uniqueID.h"
namespace Password
{
	typedef SHA256::HashContainer	Hashed;

	void	Hash(const CryptographicID&salt, const ArrayRef<const BYTE>&password, Hashed&rs);
	bool	HashMatch(const CryptographicID&salt, const ArrayRef<const BYTE>&password, const Hashed&compareToHashed);
	inline void	Hash(const CryptographicID&salt, const String&password, Hashed&rs)
	{
		Hash(salt,ArrayRef<const BYTE>((const BYTE*)password.c_str(),password.GetLength()),rs);
	}
	inline void	Hash(const CryptographicID&salt, const CryptographicID&passwordToken, Hashed&rs)
	{
		Hash(salt,ArrayRef<const BYTE>(passwordToken.GetBytes(),passwordToken.NumBytes),rs);
	}

	inline bool HashMatch(const CryptographicID&salt, const CryptographicID&passwordToken, const Hashed&compareToHashed)
	{
		return passwordToken.IsSet() && HashMatch(salt,ArrayRef<const BYTE>(passwordToken.GetBytes(),passwordToken.NumBytes),compareToHashed);
	}


};

