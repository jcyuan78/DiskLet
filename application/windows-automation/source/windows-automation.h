#pragma once

using namespace System::Management::Automation;

//extern "C"
//{
//	BOOL GetCursorPos(LPPOINT lpPoint);
//}

namespace WAPP
{
#if 0
	[CmdletAttribute(VerbsCommon::Select, "Window")]
	public ref class CmdSelectWindow : public JcCmdLet::JcCmdletBase
	{
	public:
		CmdSelectWindow(void);
		~CmdSelectWindow(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "by process name")]
		property String^ ProcessNames;

		[Parameter(Position = 1, Mandatory = true,
			HelpMessage = "disk size, in sector")]
		property String^ WindowTitles;

		[Parameter(Position = 2, Mandatory = true,
			HelpMessage = "offset of the device")]
		property uint64_t first_lba;

	public:
		virtual void BeginProcessing()	override;
		virtual void EndProcessing()	override;
		virtual void InternalProcessRecord() override
		{
            switch (ParameterSetName)
            {
            case "ForegroundWindow":
                WriteObject(new Window(NativeMethods.GetForegroundWindow()));
                break;
            case "ProcessName":
                if (processNames == null) { goto default; }
                foreach(Window w in GetWindowsByProcessName(processNames))
                {
                    WriteObject(w);
                }
                break;
            case "WindowTitle":
                foreach(Window w in GetWindowsByWindowTitle(windowTitles))
                {
                    WriteObject(w);
                }
                break;
            case "InputObject":
                foreach(Process proc in processes)
                {
                    foreach(Window w in GetWindowsOfProcess(proc))
                    {
                        WriteObject(w);
                    }

                }
                break;
            case "WindowClass":
                foreach(Window w in GetWindowsByClass(windowClasses))
                {
                    WriteObject(w);
                }
                break;

            default:

                WriteVerbose("Enumerating all windows");
                foreach(Window w in Huddled.Wasp.WindowFinder.GetTopLevelWindows())
                {
                    WriteObject(w);
                }
                break;
            } // switch (ParameterSetName...
		}

	protected:
	};
#endif

	[CmdletAttribute(VerbsCommon::Get, "CursorPos")]
	public ref class CmdGetCursorPos : public JcCmdLet::JcCmdletBase
	{
	public:
		CmdGetCursorPos(void) {};
		~CmdGetCursorPos(void) {};

	public:
		//[Parameter(Position = 0, ValueFromPipeline = true,
		//	ValueFromPipelineByPropertyName = true, Mandatory = true,
		//	HelpMessage = "by process name")]
		//property String^ ProcessNames;

		//[Parameter(Position = 1, Mandatory = true,
		//	HelpMessage = "disk size, in sector")]
		//property String^ WindowTitles;

		//[Parameter(Position = 2, Mandatory = true,
		//	HelpMessage = "offset of the device")]
		//property uint64_t first_lba;

	public:
		virtual void InternalProcessRecord() override
		{
			for (int ii = 0; ii < 30; ++ii)
			{
				POINT pp;
				GetCursorPos(&pp);
				wprintf_s(L"cur[%d] = (%d,%d)\n", ii, pp.x, pp.y);
				Sleep(500);
			}
		}
	};

	[CmdletAttribute(VerbsCommon::Set, "CursorPos")]
	public ref class CmdSetCursorPos : public JcCmdLet::JcCmdletBase
	{
	public:
		CmdSetCursorPos(void) { action = L""; };
		~CmdSetCursorPos(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "x position")]
		property int x;

		[Parameter(Position = 1, Mandatory = true,
			HelpMessage = "y position")]
		property int y;

		[Parameter(Position = 2, 
			HelpMessage = "mouse click")]
		property String^ action;

	public:
		virtual void InternalProcessRecord() override
		{
			if (action == L"move")
			{
				SetCursorPos(x, y);
			}
			else if (action == L"left")
			{
				mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
				Sleep(50);
				mouse_event(MOUSEEVENTF_LEFTUP, x, y, 0, 0);
			}
		}
	};
}