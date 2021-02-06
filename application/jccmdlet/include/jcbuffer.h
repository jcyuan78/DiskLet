#pragma once

//#include "../include/utility.h"
#include <jccmdlet-comm.h>

using namespace System::Management::Automation;
#include <boost/cast.hpp>
#pragma make_public(jcvos::IBinaryBuffer)

namespace JcCmdLet
{
	//-----------------------------------------------------------------------------
	// -- my cmdlet base class, to handle exceptions	
	public ref class JcCmdletBase : public Cmdlet
	{
	public:
		virtual void InternalProcessRecord() {};
	protected:
//		void ShowPipeMessage(void);

	protected:
		virtual void ProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- binary type	
	public ref class BinaryType : public Object
	{
	public:
		BinaryType(jcvos::IBinaryBuffer * ibuf);
		~BinaryType(void);
		!BinaryType(void);
	public:
		property size_t Length { size_t get()
		{
			return m_data->GetSize();
		}; }

	public:
		void GetData(jcvos::IBinaryBuffer * & data) { data = m_data; data->AddRef(); }
		BYTE* LockData(void);
		void Unlock(void);
		//bool GetData(void * & data);
	protected:
		jcvos::IBinaryBuffer * m_data;
		BYTE* m_locked;
	};



};
