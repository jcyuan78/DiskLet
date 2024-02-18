#pragma once

using namespace System;
#include "../../StorageManagementLib/storage_management_lib.h"
#include "disk_info.h"

#define WM_STORAGE_DEVICE_CHANGE (WM_USER+1)

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
		bool CancelProcess(DWORD timeout) {
			return m_progress->CancelProcess(timeout);
		}

	public:
		property int state_num { int get() { return m_progress->GetStageNumber(); }	}
		property int cur_state { int get() { return m_progress->GetCurStage(); }	}
		property int result {int get() {return m_progress->GetResult();	}	}
		property String ^ status {
			String ^ get() {
				std::wstring str_status;
				m_progress->GetStatus(str_status);
				return gcnew String(str_status.c_str());
			}
		}

		property int progress {
			int get() {
				int pp = m_progress->GetProgress();
				return (int)((float)pp *100.0 / PERCENT_100);
			}
		}
		property UINT32 timeout;

	protected:
		IJCProgress * m_progress;
	};

///////////////////////////////////////////////////////////////////////////////
// == Storage Manager ==
	public interface class DeviceDetector
	{
	public:
		virtual bool OnDeviceChanged(int type, ULONG volumes) = 0;
	};


///////////////////////////////////////////////////////////////////////////////
// == Storage Manager ==

	class CDeviceListener : public IDeviceChangeListener
	{
	public:
		CDeviceListener(void) :m_hwnd(NULL) {}
	public:
		virtual void OnDeviceChange(EVENT_TYPE event, ULONG disk_id)
		{
			if (m_hwnd)
			{
				PostMessage(m_hwnd, WM_STORAGE_DEVICE_CHANGE, (int)(event), disk_id);
			}
		}
		void SetCallbackWindow(HWND hwnd)
		{
			m_hwnd = hwnd;
		}
	protected:
		HWND m_hwnd;
	};

	public ref class StorageManager : public Object
	{
	public:
		StorageManager(void);
		~StorageManager(void);
	public:
		bool ListDisk(void);
		UINT GetPhysicalDiskNum(void);
		DiskInfo ^ GetPhysicalDisk(UINT index);
		bool RegisterWindow(UINT64 handle);

		bool LoadConfig(const std::wstring& fn);

	protected:
		CDeviceListener * m_listener;
		IStorageManager * m_manager;
		DeviceDetector^ m_dector;


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