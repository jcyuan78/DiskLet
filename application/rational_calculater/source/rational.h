#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 有理数分式计算问题

using namespace System;
using namespace System::Management::Automation;

namespace rational {

	class CRational
	{
	public:
		//整数
		CRational(int ii) { m_mol = ii; m_den = 1; }
		// 整数附加分数
		CRational(int ii, int mm, UINT dd) { SetValue(ii, mm, dd); };
		// 小数及循环小数
		CRational(int int_part, int fix_part, int fix_len, int loop_part, int loop_len)
		{
			SetValue(int_part, fix_part, fix_len, loop_part, loop_len);
		}

		CRational(const CRational& rat) { m_mol = rat.m_mol; m_den = rat.m_den; }

	public:
		void SetValue(int ii, int mm, UINT dd);		// 按整数附加分数形式
		void SetValue(int int_part, int fix_part, int fix_len, int loop_part, int loop_len);	//按循环小数形式

		const CRational operator + (const CRational& a) const
		{
			CRational c(*this);		c.Add(a);		return c;
		}
		static CRational * Convert(const wchar_t * & start, const wchar_t * end);

		void ToLoopDecimal(std::wstring& int_part, std::wstring& fix_part, std::wstring& loop_part);

	public:
		void Add(const CRational& a);
		void Sub(const CRational& a);
		void Mul(const CRational& a);
		void Dev(const CRational& a);
		void Simplify(void);

	// 整数算法
	public:
		int GCD(int a, int b);		// 求最大公约数

	public:
		int		m_mol;	// 分子
		UINT	m_den;	// 分母

	};

	// 定义有理数
	public value class Rational :public System::IFormattable// public System::ValueType
	{
	public:
		Rational(const Rational^ rat) { m_rat = new CRational(*rat->m_rat);  }
		Rational(const CRational* rat) { m_rat = new CRational(*rat); };
		Rational(int ii, int mm, int dd) { m_rat = new CRational(ii, mm, dd); }
		Rational(int ii) { m_rat = new CRational(ii); }
		Rational(int int_part, int fix_part, int fix_len, int loop_part, int loop_len)
		{
			m_rat = new CRational(0);
			m_rat->SetValue(int_part, fix_part, fix_len, loop_part, loop_len);
		}
//		~Rational(void) { delete m_rat; }
//		!Rational(void) { delete m_rat; }

	public:
		static Rational^ operator + (Rational^ r1, Rational^ r2)
		{
			Rational^ r3 = gcnew Rational(r1);
			r3->m_rat->Add(*r2->m_rat);
			r3->m_rat->Simplify();
			return r3;
		}
		static Rational^ operator - (Rational^ r1, Rational^ r2)
		{
			Rational^ r3 = gcnew Rational(r1);
			r3->m_rat->Sub(*r2->m_rat);
			r3->m_rat->Simplify();
			return r3;
		}
		static Rational^ operator * (Rational^ r1, Rational^ r2)
		{
			Rational^ r3 = gcnew Rational(r1);
			r3->m_rat->Mul(*r2->m_rat);
			r3->m_rat->Simplify();
			return r3;
		}
		static Rational^ operator / (Rational^ r1, Rational^ r2)
		{
			Rational^ r3 = gcnew Rational(r1);
			r3->m_rat->Dev(*r2->m_rat);
			r3->m_rat->Simplify();
			return r3;
		}

		virtual String^ ToString(System::String^ format, IFormatProvider^ formatprovider);
		virtual String^ ToString(System::String^ format) { return ToString(format, nullptr); }

		
		//整数部分，分子，分母
		//void SetValue(int ii, int mm, UINT dd);
		//void SetValue(int int_part, int fix_part, int fix_len, int loop_part, int loop_len);
		static Rational^ Set(const wchar_t * & start, const wchar_t * end);
		static Rational ^ Set(String ^ str);
	protected:
		CRational* m_rat;


	};



	[CmdletAttribute(VerbsData::ConvertTo, "Rational")]
	public ref class ConvertToRational : public JcCmdLet::JcCmdletBase
	{
	public:
		ConvertToRational(void) {};
		~ConvertToRational(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property System::String^ input;


	public:
		virtual void BeginProcessing()	override
		{
		}
		virtual void EndProcessing()	override
		{
		}
		virtual void InternalProcessRecord() override
		{
			//int len = input->Length;
			//const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(input)).ToPointer();
			//const wchar_t* end = wstr + len;
			Rational^ r = Rational::Set(input);
			//System::Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)wstr));
			WriteObject(r);
		}

	protected:
	};

	[CmdletAttribute(VerbsData::Update, "Rational")]
	public ref class CalculateRational : public JcCmdLet::JcCmdletBase
	{
	public:
		CalculateRational(void) {};
		~CalculateRational(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property System::String^ input;


	public:
		virtual void BeginProcessing()	override
		{
		}
		virtual void EndProcessing()	override
		{
		}
		virtual void InternalProcessRecord() override;

	protected:
	};

}