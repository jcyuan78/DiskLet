#include "pch.h"

#include "../include/crypto_feature.h"

#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_hash.hpp>

#include <cryptopp/crc.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>

LOCAL_LOGGER_ENABLE(L"crypto", LOGGER_LEVEL_NOTICE);


using namespace crypto;

void crypto::Sha1(SHA1_OUT& out, BYTE* in, size_t in_len)
{
	JCASSERT(in);
	CryptoPP::SHA1 hash;
	hash.Update(in, in_len);
	hash.Final(out);
}

size_t crypto::FillSalt(BYTE* buf, size_t data_len, size_t block_size, size_t buf_len)
{
	JCASSERT(data_len > 0);
	srand(boost::numeric_cast<UINT>(time(0)));
//	size_t final_size = ((data_len - 1) / block_size + 1) * block_size;
	size_t final_size = data_len + (block_size - (data_len - 1) % block_size)-1;
	if (final_size > buf_len) THROW_ERROR(ERR_APP, L"buffer size (%lld) smaller then necessary (%lld)", buf_len, final_size);
	for (size_t ii = data_len; ii < final_size; ++ii)
	{
		buf[ii] = (BYTE)(rand() & 0xFF);
	}
	return final_size;
}

void crypto::LoadPublicKey(CryptoPP::RSA::PublicKey& key, const BYTE* buf, size_t len)
{
//	CryptoPP::ArraySource source(buf, len);
	CryptoPP::ByteQueue que;

//	source.TransferTo(que);
	que.Put(buf, len);
	que.MessageEnd();
	key.Load(que);
}

void crypto::PrivateKeyToString(std::string& str, const CryptoPP::RSA::PrivateKey& key)
{
	//	jcvos::auto_array<BYTE> key_buf(1024, 00);
	//	CryptoPP::ArraySink sink(key_buf, 1024);
	//	key.Save(sink);
	//	size_t key_size = 1024 - sink.AvailableSize();
	//	LOG_DEBUG(L"key size = %zd", key_size);

	CryptoPP::ByteQueue que;
	key.Save(que);

	//	StringSink str_sink(str);
	CryptoPP::Base64Encoder encoder(new CryptoPP::StringSink(str));
	//	key_size = encoder.Put(key_buf, key_size);
	que.CopyTo(encoder);
	encoder.MessageEnd();

	//.CopyTo(str_sink);
	//str_sink.MessageEnd();
}

void crypto::PrivateKeyFromString(CryptoPP::RSA::PrivateKey& key, const std::string& str)
{
	// string to 
	CryptoPP::StringSource source(str, true);
	CryptoPP::Base64Decoder decoder;
	source.TransferTo(decoder);
	decoder.MessageEnd();

	CryptoPP::ByteQueue que;
	decoder.CopyTo(que);
	key.Load(que);
}

void crypto::AESEncrypt(BYTE* cipher_buf, size_t cipher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	JCASSERT(plane_len <= CryptoPP::AES::BLOCKSIZE);
	JCASSERT(cipher_len >= CryptoPP::AES::BLOCKSIZE);
	//	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(cipher_buf, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Encryption encryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	memcpy_s(cipher_buf, CryptoPP::AES::BLOCKSIZE, plane_buf, plane_len);
	encryption.ProcessBlock(cipher_buf);

	//BYTE vv[16];
	//memcpy_s(vv, 16, iv, 16);
	//CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	//decryption.ProcessBlock(vv);
}

void crypto::AESDecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	JCASSERT(plane_len >= CryptoPP::AES::BLOCKSIZE);
	JCASSERT(cipher_len <= CryptoPP::AES::BLOCKSIZE);
	//BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(plane_buf, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	memcpy_s(plane_buf, plane_len, cipher_buf, cipher_len);
	decryption.ProcessBlock(plane_buf);
}

