///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <jccmdlet-comm.h>
#include <LibTcg.h>

#define RELEASE_INTERFACE(ptr) {if (ptr) ptr->Release(); ptr=NULL; 	}

using namespace System;
using namespace System::Management::Automation;

namespace SecureLet
{
	//=============================================================================

	public enum class TCG_UID_INDEX
	{
		/*SPs*/ THISSP, ADMINSP, LOCKINGSP,
		TCG_NONE, SID, ANYBODY, ADMIN1, USER1,
		TCG_C_PIN_MSID, TCG_C_PIN_SID,
		LOCKING, GLOBAL_RANGE, PSID, 
	};
	//public enum class TCG_AUTHORITY
	//{
	//	TCG_NONE, SID, ANYBODY, ADMIN1, USER1,
	//	TCG_C_PIN_MSID, TCG_C_PIN_SID,
	//};

	//public enum class TCG_TABLE
	//{
	//	LOCKING, GLOBAL_RANGE,
	//};

	//const TCG_UID& SpToUid(TCG_SP sp);
	//const TCG_UID& AuthorityToUid(TCG_AUTHORITY au);
	const TCG_UID& ToUid(TCG_UID_INDEX obj);

	public ref class TcgUid : Object
	{
	public:

		//static TcgUid^ MakeUid(System::Collections::ArrayList^ uid)
		static TcgUid^ FromRaw(array<int>^ uid)
		{
			if (uid->Length != 8) throw gcnew System::ApplicationException(L"size of uid shoud be 8");
			TCG_UID uu;
			for (int ii = 0; ii < 8; ++ii) uu[ii] = uid[ii];
			TcgUid^ id = gcnew TcgUid(uu);
			return id;
		}

		static TcgUid^ TableId(TCG_UID_INDEX tab, TCG_UID_INDEX row)
		{
			TCG_UID uu;
			CopyUid(uu, ToUid(tab));
			AddUid(uu, ToUid(row));
			TcgUid^ uid = gcnew TcgUid(uu);
			return uid;
		}

		static TcgUid^ FromIndex(TCG_UID_INDEX index)
		{
			return gcnew TcgUid(ToUid(index));
		}

	public:
		TcgUid(const TCG_UID& uid)
		{
			m_uid = new BYTE[8];
			memcpy_s(m_uid, 8, uid, 8);
		}
		~TcgUid(void) { delete m_uid; }

	public:
		void GetUid(TCG_UID& uid) { memcpy_s(uid, 8, m_uid, 8); }
		const BYTE* GetUid(void) { return m_uid; }
//		const TCG_UID & GetUidR(void) { return (TCG_UID)(m_uid); }
		String^ Print(void)
		{
			String^ str;
			for (int ii = 0; ii < 8; ++ii) str += String::Format(L"{0:X02} ", m_uid[ii]);
			return str;
		}

	protected:
		BYTE* m_uid;
	};

}