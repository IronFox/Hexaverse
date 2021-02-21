#include "passwordMatch.h"

namespace Password
{
	void	Hash(const CryptographicID&salt, const ArrayRef<const BYTE>&password, Hashed&rs)
	{
		SHA256 sha;
		sha.Append(salt.GetBytes(),salt.NumBytes);
		sha.Append(password.GetPointer(),password.GetLength());
		sha.Finish(rs);
	}

	bool	HashMatch(const CryptographicID&salt, const ArrayRef<const BYTE>&password, const Hashed&compareToHashed)
	{
		Hashed hashed;
		Hash(salt,password,hashed);
		return hashed == compareToHashed;
	}

}
