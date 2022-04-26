#pragma once

#include <jcparam.h>

#include "dta_lexicon.h"
#include <storage_device_lib.h>

namespace tcg
{

	class ITcgSession : virtual public IJCInterface
	{
	public:
		virtual BYTE L0Discovery(BYTE* buf) = 0;
		virtual BYTE StartSession(const TCG_UID SP, const char* host_challenge, const TCG_UID sign_authority, bool write = true) = 0;
		virtual BYTE EndSession(void) = 0;
		virtual bool IsOpen(void) = 0;

		virtual BYTE GetDefaultPassword(std::string& password) = 0;
		virtual BYTE SetSIDPassword(const char* old_pw, const char* new_pw) = 0;
		virtual void Reset(void) = 0;


	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == Security Object ==
	//	解析security command的对象
	enum SECURITY_SHOW_OPT
	{	// 用于显示的选项
		SHOW_PACKET_INFO = 1,	// 对于tcg command, 显示packeg和subpacket

	};

	/// <summary> TCG Command/Security command的object。表示TCG的Token， </summary>
	class ISecurityObject : public IJCInterface
	{
	public:
		/// <summary>
		/// 
		/// </summary>
		/// <param name="out">  </param>
		/// <param name="layer"></param>
		/// <param name="opt">  </param>
		virtual void ToString(std::wostream & out, UINT layer, int opt) =0;
		
		/// <summary>获取command或者packet的payload。</summary>
		/// <param name="index">子节点的索引。0为自身，>0为子结构的[index-1]</param>
		virtual void GetPayload(jcvos::IBinaryBuffer*& data, int index) =0;

		/// <summary> </summary>
		virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name) = 0;
	};



	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == Security Parser ==
	//	解析器
	class ISecurityParser : public IJCInterface
	{
	public:
		virtual bool Initialize(const std::wstring& uid_config, const std::wstring& l0_config) = 0;
		//virtual bool ParseSecurityCommand(std::vector<ISecurityObject*>& out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive) = 0;
		virtual bool ParseSecurityCommand(ISecurityObject* & out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive) = 0;
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == Security Object ==
	//	解析security command的对象


	void CreateTcgSession(ITcgSession*& session, IStorageDevice* dev);

	// 指定数据文件路径
	void CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring& path);
	void CreateSecurityParser(ISecurityParser*& parser);
}

