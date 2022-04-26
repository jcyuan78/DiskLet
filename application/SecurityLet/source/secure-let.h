///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <jccmdlet-comm.h>
#include <LibTcg.h>

#include "global.h"

using namespace System;
using namespace System::Management::Automation;
//using namespace DiskLet;

namespace SecureLet
{

	[CmdletAttribute(VerbsData::Initialize, "SecurityParser")]
	public ref class InitSecurityParser : public JcCmdLet::JcCmdletBase
	{
	public:
		InitSecurityParser(void) {};
		~InitSecurityParser(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true, HelpMessage = "uuid config filename.json")]
		property String^ uuid;

		[Parameter(Position = 1, Mandatory = true, HelpMessage = "l0 discovery config filename.json")]
		property String^ l0discovery;

	public:
		virtual void InternalProcessRecord() override;
	};


			//[CmdletAttribute(VerbsCommon::Get, "L0Discovery")]
			//public ref class GetL0Discovery : public JcCmdLet::JcCmdletBase
			//{
			//public:
			//	GetL0Discovery(void);
			//	~GetL0Discovery(void) {};

			//public:
			//public:
			//	//[Parameter(/*Position = 0, ParameterSetName = "ByObject",*/
			//	//	/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = false,
			//	//	HelpMessage = "specify disk")]
			//	//property Clone::DiskInfo^ Disk;

			//	//[Parameter(/*Position = 0, */Mandatory = false, /*ParameterSetName = "ByIndex",*/
			//	//	/*ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,*/
			//	//	HelpMessage = "Clear a disk")]
			//	//property int DiskNumber;

			//	[Parameter(Position = 0, 
			//		ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			//		HelpMessage = "specify device object")]
			//	property Clone::StorageDevice^ dev;

			//public:
			//	virtual void InternalProcessRecord() override
			//	{
			//		jcvos::auto_interface<IStorageDevice> dd;
			//		if (dev) dev->GetStorageDevice(dd);
			//		else		global.GetDevice(dd);
			//		if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

			//		jcvos::auto_interface<ITcgSession> tcg;
			//		CreateTcgSession(tcg, dd);
			//		if (!tcg) throw gcnew System::ApplicationException(L"failed on creating tcg session");

			//		//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
			//		//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");

			//		jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			//		jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(1));
			//		BYTE* _buf = buf->Lock();
			//		bool br = tcg->L0Discovery(_buf);
			//		buf->Unlock(_buf);
			//		if (!br) wprintf_s(L"[err] failed on calling L0Discovery");

			//		JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
			//		WriteObject(data);

			//	}

			//protected:

			//};


};


