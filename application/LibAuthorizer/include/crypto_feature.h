#pragma once

#include "comm_structure.h"

#include <boost/uuid/uuid.hpp>

#include <cryptopp/hex.h>
#include <cryptopp/rsa.h>
#include <cryptopp/base32.h>
#include <cryptopp/base64.h>
#include <cryptopp/aes.h>

typedef BYTE SHA1_OUT[CryptoPP::SHA1::DIGESTSIZE];	// Master Key指纹 = public key的SHA1
typedef BYTE AES_KEY[CryptoPP::AES::DEFAULT_KEYLENGTH];

namespace crypto
{
	// 算法
	static void Sha1(SHA1_OUT& out, BYTE* in, size_t in_len);
	template <class ALGORITHM>
	void HexEncode(std::string& out, const BYTE* in, size_t in_len, int group, const std::string& separator)
	{
		JCASSERT(in);
		out.clear();
		CryptoPP::StringSink* sink = new CryptoPP::StringSink(out);
		ALGORITHM encoder(sink, true, group, separator);
		encoder.Put(in, in_len);
		encoder.MessageEnd();
	}
	template <class ALGORITHM>
	void HexEncode(std::string& out, const BYTE* in, size_t in_len)
	{
		JCASSERT(in);
		out.clear();
		CryptoPP::StringSink* sink = new CryptoPP::StringSink(out);
		ALGORITHM encoder(sink, true);
		encoder.Put(in, in_len);
		encoder.MessageEnd();
	}

	// 返回转换的字节数
	template <class ALGORITHM>
	size_t HexDecode(const std::string& src, BYTE* out, size_t buf_len)
	{
		JCASSERT(out);
		CryptoPP::StringSource source(src, true);
		CryptoPP::ArraySink* sink = new CryptoPP::ArraySink(out, buf_len);
		ALGORITHM decoder(sink);
		source.TransferTo(decoder);
		decoder.MessageEnd();
		return buf_len - sink->AvailableSize();
	}

	//static void Base32Encode(std::string& out, const BYTE* buf, size_t len, int group, const std::string& separator);
	//static void Base32Decode(const std::string& src, BYTE* dst, size_t len);

	// 从data_len位置开始填入随机数，到一个加密的块大小。返回最终大小
	size_t FillSalt(BYTE* buf, size_t data_len, size_t block_size, size_t buf_len);

	void LoadPublicKey(CryptoPP::RSA::PublicKey& key, const BYTE* buf, size_t len);
	void PrivateKeyToString(std::string& out, const CryptoPP::RSA::PrivateKey& key);
	void PrivateKeyFromString(CryptoPP::RSA::PrivateKey& key, const std::string& in);
	void AESEncrypt(BYTE* cihper_buf, size_t ciher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key);
	void AESDecrypt(const BYTE* cihper_buf, size_t ciher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key);

	void RSAEncrypt(BYTE* cihper_buf, size_t cihper_len, const BYTE* plane_buf, size_t plane_len, CryptoPP::RSA::PublicKey& key);
	void RSADecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, CryptoPP::RSA::PrivateKey& key);

	void CBCAESEncrypt(BYTE* cihper_buf, size_t ciher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key);
	void CBCAESDecrypt(const BYTE* cihper_buf, size_t ciher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key);
	DWORD CalculateCrc(BYTE* buf, size_t len);

	void SetStringId(const std::string& string_id, boost::uuids::uuid& uuid, DWORD& id);

	// 其他转换
	UINT64 ComputerNameHash(const std::wstring& name);

}