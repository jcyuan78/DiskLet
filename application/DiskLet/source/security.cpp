#include "pch.h"

#include "security.h"

const TCG_UID& SecureLet::SpToUid(SecureLet::TCG_SP sp)
{
	switch (sp)
	{
	case SecureLet::TCG_SP::TCG_SP_THISSP: return OPAL_THISSP_UID;
	case SecureLet::TCG_SP::TCG_SP_ADMINSP: return OPAL_ADMINSP_UID;
	case SecureLet::TCG_SP::TCG_SP_LOCKINGSP: return OPAL_LOCKINGSP_UID;

	}
	return OPAL_UID_HEXFF;
}


const TCG_UID& SecureLet::AuthorityToUid(SecureLet::TCG_AUTHORITY au)
{
	switch (au)
	{
	case SecureLet::TCG_AUTHORITY::TCG_SID: return OPAL_SID_UID;
	case SecureLet::TCG_AUTHORITY::TCG_NONE: return OPAL_UID_HEXFF;
	}
	return OPAL_UID_HEXFF;
}

const TCG_UID& SecureLet::ObjToUid(TCG_OBJ obj)
{
	// TODO: 在此处插入 return 语句
	switch (obj)
	{
	case SecureLet::TCG_OBJ::TCG_SP_THISSP: return OPAL_THISSP_UID;
	case SecureLet::TCG_OBJ::TCG_SP_ADMINSP: return OPAL_ADMINSP_UID;
	case SecureLet::TCG_OBJ::TCG_SP_LOCKINGSP: return OPAL_LOCKINGSP_UID;
	case SecureLet::TCG_OBJ::TCG_SID: return OPAL_SID_UID;
	case SecureLet::TCG_OBJ::TCG_NONE: return OPAL_UID_HEXFF;
	case SecureLet::TCG_OBJ::TCG_C_PIN_MSID:	return OPAL_C_PIN_MSID;
	case SecureLet::TCG_OBJ::TCG_C_PIN_SID:	return OPAL_C_PIN_SID;


	}
	return OPAL_UID_HEXFF;
}
