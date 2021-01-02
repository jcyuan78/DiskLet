#pragma once

#include "../include/idiskinfo.h"
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>

class CStorageManager : public IStorageManager
{
public:
	CStorageManager(void);
	virtual ~CStorageManager(void);
	friend class CDiskInfo;
	friend class CPartitionInfo;
	friend class CVolumeInfo;
	friend class CWmiObjectBase;

public:
	// dll=true：在DLL中调用，不用初始化COM
	virtual bool Initialize(bool dll = false);
	virtual bool ListDisk(void);
	virtual UINT GetPhysicalDiskNum(void) { return (UINT)(m_disk_list.size()); }
	virtual bool GetPhysicalDisk(IDiskInfo * & disk, UINT index);

protected:

//
protected:
	IWbemServices * m_services;
	IVssBackupComponents * m_backup;
	ObjectList<CDiskInfo> m_disk_list;
};
