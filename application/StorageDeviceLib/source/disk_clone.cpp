///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include <stdext.h>
#include "../include/utility.h"
#include "../include/disk_clone.h"
#include <boost/cast.hpp>
#include <list>

#define OVERLAP 16

LOCAL_LOGGER_ENABLE(L"disk_clone", LOGGER_LEVEL_NOTICE);

// 为提高调试效率，忽略实际复制操作
#define _IGNORE_COPY

// Microsoft Reserved size = 16MB. 一般第一个有效分区的起始位置
#define MSR_SIZE	(16)
// EFI的启动分区，缺省大小100MB
#define EFI_BOOT_SIZE	(100)

CDiskClone::CDiskClone(void)
	: m_src_dev (NULL), m_dst_dev(NULL)
{
}

CDiskClone::~CDiskClone(void)
{
	ClearDevice();
}



#define BUF_SIZE (UINT64)(64*1024)

bool CDiskClone::CopyPartition(CCopyProgress * progress, IPartitionInfo * src, IPartitionInfo * dst)
{
	if (src == nullptr || dst == nullptr)
	{
		THROW_ERROR(ERR_APP, L"source (%p) or destination (%p) is a null ptr", src, dst);
	}

	jcvos::auto_interface<IVolumeInfo> dst_vol;
	dst->GetVolume(dst_vol);


	UINT64 dst_offset, dst_size;
	HANDLE _hdst;
	if (dst_vol)
	{
		_hdst = dst_vol->OpenVolume(GENERIC_READ | GENERIC_WRITE);
		dst_offset = 0;
	}
	else
	{
		_hdst = dst->OpenPartition(dst_offset, dst_size, GENERIC_READ | GENERIC_WRITE);
	}
	jcvos::auto_handle<HANDLE> hdst(_hdst);
	if (hdst == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening src disk");

	UINT64 src_offset, src_size;
	jcvos::auto_handle<HANDLE> hsrc(src->OpenPartition(src_offset, src_size, GENERIC_READ));
	if (hsrc == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening src disk");

	bool br = CopySectors(progress, hsrc, hdst, dst_offset, src_offset, src_size, 1);
	if (progress) progress->SetResult(0);
	return br;
}

bool CDiskClone::CopyVolumeBySector(CCopyProgress * progress, IVolumeInfo * vsrc, IPartitionInfo * pdst, bool shadow, bool using_bitmap)
{
	LOG_STACK_TRACE();
	if (vsrc == nullptr || pdst == nullptr)
	{
		THROW_ERROR(ERR_APP, L"source (%p) or destination (%p) is a null ptr", vsrc, pdst);
	}

	jcvos::auto_interface<IVolumeInfo> dst_vol;
	pdst->GetVolume(dst_vol);
	// open destination
	UINT64 dst_offset = 0, dst_size = 0;
	HANDLE _hdst;

	DWORD overlap=0;

#if OVERLAP > 0
	overlap = FILE_FLAG_OVERLAPPED;
#endif

	if (dst_vol) _hdst = dst_vol->OpenVolume(GENERIC_READ | GENERIC_WRITE, overlap);
	else _hdst = pdst->OpenPartition(dst_offset, dst_size, GENERIC_READ | GENERIC_WRITE);
	jcvos::auto_handle<HANDLE> hdst(_hdst);
	if (hdst == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening dst disk %s", L"<??>");

	// create snapshot
	//auto_unknown<IVssBackupComponents> backup;
	std::wstring src_name;
	VOLUME_PROPERTY vprop;
	vsrc->GetProperty(vprop);
	src_name = vprop.m_name;
	
	jcvos::auto_interface<IVolumeInfo> src_vol;
	if (shadow)
	{
		bool br = vsrc->CreateShadowVolume(src_vol);
		if (!br || !src_vol) THROW_ERROR(ERR_APP, L"failed on getting shadow for %s", src_name.c_str());
		src_vol->GetProperty(vprop);
		src_name = vprop.m_name;
	}
	else
	{
		src_vol = vsrc;
		src_vol->AddRef();
	}

	// open source
	jcvos::auto_handle<HANDLE> hsrc( src_vol->OpenVolume(GENERIC_READ, overlap) );
	if (hsrc == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening src disk %s", src_name.c_str());

	bool br;
	if (using_bitmap)
	{
#if OVERLAP > 0
		br = CopySectorsByMapOverlap(progress, hsrc, hdst, dst_offset, vprop.m_clust_num,
			vprop.m_clust_size);
#else
		br = CopySectorsByMap(progress, hsrc, hdst, dst_offset, vprop.m_clust_num, 
			vprop.m_clust_size);
#endif
	}
	else
	{
		br = CopySectors(progress, hsrc, hdst, dst_offset, 0, vprop.m_size, 1);
	}
	if (progress) progress->SetResult(0);
	return br;
}

bool CDiskClone::CopyVolumeToFile(CCopyProgress * progress, IVolumeInfo * vscr, const std::wstring & fn)
{
	LOG_STACK_TRACE();
	if (vscr == nullptr || fn.empty() )
	{
		THROW_ERROR(ERR_APP, L"source (%p) or destination (%s) is a null ptr", vscr, fn.c_str());
	}
	// open destination
	UINT64 dst_offset = 0, dst_size = 0;
	HANDLE _hdst;

	DWORD overlap = 0;

#if OVERLAP > 0
	overlap = FILE_FLAG_OVERLAPPED;
#endif

	// create snapshot
	std::wstring src_name;
	VOLUME_PROPERTY vprop;
	vscr->GetProperty(vprop);
	src_name = vprop.m_name;

	jcvos::auto_interface<IVolumeInfo> src_vol;
	if (true)
	{
		bool br = vscr->CreateShadowVolume(src_vol);
		if (!br || !src_vol) THROW_ERROR(ERR_APP, L"failed on getting shadow for %s", src_name.c_str());
		src_vol->GetProperty(vprop);
		src_name = vprop.m_name;
	}
	else
	{
		src_vol = vscr;
		src_vol->AddRef();
	}

	// open source
	jcvos::auto_handle<HANDLE> hsrc(src_vol->OpenVolume(GENERIC_READ, overlap));
	if (hsrc == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening src disk %s", src_name.c_str());


	// 打开文件
	_hdst = CreateFile(fn.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, overlap, NULL);
	jcvos::auto_handle<HANDLE> hdst(_hdst);
	if (hdst == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening dst disk %s", L"<??>");
	SetFileValidData(_hdst, vprop.m_clust_num * vprop.m_clust_size);

	bool br;
	if (true)
	{
#if OVERLAP > 0
		br = CopySectorsByMapOverlap(progress, hsrc, hdst, dst_offset, vprop.m_clust_num,
			vprop.m_clust_size);
#else
		br = CopySectorsByMap(progress, hsrc, hdst, dst_offset, vprop.m_clust_num,
			vprop.m_clust_size);
#endif
	}
	else
	{
		br = CopySectors(progress, hsrc, hdst, dst_offset, 0, vprop.m_size, 1);
	}
	if (progress) progress->SetResult(0);
	return br;
}

bool CDiskClone::AsyncCopyPartition(IJCProgress *& progress, IPartitionInfo * src, IPartitionInfo * dst)
{
#if 0
	JCASSERT(progress == nullptr);
	auto func = boost::bind(&CDiskClone::CopyPartition, this, boost::placeholders::_1, src, dst);
	auto cc = CreateAsyncCall(func);
	progress = static_cast<IJCProgress *>(cc);
	bool br = cc->Run();
	return br;
#else
	return false;
#endif
}

bool CDiskClone::AsyncCopyVolumeBySector(IJCProgress *& progress, IVolumeInfo * src, IPartitionInfo * dst, 
		bool shadow, bool using_bitmap)
{
#if 0
	JCASSERT(progress == nullptr);

	auto func = boost::bind(&CDiskClone::CopyVolumeBySector, this, _1, src, dst, shadow, using_bitmap);
	auto cc = CreateAsyncCall(func);
	progress = static_cast<IJCProgress *>(cc);
	bool br = cc->Run();
	return br;
#else
	return false;
#endif
}

bool CDiskClone::AsyncCopyVolumeToFile(IJCProgress *& progress, IVolumeInfo * src, const std::wstring & fn)
{
#if 0
	JCASSERT(progress == nullptr);
	auto func = boost::bind(&CDiskClone::CopyVolumeToFile, this, _1, src, fn);
	auto cc = CreateAsyncCall(func);
	progress = static_cast<IJCProgress *>(cc);
	bool br = cc->Run();
	return br;
#else
	return false;
#endif
}

bool CDiskClone::AsyncCopyFile(IJCProgress *& progress, IPartitionInfo * src, IPartitionInfo * dst, bool shadow)
{
#if 0
	JCASSERT(progress == nullptr);

	auto func = boost::bind(&CDiskClone::CopyVolumeByFile, this, _1, src, dst);
	auto cc = CreateAsyncCall(func);
	progress = static_cast<IJCProgress *>(cc);
	bool br = cc->Run();
	return br;
#else
	return false;
#endif
}

bool CDiskClone::AsyncCopyDisk(IJCProgress * & progress, std::vector<CopyStrategy> & strategy, IDiskInfo * src, 
		IDiskInfo * dst)
{
#if 0
	JCASSERT(progress == nullptr);
	auto func = boost::bind(&CDiskClone::CopyDisk, this, _1, strategy, src, dst);
//	auto cc = CreateAsyncCall(boost::bind(&CDiskClone::CopyDisk, this, _1, strategy, src, dst));
	auto cc = CreateAsyncCall(func);
	progress = static_cast<IJCProgress *>(cc);
	bool br = cc->Run();
	return br;
#else
	return false;
#endif
}


bool CDiskClone::CopyVolumeByFile(CCopyProgress * progress, IPartitionInfo * src, IPartitionInfo * dst)
{
	LOG_STACK_TRACE();
	if (src == nullptr || dst == nullptr)
	{
		THROW_ERROR(ERR_APP, L"source (%p) or destination (%p) is a null ptr", src, dst);
	}
	VOLUME_PROPERTY src_vprop;
	src->GetVolumeProperty(src_vprop);
	// source的文件大小，用于计算进度
	UINT64 src_used = src_vprop.m_size - src_vprop.m_empty_size;
	UINT64 copied = 0;
	std::wstring src_root = src_vprop.m_name.substr(0,src_vprop.m_name.size()-1);
	VOLUME_PROPERTY dst_vprop;
	dst->GetVolumeProperty(dst_vprop);
	std::wstring dst_root = dst_vprop.m_name.substr(0, src_vprop.m_name.size() - 1);

	std::list<std::wstring> open_list;
	std::wstring str_root = L"\\";
	open_list.push_back(str_root);
	std::wstring this_dir = L".";
	std::wstring parent_dir = L"..";
	if (progress) progress->SetStatus(L"preparing ...");

	while (1)
	{
		if (open_list.empty()) break;
		std::wstring parent = open_list.front();
		open_list.pop_front();
		std::wstring src_dir = src_root + parent + L"\\*";

		WIN32_FIND_DATA find_data;
		HANDLE find = FindFirstFile(src_dir.c_str(), &find_data);
		if (find == INVALID_HANDLE_VALUE || find == nullptr) continue;

		while (1)
		{
			std::wstring path = parent + L"\\" + find_data.cFileName;
			if (this_dir != find_data.cFileName && parent_dir != find_data.cFileName)
			{
				std::wstring dst_path = dst_root + path;
				std::wstring src_path = src_root + path;
				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{// create directory
					open_list.push_back(path);
					if (find_data.cFileName[0] != '$')
					{
						std::wstring status = L"processing dir ";
						status += path;
						if (progress) progress->SetStatus(status);
						BOOL br = CreateDirectory(dst_path.c_str(), NULL);
					}
				}
				else
				{// copy file from source to dst
					if (find_data.cFileName[0] != '$')
					{
						std::wstring status = L"copying file ";
						status += path;
						if (progress) progress->SetStatus(status);
						BOOL br = CopyFile(src_path.c_str(), dst_path.c_str(), TRUE);
						HANDLE hsrc = CreateFile(src_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL, OPEN_EXISTING, 0, NULL);
						if (hsrc != INVALID_HANDLE_VALUE)
						{
							LARGE_INTEGER fsize;
							GetFileSizeEx(hsrc, &fsize);
							copied += fsize.QuadPart;
						}
					}
				}
			}
			if (progress) progress->SetProgress(copied, src_used);
			BOOL br = FindNextFile(find, &find_data);
			if (!br) break;
		}
		if (open_list.empty()) break;
	}
	return true;
}


//bool CDiskClone::CreateVolumeSnapshot(_Out_ IVssBackupComponents *& backup, _Out_ std::wstring & snapshot, _In_ const wchar_t * vol_name)
//{
//	if (!vol_name)
//	{
//		LOG_ERROR(L"[CopyVolume] Invalid param");
//		return false;
//	}
//	VSS_ID                  snapshotSetID = { 0 };
//	BOOL                    bRetVal = true;
//	LOG_NOTICE(L"CreateSnapshotSet...");
//	VSS_SNAPSHOT_PROP prop = { 0 };
//
//	try
//	{
//		if (!CreateSnapshotSet(backup, snapshotSetID, vol_name, prop))
//			THROW_ERROR(ERR_APP, L"CreateSnapshotSet failed");
//		snapshot = prop.m_pwszSnapshotDeviceObject;
//	}
//	catch (jcvos::CJCException & err)
//	{
//		VssFreeSnapshotProperties(&prop);
//		throw err;
//	}
//	VssFreeSnapshotProperties(&prop);
//	return true;
//}


// 事前条件：partition中的flexible分区必须大于0;
bool CDiskClone::PartitionAssign(PARTITION_ITEM * partitions, size_t valid_partition, UINT64 dst_offset, UINT64 dst_size)
{
	size_t next_flexible = 0;
	UINT64 src_size = 0;
	UINT64 src_flexible = 0;
	UINT64 dst_flexible = 0;
	UINT64 src_min = 0;
	for (size_t ii = 0; ii < valid_partition; ++ii)
	{
		src_size += partitions[ii].src_size;
		if (partitions[ii].fixed)
		{
			partitions[ii].dst_size = partitions[ii].src_size;
			partitions[ii].src_min_size = partitions[ii].src_size;
			partitions[ii].set = true;
		}
		else
		{
			src_flexible += partitions[ii].src_size;
		}
		src_min += partitions[ii].src_min_size;
	}
	if (src_flexible == 0)
	{
		LOG_WARNING(L"[warning] there is no flexible partition");
		return false;
	}
	if (src_min > (dst_size - dst_offset))
	{
		LOG_ERROR(L"[err] dst has no enough size. necessary=%lld MB, dst size=%lld MB", src_min+dst_offset, dst_size);
		return false;
	}
	partitions[0].dst_offset = dst_offset;
	partitions[valid_partition].dst_offset = dst_size;
	partitions[valid_partition].set = true;
	JCASSERT(src_flexible > 0);

	dst_flexible = dst_size - dst_offset - (src_size - src_flexible);
	size_t remain_to_set = valid_partition;

	UINT round = 0;
	while (1)
	{
		LOG_DEBUG(L"[round=%d], src_flexible=%lld, dst_flexible=%lld, remain=%d", 
			round++, src_flexible, dst_flexible, remain_to_set);
		if (round > (valid_partition * 10))
		{
			LOG_ERROR(L"[err] time out");
			return false;
		}
		bool changed = false;
		for (size_t ii = 0; ii < valid_partition; ++ii)
		{
			PARTITION_ITEM & partition = partitions[ii];
			if (partition.set && partition.dst_offset != 0 && partitions[ii+1].dst_offset != 0)
			{	// 由于分区的位置一定从磁盘的两头开始固定，
				if (next_flexible == ii) next_flexible++;
				continue;
			}
			// 调整大小
			if (!partition.set && !partition.fixed)
			{	// 大小未固定, 估算大小
				float ratio = (float)dst_flexible / (float)src_flexible;
				//UINT64 d
				partition.dst_size = boost::numeric_cast<UINT64>(partition.src_size * ratio);
				LOG_DEBUG(L"pre-set size: partition=%d, (ratio), src=%lld, dst=%lld",
					partition.src_index, partition.src_size, partition.dst_size);

				if (partition.dst_size < partition.src_min_size)
				{
					partition.dst_size = partition.src_min_size;
					partition.set = true;
					// 调整剩余容量
					src_flexible -= partition.src_size;
					dst_flexible -= partition.dst_size;
					LOG_DEBUG(L"pre-set size: partition=%d, (min), src=%lld, dst=%lld",
						partition.src_index, partition.src_size, partition.dst_size);
					changed = true;
				}
			}
			else
			{
				JCASSERT(partition.dst_size != 0);
				if (partition.dst_size == 0)
				{
					LOG_ERROR(L"dst[%d] is set but size =0", partition.src_index);
					return false;
				}
			}

			if (partition.dst_offset != 0)
			{
				if (partitions[ii + 1].dst_offset != 0)
				{	// 前后位置固定，设置大小
					if (partition.set)
					{
						LOG_ERROR(L"[err] failed on assign partition size, partiiton=%d, ", 
							partition.src_index);
						return false;
					}
					UINT64 size = partitions[ii + 1].dst_offset - partition.dst_offset;
					if (size < partition.src_min_size)
					{
						LOG_ERROR(L"[err] no enough capacity, partition=%d, src=%lld, src_min=%lld, dst=%lld",
							partition.src_index, partition.src_size, partition.src_min_size, size);
						return false;
					}
					partition.dst_size = size;
					partition.set = true;
					remain_to_set--;
					changed = true;
					// 调整剩余容量
					src_flexible -= partition.src_size;
					dst_flexible -= partition.dst_size;
					LOG_DEBUG(L"set partition=%d, (set size), offset=%lld, size=%lld, remain=%d",
						partition.src_index, partition.dst_offset, partition.dst_size, remain_to_set);
				}
				else if (partition.set)
				{	// 位置和大小固定，设置后面
					if (partitions[ii + 1].dst_offset != 0)
					{
						LOG_ERROR(L"assign conflicit");
						return false;
					}
					partitions[ii + 1].dst_offset = partition.dst_offset + partition.dst_size;
					remain_to_set--;
					changed = true;
					LOG_DEBUG(L"set partition=%d, (set next), offset=%lld, size=%lld, remain=%d",
						partition.src_index, partition.dst_offset, partition.dst_size, remain_to_set);
				}
			}
			else if (partitions[ii + 1].dst_offset != 0 && partition.set)
			{	// 后端和大小固定，设置前端
				if (partition.dst_offset != 0)
				{
					LOG_ERROR(L"assign confilict");
					return false;
				}
				partition.dst_offset = partitions[ii + 1].dst_offset - partition.dst_size;
				remain_to_set--;
				changed = true;
				LOG_DEBUG(L"set partition=%d, (set offset), offset=%lld, size=%lld, remain=%d",
					partition.src_index, partition.dst_offset, partition.dst_size, remain_to_set);
			}
		}
		// 所有的分区都已经分配
		if (remain_to_set == 0) break;
		if (changed == false && next_flexible < valid_partition)
		{	// 没有分区被固定下来
			partitions[next_flexible].set = true;
		}
	}
	return true;
}

bool CDiskClone::MakeBootPartitionForMBRSource(IDiskInfo * dst, UINT boot_index, UINT sys_index)
{
	wprintf_s(L"set boot partition\n");
	if (sys_index == 0)
	{
		wprintf_s(L"does not found windows in the source disk");
		return false;
	}
	// TODO: 
	//	(1) 查找空闲的盘符，
	DWORD drive_letters = GetLogicalDrives();
	DWORD mask = 0x4;
	wchar_t letter = 'C', boot_letter = 0, sys_letter = 0;
	for (; letter <= 'Z'; mask <<= 1, letter++)
	{
		if ((drive_letters & mask) == 0)
		{	// 找到一个空闲字符
			if (boot_letter == 0) boot_letter = letter;
			else
			{
				sys_letter = letter;
				break;
			}
		}
	}
	if (boot_letter == 0 || sys_letter == 0)
	{
		LOG_ERROR(L"[err] there is no empty logical driver, boot=%c, sys=%c", boot_letter, sys_letter);
		return false;
	}
	// （2）通过SystemRoot获取windows路径
//	wchar_t path[MAX_PATH];
	jcvos::auto_array<wchar_t> path(MAX_PATH);

	jcvos::auto_interface<IPartitionInfo> boot_part;
	bool br = dst->GetPartition(boot_part, boot_index);
	if (!br || !boot_part) THROW_ERROR(ERR_APP, L"failed on getting boot partition.");

	jcvos::auto_interface<IVolumeInfo> boot_vol;
	boot_part->FormatVolume(boot_vol, L"FAT32", L"", false, false);
//	boot_part->Mount(L"Z:\\");
	swprintf_s(path, MAX_PATH, L"%C:\\", boot_letter);
	LOG_NOTICE(L"mount boot partition to %s", path);
	boot_part->Mount((wchar_t*)path);

	jcvos::auto_interface<IPartitionInfo> sys_part;
	br = dst->GetPartition(sys_part, sys_index);
	if (!br || !sys_part) THROW_ERROR(ERR_APP, L"failed on getting system partition.");
	swprintf_s(path, MAX_PATH, L"%C:\\", sys_letter);
	LOG_NOTICE(L"mount system partition to %s", path);
	sys_part->Mount((wchar_t*)path);
	DWORD ir = GetEnvironmentVariable(L"SystemRoot", path, MAX_PATH);
	if (ir == 0)
	{
		LOG_WIN32_ERROR(L"cannot found system root");
		return false;
	}
	path[0] = sys_letter;

//	sys_part->Mount(L"Y:\\");
//	std::wstring sys_path = L"Y:\\Windows";

//	wchar_t cmd_buf[256] = { 0 };
	jcvos::auto_array<wchar_t> cmd_buf(MAX_PATH);
	swprintf_s(cmd_buf, MAX_PATH, L"bcdboot.exe %s /s Z: /f ALL", (wchar_t*)path);
	LOG_NOTICE(L"call bcd boot as %s", cmd_buf);

	PROCESS_INFORMATION pi;		memset(&pi, 0, sizeof(pi));
	STARTUPINFO si;				memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	GetStartupInfo(&si);
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.wShowWindow = SW_HIDE;
	si.dwFlags |= STARTF_USESTDHANDLES;

	br = CreateProcess(NULL, cmd_buf, NULL, NULL, TRUE,
		0, NULL, NULL, &si, &pi);
	if (!br) THROW_WIN32_ERROR(L" failed on running bcdboot ");

	ir = WaitForSingleObject(pi.hProcess, 100000);
	if (ir == WAIT_TIMEOUT)
	{
		wprintf_s(L"bcdboot timeout.");
		TerminateProcess(pi.hProcess, 100);
	}
	GetExitCodeProcess(pi.hProcess, &ir);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	wprintf_s(L"bcdboot completed. code=%d\n\n", ir);

	dst->CacheGptHeader();
	dst->SetPartitionType(GPT_TYPE_EFI, boot_index);
	dst->UpdateGptHeader();
	return true;
}

bool CDiskClone::HandleRepasePoint(DWORD tag, const std::wstring & src, const std::wstring & dst)
{
//	wprintf_s(L"link ");
	BOOL br;
	DWORD read = 0;
	switch (tag)
	{
	case IO_REPARSE_TAG_MOUNT_POINT: {
		std::wstring src_path;
		src_path = src;
		wchar_t names[MAX_PATH];
		PWCHAR NameIdx = NULL;
		br = GetVolumeNameForVolumeMountPoint(src_path.c_str(), names, MAX_PATH-1);
		if (!br) LOG_WIN32_ERROR(L"failed on getting volumen anme");
		br = SetVolumeMountPoint(dst.c_str(), names);
		if (!br) LOG_WIN32_ERROR(L"failed on setting mount point");
		wprintf_s(L"mount point"); break; }
	case IO_REPARSE_TAG_SYMLINK: {
		HANDLE hh = CreateFile(src.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
			0, OPEN_EXISTING, FILE_ATTRIBUTE_REPARSE_POINT, NULL);
		if (hh == INVALID_HANDLE_VALUE) {
			LOG_WIN32_ERROR(L"failed on open link");
			break;
		}
		jcvos::auto_array<BYTE> _buf(1024, 0);
		REPARSE_GUID_DATA_BUFFER * repare = (REPARSE_GUID_DATA_BUFFER*)((BYTE*)_buf);
		BOOL br = DeviceIoControl(hh, FSCTL_GET_REPARSE_POINT, NULL, 0,
			repare, 1024, &read, NULL);
		if (!br) LOG_WIN32_ERROR(L"failed on getting reparse info")
		CloseHandle(hh);
		wprintf_s(L"symbolink");
		break;}
	}
	return false;
}

bool CDiskClone::CopySectors(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 src_offset, UINT64 cluster_num, UINT64 cluster_size)
{
	UINT64 size = cluster_num * cluster_size;

	LARGE_INTEGER src_pos, dst_pos;
	jcvos::auto_array<BYTE> _buf(BUF_SIZE);
	BYTE * buf = _buf;


	UINT64 offset = 0;
	size_t remain = size;
	size_t next_update = 0;
	while (remain > 0)
	{
		DWORD copy_size = (DWORD)min(BUF_SIZE, remain);
		DWORD read, written;
		dst_pos.QuadPart = dst_offset + offset;
		SetFilePointerEx(hdst, dst_pos, NULL, FILE_BEGIN);
		src_pos.QuadPart = src_offset + offset;
		SetFilePointerEx(hsrc, src_pos, NULL, FILE_BEGIN);

		BOOL br = ReadFile(hsrc, buf, copy_size, &read, NULL);
		if (!br || read < copy_size)
		{
			wprintf_s(L"failed on reading data sec:0x%llX, read size=%d\n", offset / 512, read);
			LOG_WIN32_ERROR(L"failed on reading data, sec=0x%llX", offset / 512);
		}

		br = WriteFile(hdst, buf, copy_size, &written, NULL);
		if (!br || written < copy_size)
		{
			wprintf_s(L"failed on writing data sec:0x%llX, write size=%d\n", offset / 512, written);
			LOG_WIN32_ERROR(L"failed on writing data, sec=0x%llX", offset / 512);
		}

		remain -= copy_size;
		offset += copy_size;
		if ((size-remain) >= next_update)
		{
			if (progress) progress->SetProgress((size - remain), size);
			next_update += 100 * 1024 * 1024;
		}
	}
	return true;
}

bool CDiskClone::CopySectorsByMap(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 cluster_num, UINT64 cluster_size)
{
	LOG_STACK_TRACE();
	// get bitmap
	DWORD read = 0;
	size_t bmp_size = (cluster_num - 1) / 32 + 10;
	jcvos::auto_array<DWORD> _buf_2(bmp_size, 0);

	STARTING_LCN_INPUT_BUFFER start_add = { 0 };
	BOOL br = DeviceIoControl(hsrc, FSCTL_GET_VOLUME_BITMAP,
		&start_add, sizeof(STARTING_LCN_INPUT_BUFFER), _buf_2, (DWORD)(bmp_size * sizeof(DWORD)),
		&read, NULL);
	if (!br) LOG_WIN32_ERROR(L" failed on get volume bitmap");
	VOLUME_BITMAP_BUFFER * bmp = (VOLUME_BITMAP_BUFFER *)((DWORD*)_buf_2);
	LOG_DEBUG(L"cluster_size=%dKB, cluster_num=%d, size=%dKB",
		cluster_size / 1024, cluster_num, cluster_num * cluster_size / 1024);

	// 按照bitmap的DWORD处理，一个DWORD包含32个cluster = 128KB。
	size_t buf_size = cluster_size * 32;
	jcvos::auto_array<BYTE> _buf(buf_size);
	BYTE * buf = (BYTE*)_buf;
	DWORD * dwbmp = (DWORD*)(bmp->Buffer);
	size_t seg_num = (cluster_num - 1) / 32 + 1;

	size_t copied = 0;
	UINT64 remain = cluster_num * cluster_size;
	UINT64 offset = 0;
	UINT64 next_report = 0;
	LARGE_INTEGER src_pos, dst_pos;

	for (size_t ii = 0; ii < seg_num; ++ii)
	{
		size_t copy_size = min(buf_size, remain);
		if (dwbmp[ii] != 0)
		{
			LOG_DEBUG(L"copy section=\t%d", ii);
			DWORD read, written;
			src_pos.QuadPart = offset;
			SetFilePointerEx(hsrc, src_pos, NULL, FILE_BEGIN);
			ReadFile(hsrc, buf, (DWORD)copy_size, &read, NULL);

			dst_pos.QuadPart = dst_offset + offset;
			SetFilePointerEx(hdst, dst_pos, NULL, FILE_BEGIN);
			br = WriteFile(hdst, buf, (DWORD)copy_size, &written, NULL);
			if (!br)
			{
				wprintf_s(L"failed on writing data sec:0x%llX\n", offset / 512);
				LOG_WIN32_ERROR(L"failed on writing data, sec=0x%llX", offset / 512);
			}
			if (written != copy_size)
			{
				wprintf_s(L"write size (%d) is smaller than request (%zd)\n", written, copy_size);
			}

			copied++;
		}
		remain -= buf_size;
		offset += buf_size;

		if (ii > next_report)
		{
			if (progress) progress->SetProgress(ii, seg_num);
			next_report += 10;
		}
	}
	LOG_DEBUG(L"seg=%d, copied=%d, copy=%.f%%", seg_num, copied, float(copied) / (float)seg_num * 100);

	return true;
}

class Task
{
public:
	OVERLAPPED	op;
	BYTE *		buf;
	size_t		copy_size;
	UINT64		offset;
	UINT64		seg_id;
	enum {PENDING, READING, WRITING} state;
};

typedef Task * LPTASK;

// 用于记录传输速度和温度等
class PerformanceLog
{
public:
	UINT64 seg_id;
	LONGLONG src_start_time;
	LONGLONG src_end_time;
	double src_speed;
	LONGLONG dst_start_time;
	LONGLONG dst_end_time;
	double dst_speed;
	short src_temp;
	short dst_temp;
};

bool CDiskClone::CopySectorsByMapOverlap(CCopyProgress * progress, HANDLE hsrc, HANDLE hdst, UINT64 dst_offset, UINT64 cluster_num, UINT64 cluster_size)
{
	// get bitmap
	DWORD read = 0;
	size_t bmp_size = (cluster_num - 1) / 32 + 10;
	jcvos::auto_array<DWORD> _buf_2(bmp_size, 0);

	STARTING_LCN_INPUT_BUFFER start_add = { 0 };
	BOOL br = DeviceIoControl(hsrc, FSCTL_GET_VOLUME_BITMAP,
		&start_add, sizeof(STARTING_LCN_INPUT_BUFFER), _buf_2, (DWORD)(bmp_size * sizeof(DWORD)),
		&read, NULL);
	if (!br) LOG_WIN32_ERROR(L" failed on get volume bitmap");
	VOLUME_BITMAP_BUFFER * bmp = (VOLUME_BITMAP_BUFFER *)((DWORD*)_buf_2);
	LOG_DEBUG(L"cluster_size=%dKB, cluster_num=%d, size=%dKB",
		cluster_size / 1024, cluster_num, cluster_num * cluster_size / 1024);

	// 按照bitmap的DWORD处理，一个DWORD包含32个cluster = 128KB。
	size_t buf_size = cluster_size * 32;
	DWORD * dwbmp = (DWORD*)(bmp->Buffer);
	size_t seg_num = (cluster_num - 1) / 32 + 1;

	size_t copied = 0;
	UINT64 remain = cluster_num * cluster_size;
	UINT64 offset = 0;
	UINT64 next_report = 0;
	LARGE_INTEGER src_pos, dst_pos;

	// 初始化
	// 创建buffer
	size_t total_buf_size = buf_size * OVERLAP;
	jcvos::auto_array<BYTE> total_buf(total_buf_size);
	// 初始化task array
	jcvos::auto_array<Task> task_array(OVERLAP, 0);
	jcvos::auto_array<HANDLE> waiting(OVERLAP, 0);
	for (size_t ii = 0; ii < OVERLAP; ++ii)
	{
		Task * task = task_array + ii;
		task->buf = total_buf + buf_size * ii;
		task->op.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
		if (task->op.hEvent == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on creating event %d", ii);
		waiting[ii] = task->op.hEvent;
	}

	// 初始化standby, read, write队列
	PerformanceLog * perf_log = new PerformanceLog[seg_num];
	memset(perf_log, 0, sizeof(PerformanceLog)*seg_num);
	// src和dst的温度
	short src_temp=0, dst_temp=0;
	double ts = jcvos::GetTsCycle();
	LONGLONG one_sec = (LONGLONG)(1000 * 1000 / ts);
	LONGLONG init_ts = jcvos::GetTimeStamp();
	LONGLONG cur_ts = init_ts;
	LONGLONG check_pt = cur_ts;

	LONGLONG pre_ts = cur_ts;
	UINT64 src_pre_read = 0;
	UINT64 dst_pre_write = 0;

	DWORD written = 0;
	DWORD read_sec = 0, write_sec = 0;
	size_t seg_id = 0;
	int working_task = 0;

	FILE *f = NULL;
	_wfopen_s(&f, L"performance_log.csv", L"w+");
	if (f == nullptr) LOG_ERROR(L"failed on opening file %s", L"performance_log.csv");
	fprintf_s(f, "ts(s),delta_ts(us),read_speed(MB/s),src_temp(C),write_speed(MS/s),dst_temp(C)\n");


	while (1)
	{
		cur_ts = jcvos::GetTimeStamp();
		if (cur_ts > check_pt)
		{	// 读取温度
			DEVICE_HEALTH_INFO hh;
			UINT64 src_read, dst_write;
			if (m_src_dev)
			{
				m_src_dev->GetHealthInfo(hh, NULL);
				src_temp = hh.m_temperature_cur;
				src_read = hh.m_host_read;
			}
			if (m_dst_dev)
			{
//				DEVICE_HEALTH_INFO hh;
				m_dst_dev->GetHealthInfo(hh, NULL);
				dst_temp = hh.m_temperature_cur;
				dst_write = hh.m_host_write;
			}
			double delta = (cur_ts - pre_ts) * ts;
			//double read_speed = (src_read - src_pre_read) / ts * 1024.0 * 1024.0;
			//double write_speed = (dst_write - dst_pre_write) / ts * 1024.0 * 1024.0;
			double read_speed = src_pre_read / delta;
			double write_speed = dst_pre_write / delta;
			fprintf_s(f, "%.2f,%.1f,%.1f,%d,%.1f,%d\n",
				(cur_ts-init_ts)*ts/1000.0/1000.0, delta, read_speed, src_temp, write_speed, dst_temp);
			pre_ts = cur_ts;
			src_pre_read = 0;
			dst_pre_write = 0;
			check_pt += one_sec;
		}
		// 等待event
		DWORD ir = WaitForMultipleObjects(OVERLAP, waiting, FALSE, 10000);
		if (ir == WAIT_FAILED) THROW_WIN32_ERROR(L"failed on waiting task");
		if (ir == WAIT_TIMEOUT)
		{
			LOG_WARNING(L"time out when waiting for task, working task=%d", working_task);
			continue;
		}
		else if (ir >= (WAIT_OBJECT_0 + OVERLAP)) THROW_WIN32_ERROR(L"unkown error on waiting tasks");
		// 获取event, 检查状态，empty, read, write
		Task & task = task_array[ir - WAIT_OBJECT_0];
		switch (task.state)
		{
		case Task::PENDING: {	// start for read
			LOG_DEBUG_(1, L"pending: task=%d", ir);
			//find available segment
			while ( (seg_id < seg_num) && (dwbmp[seg_id] == 0) ) seg_id++, remain-=buf_size;
			if (seg_id < seg_num)
			{
				LOG_DEBUG_(0, L"read data, task=%d, seg_id=\t%d", ir, seg_id);
				size_t copy_size = min(buf_size, remain);
				task.copy_size = copy_size;
				task.offset = seg_id * buf_size;
				remain -= buf_size;
				task.state = Task::READING;
				task.seg_id = seg_id;
				seg_id++;

				src_pos.QuadPart = task.offset;
				task.op.OffsetHigh = src_pos.HighPart;
				task.op.Offset = src_pos.LowPart;
				perf_log[task.seg_id].seg_id = task.seg_id;
				perf_log[task.seg_id].src_temp = src_temp;
				perf_log[task.seg_id].src_start_time = jcvos::GetTimeStamp();
				ReadFile(hsrc, task.buf, boost::numeric_cast<DWORD>(copy_size), &read, &(task.op));
				read_sec++;
				working_task++;
			}
			else
			{
				task.state = Task::PENDING;
				SetEvent(task.op.hEvent);
			}
			break;	}
		case Task::READING:	{	// start for write
			LOG_DEBUG_(1, L"reading: task=%d", ir);
			// check result
			//br = GetOverlappedResult(hsrc, &(task.op), &read, FALSE);
			//if (!br || read != task.copy_size)
			//{
			//	LOG_ERROR(L"[err] error in reading source file, offset=%lld, request size=%lld, read=%d",
			//		task.offset, task.copy_size, read);
			//}
			// 
			task.state = Task::WRITING;
			src_pre_read += task.copy_size;

			dst_pos.QuadPart = dst_offset + task.offset;
			task.op.Pointer = (PVOID)(dst_pos.QuadPart);

			perf_log[task.seg_id].dst_temp = dst_temp;
			perf_log[task.seg_id].dst_start_time = jcvos::GetTimeStamp();
			br = WriteFile(hdst, task.buf, boost::numeric_cast<DWORD>(task.copy_size), 
				&written, &(task.op));
			LOG_DEBUG_(1, L"write data, task=%d, seg_id=%d", ir, seg_id);
			break;	}

		case Task::WRITING: {	//  
			LOG_DEBUG_(1, L"writing: task=%d", ir);
			// check result
			//br = GetOverlappedResult(hdst, &(task.op), &written, FALSE);
			//if (!br || written != task.copy_size)
			//{
			//	LOG_ERROR(L"[err] error in writing source file, offset=%lld, request size=%lld, read=%d",
			//		task.offset, task.copy_size, written);
			//}
			write_sec++;
			perf_log[task.seg_id].dst_end_time = jcvos::GetTimeStamp();
			dst_pre_write += task.copy_size;
			//find available segment
			while (dwbmp[seg_id] == 0 && seg_id < seg_num) seg_id++, remain-=buf_size;
			if (seg_id < seg_num)
			{
				LOG_DEBUG_(0, L"read data, task=%d, seg_id=\t%d", ir, seg_id);
				size_t copy_size = min(buf_size, remain);
				task.copy_size = copy_size;
				task.offset = seg_id * buf_size;
				task.seg_id = seg_id;
				seg_id++;
				remain -= buf_size;
				task.state = Task::READING;
				copied++;

				src_pos.QuadPart = task.offset;
				task.op.Pointer = (PVOID)(src_pos.QuadPart);
				perf_log[task.seg_id].seg_id = task.seg_id;
				perf_log[task.seg_id].src_temp = src_temp;
				perf_log[task.seg_id].src_start_time = jcvos::GetTimeStamp();
				ReadFile(hsrc, task.buf, boost::numeric_cast<DWORD>(copy_size), 
					&read, &(task.op));
				read_sec++;
			}
			else
			{
				task.state = Task::PENDING;
				working_task--;
			}
			break;	}
		}
		// 分配下一个任务
		LOG_DEBUG_(0, L"seg=%d/%d, read=%d, write=%d, working=%d\n", seg_id, seg_num, 
			read_sec, write_sec, working_task);
		if ((seg_id >= seg_num) && (write_sec >= read_sec))
		{
			LOG_DEBUG(L"completed copy");
			break;
		}

		if (seg_id > next_report)
		{
			if (progress) progress->SetProgress(seg_id, seg_num);
			next_report += 10;
		}
	}
	LOG_DEBUG(L"seg=%d, copied=%d, copy=%.f%%", seg_num, copied, float(copied) / (float)seg_num * 100);


	// 输出log
/*
	FILE *f = NULL;
	_wfopen_s(&f, L"performance_log.csv", L"w+");
	if (f == NULL) LOG_ERROR(L"failed on opening file %s", L"performance_log.csv");
	fprintf_s(f, "seg,src_start,src_end,src_duration(us),read_speed(MB/s),src_temp(C),dst_start,dst_end,dst_duration(us),write_speed(MS/s),dst_temp(C)\n");
	for (UINT64 ii = 0; ii < seg_num; ++ii)
	{
		PerformanceLog & pp = perf_log[ii];
		if (pp.seg_id == 0) continue;
		double src_d = (pp.dst_start_time - pp.src_start_time)*ts;
		double dst_d = (pp.dst_end_time - pp.dst_start_time)*ts;
		double src_speed = buf_size / src_d;
		double dst_speed = buf_size / dst_d;
		fprintf_s(f, "%lld,%lld,%lld,%.1f,%.1f,%d,%lld,%lld,%.1f,%.1f,%d\n",
			pp.seg_id, pp.src_start_time, pp.dst_start_time, src_d, src_speed, pp.src_temp,
			pp.dst_start_time, pp.dst_start_time, dst_d, dst_speed, pp.dst_temp);
	}
*/
	delete [] perf_log;
	fclose(f);
	return true;
}

//bool CDiskClone::CreateSnapshotSet(
//	_Out_ IVssBackupComponents*& backup, _Out_ VSS_ID& snapshotSetId,
//	const wchar_t * vol_name, _Out_ VSS_SNAPSHOT_PROP & prop)
//{
//	JCASSERT(backup == NULL);
//	HRESULT              hres = S_OK;
//	bool                 bRetVal = true;
//	auto_unknown<IVssAsync> pPrepare;
//	auto_unknown<IVssAsync> pDoShadowCopy;
//
//	hres = CreateVssBackupComponents(&backup);      //Release if no longer needed
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"CreateVssBackupComponents failed, code=0x%x", hres);
//		return false;
//	}
//
//	hres = backup->InitializeForBackup();
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"InitializeForBackup failed, code=0x%x", hres);
//		LOG_WIN32_ERROR(L" initializing");
//		bRetVal = false;
//		goto TheEnd;
//	}
//
//	hres = backup->SetBackupState(false, false, VSS_BT_FULL);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"Set backup state failed, code=0x%x", hres);
//		bRetVal = false;
//		goto TheEnd;
//	}
//
//	hres = backup->SetContext(VSS_CTX_BACKUP);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"IVssBackupComponents SetContext failed, code=0x%x", hres);
//		bRetVal = false;
//		goto TheEnd;
//	}
//
//	wchar_t str_vol[256];
//	/////
//	hres = backup->StartSnapshotSet(&snapshotSetId);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"StartSnapshotSet failed, code=0x%x", hres);
//		bRetVal = false;
//		goto TheEnd;
//	}
//
//	VSS_ID snapshot_id;
//	wcscpy_s(str_vol, vol_name);
//	hres = backup->AddToSnapshotSet(str_vol, GUID_NULL, &snapshot_id);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"AddToSnapshotSet failed, vol_name=%s, code=0x%x", str_vol, hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//	hres = backup->SetBackupState(false, false, VSS_BT_FULL);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"SetBackupState failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//
//	hres = backup->PrepareForBackup(&pPrepare);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"PrepareForBackup failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//	LOG_NOTICE(L"Preparing for backup...");
//	hres = pPrepare->Wait();
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"IVssAsync Wait failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//	hres = backup->DoSnapshotSet(&pDoShadowCopy);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"DoSnapshotSet failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//	LOG_NOTICE(L"Taking snapshots...");
//	hres = pDoShadowCopy->Wait();
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"IVssAsync Wait failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//
//
//	LOG_NOTICE(L"Get the snapshot device object from the properties...");
//	hres = backup->GetSnapshotProperties(snapshot_id, &prop);
//	if (hres != S_OK)
//	{
//		LOG_ERROR(L"GetSnapshotProperties failed, code=0x%x", hres);
//		bRetVal = FALSE;
//		goto TheEnd;
//	}
//	bRetVal = true;
//TheEnd:
//	return bRetVal;
//}

CCopyProgress::CCopyProgress(void)
{
	InitializeCriticalSection(&m_critical);
	m_update_event = CreateEvent(NULL, TRUE, TRUE, L"");
	m_progress = 0;
	m_result = -1;
	m_thread = 0;
}

CCopyProgress::~CCopyProgress(void)
{
	DeleteCriticalSection(&m_critical);
	CloseHandle(m_update_event);
	CloseHandle(m_thread);
}

int CCopyProgress::GetProgress(void)
{
	long tmp;
	InterlockedExchange(&tmp, m_progress);
	return tmp;
}

int CCopyProgress::GetResult(void)
{
	long tmp;
	InterlockedExchange(&tmp, m_result);
	return tmp;
}

void CCopyProgress::GetStatus(std::wstring & status)
{
	EnterCriticalSection(&m_critical);
	status = m_status;
	LeaveCriticalSection(&m_critical);
}

void CCopyProgress::SetResult(long result)
{
	InterlockedExchange(&m_result, result);
}

DWORD CCopyProgress::WaitingForThread(void)
{
DWORD ir = 0;
if (m_thread) ir = WaitForSingleObject(m_thread, INFINITE);
return ir;
}

void CCopyProgress::SetStatus(const std::wstring & status)
{
	EnterCriticalSection(&m_critical);
	m_status = status;
	LeaveCriticalSection(&m_critical);
}

void CCopyProgress::SetThread(HANDLE thread)
{
	EnterCriticalSection(&m_critical);
	m_thread = thread;
	LeaveCriticalSection(&m_critical);
}

void CCopyProgress::SetProgress(long progress)
{
	InterlockedExchange(&m_progress, progress);
	SetEvent(m_update_event);
}

void CCopyProgress::SetProgress(UINT64 done, UINT64 max)
{
	long percent = 0;
	UINT64 org_done = done;
	if (done != 0)
	{
		int ss = 0;
		for (; done < 0x8000000000000000I64 && ss < 64; )
		{
			done <<= 1, ss++;
		}
		UINT64 pp = (done / max);
		if (ss >= 31)	pp >>= (ss - 31);
		else			pp <<= (31 - ss);
		percent = (long)(pp);
		//		wprintf_s(L"done=%lld, max=%lld, p0=%.3f, p=0x%08X, %.3f\n",
		//			org_done, max, (double)org_done / (double)max * 100.0, percent, (float)(percent) / float(INT_MAX) * 100.0);
	}
	SetProgress(percent);
}

int CCopyProgress::WaitForStatusUpdate(DWORD timeout, std::wstring & status, int & progress)
{
	WaitForSingleObject(m_update_event, timeout);
	int res;
	EnterCriticalSection(&m_critical);
	progress = m_progress;
	status = m_status;
	res = m_result;
	LeaveCriticalSection(&m_critical);
	return res;
}


bool CDiskClone::MakeCopyStrategy(std::vector<CopyStrategy> & strategy, IDiskInfo * src, IDiskInfo * dst, UINT64 disk_size)
{
	LOG_STACK_TRACE();
	JCASSERT(src && dst);

	// 所有容量以MB为单位，便于处理
	UINT64 src_size/*, src_fixed = 0, src_flexible = 0*/;
	UINT64 dst_size/*, dst_fixed = 0, dst_flexible =0*/;
	DISK_PROPERTY src_prop, dst_prop;
	dst->GetProperty(dst_prop);
	src->GetProperty(src_prop);

	// 检查source的partition

	// 扫描src的每个分区，计算固定容量和可变容量
	UINT part_num = src->GetParttionNum();
	jcvos::auto_array<PARTITION_ITEM> partitions(part_num + 5, 0);
	size_t pid = 0;		// partitions数组的
	src_size = 0;

	// 对于MBR系统，在dst中添加一个boot分区
	bool src_mbr = false;
	UINT32 sys_drive_index = 0;
	wchar_t sys_drive = 0;		// 系统盘的盘符
	if (src_prop.m_style == DISK_PROPERTY::PARTSTYLE_MBR)
	{
		PARTITION_ITEM & pp = partitions[pid++];
		pp.src_index = 0;
		pp.src_size = EFI_BOOT_SIZE;
		pp.src_min_size = EFI_BOOT_SIZE;
		pp.fixed = true;
		src_mbr = true;
		// <TOOD> find system partition
		wchar_t str[MAX_PATH];
		DWORD len = GetEnvironmentVariable(L"SystemDrive", str, MAX_PATH);
		if (len == 0 || len > MAX_PATH)
		{	// 如果无法获取 system drive，尝试获取 system root
			LOG_ERROR(L"[err] failed on getting SystemDrive from, res=%d", len);
			len = GetEnvironmentVariable(L"SystemRoot", str, MAX_PATH);
			if (len == 0 || len > MAX_PATH)
			{
				LOG_ERROR(L"[err] failed on getting SystemRoot from, res=%d", len);
				memset(str, 0, MAX_PATH);
			}
		}
		if (str[0] != 0) sys_drive = str[0];
	}

	int flexible = 0;
	for (UINT ii = 0; ii < part_num; ++ii)
	{
		jcvos::auto_interface<IPartitionInfo> src_part;
		src->GetPartition(src_part, ii);
		if (!src_part) continue;

		PARTITION_ITEM & partition = partitions[pid++];

		PARTITION_PROPERTY pp;
		VOLUME_PROPERTY vp;

		src_part->GetProperty(pp);
		// 忽略Micronsoft Reserved Partion
		if (!src_mbr)
		{
			if (pp.m_gpt_type == GPT_TYPE_MSR) continue;
		}

		src_part->GetVolumeProperty(vp);
		src_size += partition.src_size;
		// 获取最最小尺寸
		UINT64 min_size, max_size;
		std::wstring extended;
		src_part->GetSupportedSize(min_size, max_size, extended);
		//LOG_NOTICE(L"get source partition [%d:%d] letter=%s, offset=%lld, size=%lld, min size=%lld",
		//	src_prop.m_index, pp.m_index, pp.m_drive_letter.c_str(), pp.m_offset, pp.m_size, min_size);

		partition.src_size = ByteToMB_Up(pp.m_size);
//		partition.src_min_size = ByteToMB(min_size);
		partition.src_min_size = ByteToMB_Up(min_size);

		partition.src_index = pp.m_index;
		partition.fixed = true;

		//ss.m_fixed_size = true;
		if (!vp.m_drive_letter.empty())
		{
			if (vp.m_filesystem == L"NTFS" || vp.m_filesystem == L"RAW")
			{
				partition.fixed = false;
				flexible++;
			}
			// 检查是否是windows系统盘
			if (pp.m_drive_letter[0] == sys_drive)
			{	// 根据盘符判定系统盘
				partition.system = true;
				sys_drive_index = pp.m_index;
				LOG_NOTICE(L"found system partition index=%d volume=%s\n", 
					pp.m_index, vp.m_name.c_str());			
			}
			//partition.system = CheckWindowsDrive(src_part);
			//if (partition.system)	LOG_NOTICE(L"found system partition index=%d volume=%s\n", 
			//	pp.m_index, vp.m_name.c_str());
		}
		LOG_NOTICE(L"source partition[%d] size=%lld MB, min size=%lld MB, fixed=%d, system=%d",
			pp.m_index,	partition.src_size, partition.src_min_size, partition.fixed, partition.system);

		//if (!partition.fixed) flexible++;
		//{
		//	src_fixed += partition.src_size;
		//	partition.dst_size = partition.src_size;
		//	dst_fixed += partition.dst_size;
		//}
		//else
		//{
		//	src_flexible += partition.src_size;
		//	flexible++;
		//}
	}

	size_t valid_partition = pid;
	if (pid == 0)
	{
		LOG_ERROR(L"[err] there is no sorce partition.");
		return false;
	}

	if (flexible == 0)
	{	// <TODO> 固定分配


	}
	else
	{	// 动态规划方案配置策略
		dst_size = ByteToMB_Down(dst_prop.m_size);
		if (disk_size > dst_size) disk_size = dst_size;
		// 设置末尾guide
		bool br = PartitionAssign(partitions, valid_partition, MSR_SIZE, disk_size);
	}

	// 生成列表
	CopyStrategy ss;
	ss.m_op = CopyStrategy::CLEAR_DISK;
	strategy.push_back(ss);
	for (UINT ii = 0; ii < valid_partition; ++ii)
	{
//		CopyStrategy ss;
		memset(&ss, 0, sizeof(CopyStrategy));
		PARTITION_ITEM & pp = partitions[ii];

		if (pp.src_index == 0 && pp.fixed)
		{	// EFI BOOT PARTITION
			ss.m_op = CopyStrategy::CREATE_BOOT_PARTITION;
			src_mbr = true;
		}
		else
		{
			ss.m_op = CopyStrategy::COPY_PARTITION;
			if ((pp.fixed) || (pp.src_size == pp.dst_size))	ss.m_op |= CopyStrategy::COPY_NO_CHANGE;
			else if (pp.dst_size < pp.src_size)				ss.m_op |= CopyStrategy::COPY_SHRINK;
			else											ss.m_op |= CopyStrategy::COPY_EXPAND;
			ss.m_src_part = pp.src_index;

			jcvos::auto_interface<IPartitionInfo> src_part;
			src->GetPartition(src_part, ii);
			if (!src_part ) continue;
			PARTITION_PROPERTY prop;
			src_part->GetProperty(prop);
			if (!src_mbr)
			{
				//ss.dst_type = prop.m_gpt_type;
				//ss.src_guid = prop.m_guid;
			}
		}
		ss.src_size = pp.src_size;
		ss.dst_size = pp.dst_size;
		ss.dst_offset = pp.dst_offset;
		if (pp.system) ss.m_op |= CopyStrategy::SYS_PARTITION;
//		ss.dst_type = pp.src_type;
//		ss.m_sys_drive;
		strategy.push_back(ss);
		LOG_NOTICE(L"Cal: Part[%d] src size=%lld, dst size=%lld", pp.src_index, pp.src_size, pp.dst_size);
	}
	memset(&ss, 0, sizeof(CopyStrategy));
	if (src_mbr)
	{
		ss.m_op = CopyStrategy::MAKE_BOOT;
		ss.m_src_part = sys_drive_index;
	}
	else
	{
		ss.m_op = CopyStrategy::COPY_GUID;
		//ss.src_guid = src_prop.m_guid;		// src disk的guid
	}
	strategy.push_back(ss);
	return true;
}

bool CDiskClone::CopyDisk(CCopyProgress * progress, std::vector<CopyStrategy> & strategies, IDiskInfo * src, IDiskInfo * dst)
{
	JCASSERT(progress);
	UINT32 boot_index = 0, sys_index = 0;
	UINT64 dst_offset = 0;

	dst->Online(true);
	UINT part_num = boost::numeric_cast<UINT>(strategies.size());
	jcvos::auto_array<GUID> dst_guid(part_num, 0);

	// 分配dst的容量
	for (auto it = strategies.begin(); it != strategies.end(); ++it)
	{
		CopyStrategy & strategy = *it;
		//GUID type;
		UINT32 index;
		jcvos::auto_interface<IPartitionInfo> src_part;
		jcvos::auto_interface<IPartitionInfo> dst_part;

		switch (strategy.m_op & CopyStrategy::OPERATION)
		{
		case CopyStrategy::CREATE_BOOT_PARTITION: {
			if (progress)		progress->SetStatus(L"Create boot partiiton");
			LOG_NOTICE(L"Create EFI boot partiiton, size=%lldMB", strategy.dst_size);
			index = CreatePartition(dst_part, dst, dst_offset, strategy.dst_size, { 0 });
			if (index == 0) THROW_ERROR(ERR_APP, L"failed on creating boot partition");
			LOG_NOTICE(L"partition created, index=%d, offset=%lldMB, size=%lldMB", index, dst_offset, strategy.dst_size);
			boot_index = index;
			break; 		}

		case CopyStrategy::CLEAR_DISK:{	// <TODO>
			LOG_NOTICE(L"Clear target disk");
			if (progress)		progress->SetStatus(L"Clear destination disk");
			dst->ClearDisk({ 0 });
			break;  }

		case CopyStrategy::COPY_PARTITION: {
			src->GetPartition(src_part, strategy.m_src_part);
			PARTITION_PROPERTY prop;
			src_part->GetProperty(prop);

			UINT64 attr = 0;
			UINT64 dst_size;
			if ((strategy.m_op & CopyStrategy::COPY_SIZE) == CopyStrategy::COPY_EXPAND)
			{
				dst_size = strategy.src_size;
			}
			else dst_size = strategy.dst_size;

			LOG_NOTICE(L"create partition, size=%lldMB", strategy.dst_size);
			index = CreatePartition(dst_part, dst, dst_offset, dst_size, prop.m_gpt_type);
			if (index == 0) THROW_ERROR(ERR_APP, L"failed on creating partition");
			LOG_NOTICE(L"partition created, index = %d, offset=%lldMB, size=%lldMB", index,
				dst_offset, strategy.dst_size);
			if (dst_offset != strategy.dst_offset) THROW_ERROR(ERR_APP, L"offset does not match, expected=%lldMB, assigned=%lldMB",
				strategy.dst_offset, dst_offset);
			//strategy.dst_index = index;
			JCASSERT(index < part_num);
			dst_guid[index] = prop.m_guid;
			//if (strategy.m_sys_drive) sys_drive_index = index;

			// copy partition
			wchar_t str_status[32];
			swprintf_s(str_status, L"Copy parttion [%d] ...", index);
			if (progress) progress->SetStatus(str_status);
			if ((strategy.m_op & CopyStrategy::COPY_METHOD) == CopyStrategy::COPY_BY_SECTOR)
			{
				CopyPartition(progress, src_part, dst_part);
			}
			else //if ((strategy.m_op & CopyStrategy::COPY_METHOD) == CopyStrategy::COPY_VOLUME)
			{
				if ((strategy.m_op & CopyStrategy::COPY_SIZE) == CopyStrategy::COPY_SHRINK)
				{
					src_part->Resize(dst_size);

					jcvos::auto_interface<IVolumeInfo> src_vol;
					bool br = src_part->GetVolume(src_vol);
					if (!br || !src_vol) THROW_ERROR(ERR_APP, L"failed on getting source volume");
					jcvos::auto_interface<IVolumeInfo> shadow_vol;
					br = src_vol->CreateShadowVolume(shadow_vol);
					if (!br || !shadow_vol ) THROW_ERROR(ERR_APP, L"failed on getting shadow");
#ifndef _IGNORE_COPY
					CopyVolumeBySector(progress, shadow_vol, dst_part, false, true);
#endif
					src_part->Resize(strategy.src_size);
				}
				else
				{	// size- expand 或者 no change
					jcvos::auto_interface<IVolumeInfo> src_vol;
					bool br = src_part->GetVolume(src_vol);
					if (!br || !src_vol) THROW_ERROR(ERR_APP, L"failed on getting source volume");
					jcvos::auto_interface<IVolumeInfo> shadow_vol;
					br = src_vol->CreateShadowVolume(shadow_vol);
					if (!br || !shadow_vol) THROW_ERROR(ERR_APP, L"failed on getting shadow");
#ifndef _IGNORE_COPY
					CopyVolumeBySector(progress, shadow_vol, dst_part, false, true);
#endif
					if ((strategy.m_op & CopyStrategy::COPY_SIZE) == CopyStrategy::COPY_EXPAND)
					{
						LOG_NOTICE(L"expand destination partition to %lldMB", strategy.dst_size);
						dst_part->Resize(MbToByte(strategy.dst_size));
					}
				}
			}
			if (strategy.m_op & CopyStrategy::SYS_PARTITION) sys_index = index;
			break; }

		case CopyStrategy::MAKE_BOOT: {
			MakeBootPartitionForMBRSource(dst, boot_index, sys_index);
			break; }

		case CopyStrategy::COPY_GUID: {
			dst->CacheGptHeader();
			for (UINT ii = 0; ii < part_num; ++ii)
			{
				if (dst_guid[ii] != GUID({ 0 })) dst->SetPartitionId(dst_guid[ii], ii);
			}
			DISK_PROPERTY dprop;
			src->GetProperty(dprop);
			dst->SetDiskId(dprop.m_guid);
			dst->UpdateGptHeader();
			dst->Online(false);
			break;		}
		}
	}
	return true;
}

void CDiskClone::SetDevice(IStorageDevice * src, IStorageDevice * dst)
{
	ClearDevice();
	m_src_dev = src;
	if (m_src_dev) m_src_dev->AddRef();
	m_dst_dev = dst;
	if (m_dst_dev) m_dst_dev->AddRef();
}

void CDiskClone::ClearDevice(void)
{
	RELEASE(m_src_dev);
	RELEASE(m_dst_dev);
}

UINT32 CDiskClone::CreatePartition(IPartitionInfo * &part, IDiskInfo * disk,
	/*INOUT*/ UINT64 & offset_mb, /*INOUT*/ UINT64 & size_mb, const GUID &type)
{
	LOG_NOTICE(L"request offset=%lld MB, size=%lld MB", offset_mb, size_mb);
	UINT64 size_byte = MbToByte(size_mb);
	bool br = disk->CreatePartition(part, UINT64(-1), size_byte, L"", type, { 0 }, 0);
	if (!br || !part)
	{
		LOG_ERROR(L"[err] failed on create partition");
		offset_mb += size_mb;
		return 0;
	}
	PARTITION_PROPERTY part_prop;
	part->GetProperty(part_prop);
	size_mb = ByteToMB_Down(part_prop.m_size);
	UINT64 res_offset = ByteToMB_Down(part_prop.m_offset);
	offset_mb =  res_offset/* + size_mb*/;
	LOG_NOTICE(L"result offset=%lld MB, size=%lld MB", res_offset, size_mb);
	return part_prop.m_index;
}


bool CDiskClone::CheckWindowsDrive(IPartitionInfo * part)
{
	jcvos::auto_interface<IVolumeInfo> vol;
	part->GetVolume(vol);
	VOLUME_PROPERTY vol_prop;
	vol->GetProperty(vol_prop);

	std::wstring win_path = vol_prop.m_name + L"Windows";
	HANDLE hwin = CreateFile(win_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hwin == nullptr || hwin == INVALID_HANDLE_VALUE) return false;
	CloseHandle(hwin);

	std::wstring sys_path = win_path + L"\\System32";
	HANDLE hsys = CreateFile(sys_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hsys == nullptr || hsys == INVALID_HANDLE_VALUE) return false;
	CloseHandle(hsys);
	return true;
}