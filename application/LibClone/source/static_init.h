#pragma once

#include "../../StorageManagementLib/storage_management_lib.h"



class CStaticInit
{
public:
	CStaticInit(void);
	~CStaticInit(void) {};

protected:
	//CSoftwareAuthorize * m_authorize;
	GUID m_module_guid;
};

extern CStaticInit global_init;

extern bool authorized;