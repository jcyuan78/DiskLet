#pragma once

using namespace System;
#include "../../StorageManagementLib/storage_management_lib.h"
#include "disk_info.h"


#pragma make_public(IStorageManager)

extern const std::string g_app_name;


namespace Clone
{
///////////////////////////////////////////////////////////////////////////////
// == PROGRESS structure ==

	//public ref class PROGRESS : public Object
	//{
	//public:
	//	int state_num;
	//	int cur_state;
	//	int result;
	//	String ^ status;
	//	float progress;
	//};

///////////////////////////////////////////////////////////////////////////////
// == Invoking progress class ==


	public ref class InvokingProgress : public Object
	{
	public:
		InvokingProgress(IJCProgress * pp);
		~InvokingProgress(void);
	public:
		//PROGRESS ^ GetProgress(UINT32 timeout);

	public:
		property int state_num {
			int get() {
				return 0;
			}
		}
		property int cur_state {
			int get() {
				return 0;
			}
		}
		property int result {
			int get() {
				return m_progress->GetResult();
			}
		}
		property String ^ status {
			String ^ get() {
				std::wstring str_status;
				m_progress->GetStatus(str_status);
				return gcnew String(str_status.c_str());
			}
		}

		property float progress {
			float get() {
				int pp = m_progress->GetProgress();
				return (float)((float)pp *100.0 / INT_MAX);
			}
		}
		property UINT32 timeout;

	protected:
		IJCProgress * m_progress;
	};


///////////////////////////////////////////////////////////////////////////////
// == Storage Manager ==

	public ref class StorageManager : public Object
	{
	public:
		StorageManager(void);
		~StorageManager(void);
	public:
		bool ListDisk(void);
		UINT GetPhysicalDiskNum(void);
		DiskInfo ^ GetPhysicalDisk(UINT index);

	protected:
		IStorageManager * m_manager;
	};

///////////////////////////////////////////////////////////////////////////////
// == Disk Clone ==
	public ref class DiskClone : public Object
	{
	public:
		DiskClone(void);
		~DiskClone(void);

	public:
		//		Collections::ArrayList ^ MakeCopyStrategy();
		InvokingProgress ^ CopyDisk(DiskInfo ^ src_disk, DiskInfo ^ dst_dist);

	protected:
		CDiskClone * m_disk_clone;
	};

	public ref class AuthorizeException : public System::ApplicationException
	{
	public:
		AuthorizeException(System::String^ expression) : System::ApplicationException(expression) {}
	};
}