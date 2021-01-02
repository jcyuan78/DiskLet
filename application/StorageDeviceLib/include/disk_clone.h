#pragma once

//#include "storage_manager.h"
#include "idiskinfo.h"
#include "istorage_device.h"
#include "stratage_maker.h"
#include <boost/bind.hpp>

class CCopyProgress : public IJCProgress
{
public:
	CCopyProgress(void);
	~CCopyProgress(void);

// 实现IProgress接口
public:
	virtual int GetProgress(void);
	// Result: <0:处理中，=0:处理结束，没有错误，>0:处理结束，错误代码
	virtual int GetResult(void);
	virtual void GetStatus(std::wstring & status);
	virtual int WaitForStatusUpdate(DWORD timeout, std::wstring & status, int & progress);

public:
	void SetThread(HANDLE thread);
	void SetProgress(long progress);
	void SetProgress(UINT64 cur, UINT64 max);
	void SetResult(long result);
	DWORD WaitingForThread(void);
	void SetStatus(const std::wstring & status);

protected:
	CRITICAL_SECTION m_critical;
	HANDLE m_update_event;
	long m_progress;
	long m_result;
	HANDLE m_thread;
	std::wstring m_status;
};

struct PARTITION_ITEM
{
	UINT64 src_size;		//MB单位
	UINT64 src_min_size;	// 最小可压缩的容量，用于缩小时衡量
	UINT64 dst_offset;		// MB
	UINT64 dst_size;		// MB
	UINT src_index;
	bool system;
	bool fixed;				// 表示复制方法，true：不改变分区大小. fixed size for 
	// dst的大小已经分配，如果dst的offset!=0，这此dst固定
	bool set;				// dst是否已经设置完成
	//GUID src_type;
};

class CDiskClone
{
public:
	CDiskClone(void);
	virtual ~CDiskClone(void);

public:
	bool AsyncCopyFile(IJCProgress * & progress, IPartitionInfo * src, IPartitionInfo * dst, bool shadow);
	bool CopyVolumeByFile(CCopyProgress * progress, IPartitionInfo * src, IPartitionInfo * dst);

	bool AsyncCopyPartition(IJCProgress * & progress, IPartitionInfo * src, IPartitionInfo * dst);
	bool CopyPartition(CCopyProgress * progress, IPartitionInfo * src, IPartitionInfo * dst);


	bool AsyncCopyVolumeBySector(IJCProgress * & progress, IVolumeInfo * src, IPartitionInfo * dst, bool shadow, bool using_bitmap=true);
	bool CopyVolumeBySector(CCopyProgress * progress, IVolumeInfo * src, IPartitionInfo * dst, bool shadow, bool using_bitmap=true);
	
	bool CopyVolumeToFile(CCopyProgress * progress, IVolumeInfo * src, const std::wstring & fn);
	bool AsyncCopyVolumeToFile(IJCProgress * & progress, IVolumeInfo * src, const std::wstring & fn);

	// disk_size：指定目的磁盘大小，用于测试。
	bool MakeCopyStrategy(std::vector<CopyStrategy> & strategy, IDiskInfo * src, IDiskInfo * dst, UINT64 disk_size = (-1));
	bool AsyncCopyDisk(IJCProgress * & progress, std::vector<CopyStrategy> & strategy, IDiskInfo * src, IDiskInfo * dst);
	bool CopyDisk(CCopyProgress * progress, std::vector<CopyStrategy> & strategy, IDiskInfo * src, IDiskInfo * dst);

	void SetDevice(IStorageDevice * src, IStorageDevice * dst);
	void ClearDevice(void);
	// 导出分区分配函数用于算法测试
#ifdef _DEBUG
public:
#else
protected:
#endif
	bool PartitionAssign(PARTITION_ITEM * partitions, size_t valid_partitios, UINT64 src_size, UINT64 dst_size);
	// 在目标盘中配置启动分区。boot_index: EFI启动分区的index，sys_index：目标盘中的系统分区
	bool MakeBootPartitionForMBRSource(IDiskInfo * dst, UINT boot_index, UINT sys_index);

protected:

protected:
	static bool HandleRepasePoint(DWORD tag, const std::wstring & src, const std::wstring & dst);
	bool CopySectors(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 src_offset, UINT64 cluster_num, UINT64 cluster_size);

	bool CopySectorsByMap(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 cluster_num, UINT64 cluster_size);
	bool CopySectorsByMapOverlap(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 cluster_num, UINT64 cluster_size);
	UINT32 CreatePartition(IPartitionInfo * &part, IDiskInfo * disk,
		/*INOUT*/ UINT64 & offset, /*INOUT*/ UINT64 & size, const GUID &type);
	bool CDiskClone::CheckWindowsDrive(IPartitionInfo * part);

protected:
	// 用于记录磁盘温度
	IStorageDevice * m_src_dev;
	IStorageDevice * m_dst_dev;
};

template <class FUNC>
class CAsyncCall : public CCopyProgress
{
public:
	CAsyncCall(FUNC & f) : m_func(f) {}
	~CAsyncCall(void) {}

public:
	bool Run(void)
	{
		SetStatus(L"Preparing ...");
		m_result = -1;
		m_thread = CreateThread(NULL, 0, CallThread, this, 0, &m_tid);
		return true;
	}

protected:
	static DWORD WINAPI CallThread(_In_ LPVOID lpParameter)
	{
		CAsyncCall<FUNC> * this_obj = (CAsyncCall<FUNC> *)(lpParameter);
		try
		{
			this_obj->m_func(static_cast<CCopyProgress*>(this_obj));
			this_obj->SetResult(0);
		}
		catch (jcvos::CJCException & err)
		{
			wprintf_s(L"[err] %s", err.WhatT());
			this_obj->SetResult(1);
		}
		return 0;
	}

protected:
	DWORD m_tid;	// thread id
	FUNC m_func;
};

template <class FUNC>
CAsyncCall<FUNC> * CreateAsyncCall(FUNC & f)
{
	CAsyncCall<FUNC> *ff = new jcvos::CDynamicInstance<CAsyncCall<FUNC> >(f);
	return ff;
}
