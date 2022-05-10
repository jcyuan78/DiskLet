///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <jcparam.h>

#include "dta_lexicon.h"
#include <storage_device_lib.h>

//#define PROTOCOL_ID_TCG (0x01)
//#define PROTOCOL_ID_TPER (0x02)
//#define COMID_L0DISCOVERY (0x01)

namespace tcg
{
	class ISecurityObject;

	static const DWORD PROTOCOL_ID_TCG = (0x01);
	static const DWORD PROTOCOL_ID_TPER = (0x02);
	static const DWORD COMID_L0DISCOVERY = (0x01);

	class ITcgSession : virtual public IJCInterface
	{
	public:
		/// <summary>
		/// 获取TCG支持的features (L0Discovery). 通常从Session的缓存获取，如果force为true，则重新执行L0Discovery。
		/// </summary>
		/// <param name="feature">[OUT]feature结果</param>
		/// <param name="force">[IN]是否强制刷新</param>
		/// <returns></returns>
		virtual bool GetFeatures(ISecurityObject * & feature, bool force) = 0;
		virtual bool L0Discovery(BYTE* buf) = 0;
		// Properties
		// @props: [INOUT]请求的属性，已经device的答复
		virtual BYTE Properties(boost::property_tree::wptree & props, const std::vector<std::wstring>& req) = 0;
		virtual BYTE StartSession(const TCG_UID SP, const char* host_challenge, const TCG_UID sign_authority, 
			bool write = true) = 0;
		virtual BYTE EndSession(void) = 0;

		virtual bool IsOpen(void) = 0;


		//
		virtual BYTE GetTable(ISecurityObject * & res, const TCG_UID table, WORD start_col, WORD end_col)=0;
		virtual BYTE SetTable(ISecurityObject * & res, const TCG_UID table, int name, const char* value) = 0;
		virtual BYTE SetTable(ISecurityObject*& res, const TCG_UID table, int name, int val)=0;

		virtual BYTE Activate(ISecurityObject*& res, const TCG_UID obj) = 0;
		virtual BYTE Revert(ISecurityObject*& res, const TCG_UID sp) = 0;

		// == hi-level features (with session open) ==
		virtual BYTE RevertTPer(const char* password, const TCG_UID authority, const TCG_UID sp) = 0;
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
		virtual bool ParseSecurityCommand(ISecurityObject* & out, const BYTE* payload, size_t len, DWORD protocol, DWORD comid, bool receive) = 0;
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == Security Object ==
	//	解析security command的对象


	void CreateTcgSession(ITcgSession*& session, IStorageDevice* dev);

	// 指定数据文件路径
	void CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring& path);
	void GetSecurityParser(ISecurityParser*& parser);
}


//// ==== configs =====================================================================================================
// 配置：如果USING_DIRECTORY_FOR_FEATURE有效，则feature set被作为以feature name为索引的字典。否则feature set是一个由feature组成的数组。
#define USING_DIRECTORY_FOR_FEATURE

