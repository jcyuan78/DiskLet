#pragma once

#include "tinyxml2.h"

using namespace System;
using namespace System::Management::Automation;

namespace htmlparser {

	//ref class HtmlElement;

	//typedef array<HtmlElement^> HtmlElementList;

	public ref class HtmlElement : public System::Object
	{
	public:
		HtmlElement(tinyxml2::XMLElement* ele) { m_element = ele; }
		~HtmlElement(void) {};
		!HtmlElement(void) {};

	public:
		//HtmlElement^ FindTag(String^ tag_name, String^ id)
		//{
		//	JCASSERT(m_element);
		//	std::wstring wstr;
		//	ToStdString(wstr, tag_name);
		//	std::vector<tinyxml2::XMLElement*> eles;
		//	m_element->FindAllElements(eles, wstr);
		//	if (eles.empty()) return nullptr;
		//	else
		//	{
		//		auto it = eles.begin();
		//		return gcnew HtmlElement(*it);
		//	}
		//}
		System::Collections::ArrayList^ FindAllTags(String^ tag_name)
		{
			JCASSERT(m_element);
			std::wstring wstr;
			ToStdString(wstr, tag_name);
			std::vector<tinyxml2::XMLElement*> eles;
			m_element->FindAllElements(eles, wstr);
//			HtmlElementList^ list = gcnew HtmlElementList;
			System::Collections::ArrayList^ list = gcnew System::Collections::ArrayList;
			for (auto it = eles.begin(); it != eles.end(); ++it)
			{
				list->Add(gcnew HtmlElement(*it));
			}
			return list;
		}


	public:
		property String^ name {String^ get() {
			JCASSERT(m_element);
			const wchar_t * nn = m_element->Name();
			return gcnew String(nn);
		}}

		property String^ text {String^ get() {
			JCASSERT(m_element);
			const std::wstring & str = m_element->GetContents();
			return gcnew String(str.c_str());

		}}

	protected:
		tinyxml2::XMLElement* m_element;
	};

	public ref class HtmlDoc : public System::Object
	{
	public:
		HtmlDoc(tinyxml2::XMLDocument* doc) { m_doc = doc; };
		~HtmlDoc(void) { delete m_doc; };
		!HtmlDoc(void) { delete m_doc; };

	public:
		HtmlElement^ GetRoot(void)
		{
			tinyxml2::XMLElement* ele = m_doc->RootElement();
			return gcnew HtmlElement(ele);
		}

		System::Collections::ArrayList^ FindAllTags(String^ tag_name)
		{
			std::wstring wstr;
			ToStdString(wstr, tag_name);
			std::vector<tinyxml2::XMLElement*> eles;

			tinyxml2::XMLElement* ele = m_doc->RootElement();
			while (ele)
			{
				ele->FindAllElements(eles, wstr);
				ele = ele->NextSiblingElement();
			}
			System::Collections::ArrayList^ list = gcnew System::Collections::ArrayList;
			for (auto it = eles.begin(); it != eles.end(); ++it)
			{
				list->Add(gcnew HtmlElement(*it));
			}			
			return list;
		}

	protected:
		tinyxml2::XMLDocument* m_doc;
	};


	[CmdletAttribute(VerbsData::ConvertFrom, "html")]
	public ref class ParseHtml : public JcCmdLet::JcCmdletBase
	{
	public:
		ParseHtml(void) {};
		~ParseHtml(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property System::String^ html;


	public:
		virtual void BeginProcessing()	override
		{
		}
		virtual void EndProcessing()	override
		{
//			std::string src;
			const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(m_contents)).ToPointer();
//			jcvos::UnicodeToUtf8(src, wstr);
			tinyxml2::XMLDocument * doc = new tinyxml2::XMLDocument;
			tinyxml2::XMLError ir = doc->Parse(wstr);
			if (ir != tinyxml2::XML_SUCCESS)
			{
				wprintf_s(L"err=%s", doc->ErrorStr());
			}

			System::Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)wstr));

			HtmlDoc^ dd = gcnew HtmlDoc(doc);
//			tinyxml2::XMLElement* ele = doc->RootElement();
//			if (ele)
//			HtmlElement ^ele = 
			WriteObject(dd);
		}
		virtual void InternalProcessRecord() override
		{
//			const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(html)).ToPointer();
//			m_contents += wstr;
//			System::Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)wstr));
			m_contents += html;

		}

	protected:
		String ^ m_contents;
		//LBA_INFO* m_lba_mapping;
		//// map的大小，以8 sec (4KB）为单位;
		//size_t m_map_size;
		//uint64_t m_first_cluster;
		//FILE_INFO* m_cur_file_info;
	};


}