void crypto::RSAEncrypt(BYTE* cipher_buf, size_t cipher_len, const BYTE* plane_buf, size_t plane_len, CryptoPP::RSA::PublicKey& key)
{
	JCASSERT(cipher_len == plane_len);
	size_t fixed_len = key.MaxImage().ByteCount();
//	key.GetValue<size_t>("KeySize", fixed_len);
	LOG_DEBUG(L"size of decryption = %zd", fixed_len);

	CryptoPP::AutoSeededRandomPool rng;
	const BYTE* in_buf = plane_buf;
	BYTE* out_buf = cipher_buf;
	size_t remain = plane_len;

	while (remain > 0)
	{
		if (remain < fixed_len) THROW_ERROR(ERR_APP,
			L"buffer len (%lld) does not match, fixed=%lld", plane_len, fixed_len);
		CryptoPP::Integer x = key.ApplyRandomizedFunction(rng, CryptoPP::Integer(in_buf, fixed_len));
		x.Encode(out_buf, fixed_len);
		in_buf += fixed_len; out_buf += fixed_len; remain -= fixed_len;
	}
}

void crypto::RSADecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, CryptoPP::RSA::PrivateKey& key)
{
	size_t fixed_len = key.MaxImage().ByteCount();
	//key.GetValue<size_t>("KeySize", fixed_len);
	LOG_DEBUG(L"size of decryption = %zd", fixed_len);

	CryptoPP::AutoSeededRandomPool rng;
	const BYTE* in_buf = cipher_buf;
	size_t remain = cipher_len;
	BYTE *out_buf = plane_buf;

	while (remain > 0)
	{
		if (remain < fixed_len)	THROW_ERROR(ERR_APP,
			L"buffer len (%lld) does not match fixed=%lld, remain=%lld", cipher_len, fixed_len, remain);
		CryptoPP::Integer x = key.CalculateInverse(rng, CryptoPP::Integer(in_buf, fixed_len));
		if (plane_len < fixed_len) THROW_ERROR(ERR_APP, L"output length is not enough");
		x.Encode(out_buf, fixed_len);
		in_buf += fixed_len;
		out_buf += fixed_len;
		remain -= fixed_len;
		plane_len -= fixed_len;
	}
}

void crypto::CBCAESEncrypt(BYTE* cipher_buf, size_t cipher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(iv, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Encryption encryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Encryption cbc_encryption(encryption, iv);
	CryptoPP::StreamTransformationFilter encryptor(cbc_encryption, new CryptoPP::ArraySink(cipher_buf, cipher_len));
	encryptor.Put(plane_buf, plane_len);
	encryptor.MessageEnd();
}

void crypto::CBCAESDecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(iv, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Decryption cbc_decryption(decryption, iv);
	CryptoPP::StreamTransformationFilter decryptor(cbc_decryption, new CryptoPP::ArraySink(plane_buf, plane_len));
	decryptor.Put(cipher_buf, cipher_len);
	decryptor.MessageEnd();
}

DWORD crypto::CalculateCrc(BYTE* buf, size_t len)
{
	DWORD crc_out;
	CryptoPP::CRC32 crc;
	crc.Update(buf, len);
	crc.TruncatedFinal((BYTE*)(&crc_out), sizeof(DWORD));
	return crc_out;
}

void crypto::SetStringId(const std::string& string_id, boost::uuids::uuid& uuid, DWORD& id)
{
	boost::uuids::string_generator str_gen;
	boost::uuids::uuid dns_namespace_uuid = str_gen("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}");
	boost::uuids::name_generator gen(dns_namespace_uuid);
	uuid = gen(string_id);
	id = LODWORD(boost::uuids::hash_value(uuid));
}

UINT64 crypto::ComputerNameHash(const std::wstring& name)
{
	std::string src;
	std::string dst;
	jcvos::UnicodeToUtf8(src, name);
	dst.resize(src.size());
//	strupr(const_cast<char*>(str_computer_name.data()));
	transform(src.begin(), src.end(), dst.begin(), ::toupper);
	boost::hash<std::string> string_hash;
	return string_hash(dst);
}

DWORD Authority::LICENSE_HEADER::CalculateCrc(BYTE size)
{
	size_t len = (size_t)size * LICENSE_BLOCK_SIZE - sizeof(DWORD);		// 每个单位16字节，去除crc
	BYTE* ptr = (BYTE*)this + sizeof(DWORD);
	DWORD crc = crypto::CalculateCrc(ptr, len);
	return crc;
}