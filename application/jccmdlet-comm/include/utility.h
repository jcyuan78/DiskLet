#pragma once

using namespace System;
using namespace System::Management::Automation;

template <typename T>
void AddPropertyMember(PSObject^ list, String^ name, T^ val)
{
	PSNoteProperty^ item = gcnew PSNoteProperty(name, val);
	list->Members->Add(item);
}

template <typename T1, typename T2>
void AddPropertyMember(PSObject^ list, String^ name, const T2& val)
{
	PSNoteProperty^ item = gcnew PSNoteProperty(name, gcnew T1(val));
	list->Members->Add(item);
}




inline void ToStdString(std::wstring & dst, System::String ^ src)
{
	if (!System::String::IsNullOrEmpty(src))
	{
		const wchar_t * wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(src)).ToPointer();
		dst = wstr;
		System::Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)wstr));
	}
}

inline void SystemGuidToGUID(GUID & out_id, System::Guid ^ in_id)
{
	String ^ str_in = in_id->ToString(L"B");
	std::wstring str_out;
	ToStdString(str_out, str_in);
	CLSIDFromString(str_out.c_str(), &out_id);
}

inline System::Guid ^ GUIDToSystemGuid(const GUID & in_id)
{
	wchar_t str[64];
	StringFromGUID2(in_id, str, 64);
	return gcnew System::Guid(gcnew System::String(str));
}

/*
Clone::PartitionType GuidToPartitionType(System::Guid ^ type_id);

inline System::Guid ^ GptTypeToGuid(const Clone::PartitionType & type)
{
	System::Guid ^ type_id = nullptr;

	switch (type)
	{
	case Clone::PartitionType::EFI_Partition:
		type_id = gcnew System::Guid("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}");
		break;
	case Clone::PartitionType::Microsoft_Reserved:
		type_id = gcnew System::Guid("{e3c9e316-0b5c-4db8-817d-f92df00215ae}");
		break;
	case Clone::PartitionType::Basic_Data:
		type_id = gcnew System::Guid("{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}");
		break;
	case Clone::PartitionType::LDM_Metadata:
		type_id = gcnew System::Guid("{5808c8aa-7e8f-42e0-85d2-e1e90434cfb3}");
		break;
	case Clone::PartitionType::LDM_Data:
		type_id = gcnew System::Guid("{af9b60a0-1431-4f62-bc68-3311714a69ad}");
		break;
	case Clone::PartitionType::Microsoft_Recovery:
		type_id = gcnew System::Guid("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}");
		break;
	default:
		type_id = gcnew System::Guid();
		break;
	}
	return type_id;

}

*/


void GetComError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...);
void GetWmiError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...);

#ifndef THROW_COM_ERROR
#define THROW_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	jcvos::CJCException err(buf, jcvos::CJCException::ERR_APP);	\
    LogException(__STR2WSTR__(__FUNCTION__), __LINE__, err);	\
    throw err;	} while(0);
#endif

#ifndef  LOG_COM_ERROR
#define LOG_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	LOG_ERROR(buf);	} while(0);

#endif // ! LOG_COM_ERROR


namespace JcCmdLet
{

	template <typename TYPE>
	class auto_unknown
	{
	public:
		typedef TYPE* PTYPE;
		explicit auto_unknown(void) : m_ptr(NULL) {};
		explicit auto_unknown(TYPE* ptr) : m_ptr(ptr) {};
		~auto_unknown(void) { if (m_ptr) m_ptr->Release(); };

		operator TYPE*& () { return m_ptr; };
		operator IUnknown* () { return static_cast<IUnknown*>(m_ptr); }
		auto_unknown<TYPE>& operator = (TYPE* ptr)
		{
			JCASSERT(NULL == m_ptr);
			m_ptr = ptr;
			return (*this);
		}
		operator LPVOID* () { return (LPVOID*)(&m_ptr); }

		TYPE* operator ->() { return m_ptr; };

		PTYPE* operator &() { return &m_ptr; };
		TYPE& operator *() { return *m_ptr; };
		bool operator !()	const { return NULL == m_ptr; };
		bool operator == (const TYPE* ptr)	const { return /*const*/ ptr == m_ptr; };
		bool valid() const { return NULL != m_ptr; }
		//operator bool() const	{ return const NULL != m_ptr;};

		template <typename PTR_TYPE>
		PTR_TYPE d_cast() { return dynamic_cast<PTR_TYPE>(m_ptr); };

		void reset(void) { if (m_ptr) m_ptr->Release(); m_ptr = NULL; };

		template <typename TRG_TYPE>
		void detach(TRG_TYPE*& type)
		{
			JCASSERT(NULL == type);
			type = dynamic_cast<TRG_TYPE*>(m_ptr);
			JCASSERT(type);
			m_ptr = NULL;
		};

	protected:
		TYPE* m_ptr;
	};
}