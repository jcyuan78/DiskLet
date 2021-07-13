#include "pch.h"

#include "rational.h"

using rational::Rational;
using rational::CRational;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// === utilitys ===

bool is_number(wchar_t ch)
{
	return (ch >= '0') && (ch <= '9');
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// === Rational ===

String^ rational::Rational::ToString(System::String^ format, IFormatProvider^ formatprovider)
{
	if (format == nullptr || format == "F")
	{	// 假分数形式显示
		return String::Format(L"[{0}/{1}]", m_rat->m_mol, m_rat->m_den);
	}
	else if (format == "T")
	{	// 整数加真分数形式显示
		if (abs(m_rat->m_mol) >= m_rat->m_den)
		{
			return String::Format(L"{0}[{1}/{2}]", (m_rat->m_mol / m_rat->m_den), (m_rat->m_mol % m_rat->m_den),
				m_rat->m_den);
		}
		else return String::Format(L"[{0}/{1}]", m_rat->m_mol, m_rat->m_den);
	}
	return nullptr;
}
//
//void rational::Rational::SetValue(int ii, int mm, UINT dd)
//{	// 整数，分子，分母的形式设置
//	if (dd == 0) throw gcnew System::ApplicationException(L"分母不能为 0");
//
//}

//void rational::Rational::SetValue(int int_part, int fix_part, int fix_len, int loop_part, int loop_len)
//{	// 按循环小数设置
//	// 不循环部分
//	
//
//
//}

Rational ^ rational::Rational::Set(const wchar_t*& start, const wchar_t* end)
{
	//Rational^ rat = nullptr;
	enum state
	{
		S_START, //开始解析
		S_INT, //整数+分数形式
		S_MOLECULAR, //分子
		S_DENOMINATOR, //分母

		S_DECIMAL, //小数+循环小数
		S_LOOP, //循环节

	} state;

	int value_index = 0;
	//std::wstring str_val[3];		// 3组整数，对于分数形式：整数，分子，分母；对于小数形式：整数，小数，循环节。
	int value[3];					// 各组整数的数值
	size_t length[3];				// 各组整数的字符串长度
	//std::wstring str_int, str_molecular, str_denominator, str_cur_val;
	state = S_START; // 从整数部分开始
	const wchar_t * cur_val = start;
	bool running = true;
//	while (running)
	for (; ; ++start)
	{
		if ((start!=end) && is_number(*start) ) continue;	// 读取数字部分

		length[value_index] = start - cur_val;
		value[value_index] = 0;
		if (length[value_index] > 0)
		{
			std::wstring & ss = std::wstring(cur_val, length[value_index]);
			value[value_index] = _wtoi(ss.c_str());
		}
		value_index++;
		if (start == end)
		{
			if (state == S_START) return gcnew Rational(value[0]);
			else if (state == S_DECIMAL) return gcnew Rational(value[0], value[1], length[1], 0, 0);
			else break;
		}

		switch (state)
		{
		case S_START:
			// 读取到‘.’，表示循环小数。保存整数部分。
			if (*start == '.')	state = S_DECIMAL;
			// 分数部分的开始
			else if (*start == '[')state = S_MOLECULAR;
			// 只有整数部分
			else	return gcnew Rational(value[0]);	
			break;

		case S_MOLECULAR:	//分子不能为空
			if (length[1]==0) throw gcnew System::ApplicationException(L"分子不能为空");
			if (*start == '/')	state = S_DENOMINATOR;
			else throw gcnew System::ApplicationException(L"分数中缺少 /");
			break;

		case S_DENOMINATOR: // 分母不能为空
			if (length[2]==0) throw gcnew System::ApplicationException(L"分母不能为空");
			if (*start == ']')
			{	// 转换
				start++;
				if (value[2] == 0) throw gcnew System::ApplicationException(L"分母不能为 0");
				return gcnew Rational(value[0], value[1], value[2]);
			}
			else throw gcnew System::ApplicationException(L"缺少 ]");
			break;

		case S_DECIMAL:
			if (*start == '[')	state = S_LOOP;
			//非循环小数
			else return gcnew Rational(value[0], value[1], length[1], 0, 0);
			break;

		case S_LOOP:
			if (*start == ']')
			{	// 转换
				start++;
				return gcnew Rational(value[0], value[1], length[1], value[2], length[2]);
			}
			else throw gcnew System::ApplicationException(L"缺少 ]");
			break;
		}
		cur_val = start+1;
	}
	throw gcnew System::ApplicationException(L"不完整的表达式");
	return nullptr;
}

rational::Rational ^ rational::Rational::Set(String^ str)
{
	// 分数形式
	Text::RegularExpressions::Regex^ reg_fraction = gcnew Text::RegularExpressions::Regex(
		L"([\\d]*)\\[([\\d]+)/([\\d]+)\\]");
	// 循环小数形式
	Text::RegularExpressions::Regex^ reg_decimal = gcnew Text::RegularExpressions::Regex(
		L"([\\d]+)\\.([\\d]*)\\[([\\d]+)\\]");
	auto match = reg_fraction->Match(str);
	if (match->Success)
	{
		String^ str_int = match->Groups[1]->Value;
		int int_part = 0;
		if (str_int && str_int != L"") int_part = System::Convert::ToInt32(str_int);
		int molecular = System::Convert::ToInt32(match->Groups[2]->Value);
		int denominator = System::Convert::ToInt32(match->Groups[3]->Value);
		if (denominator == 0) throw gcnew System::ApplicationException(L"分母不能为 0");
		Rational^ r = gcnew Rational(int_part, molecular, denominator);
		return r;
	}
	else
	{
		auto mm = reg_decimal->Match(str);
		if (mm->Success)
		{
			int int_part = Convert::ToInt32(mm->Groups[1]->Value);
			int fix_part = 0;
			String^ str_fix = mm->Groups[2]->Value;
			if (str_fix && str_fix != L"") fix_part = Convert::ToInt32(str_fix);
			int loop_part = 0;
			String^ str_loop = mm->Groups[3]->Value;
			if (str_loop && str_loop != "") loop_part =	Convert::ToInt32(str_loop);
			Rational^ r = gcnew Rational(0, 0, 1);
			r->m_rat->SetValue(int_part, fix_part,str_fix->Length, loop_part, str_loop->Length);
			return r;
		}
	}
	throw gcnew System::ApplicationException(L"格式错误");
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// === CRational ===

void rational::CRational::SetValue(int ii, int mm, UINT dd)
{
	if (dd == 0) THROW_ERROR(ERR_APP, L"denominator cannot be zero");
	m_den = dd;
	m_mol = ii * m_den + mm;
}

void rational::CRational::SetValue(int int_part, int fix_part, int fix_len, int loop_part, int loop_len)
{
	//不循环部分的分母
	UINT m1 = 1;
	for (int ii = 0; ii < fix_len; ++ii) m1 *= 10;
	SetValue(int_part, fix_part, m1);

	if (loop_len > 0)
	{	//循环部分
		UINT m2 = 0;
		for (int ii = 0; ii < loop_len; ++ii)
		{
			m2 *= 10;
			m2 += 9;
		}
		m2 *= m1;
		CRational r2(0, loop_part, m2);
		Add(r2);
	}
	Simplify();
}

void rational::CRational::Add(const CRational& a)
{	//分母为两个分母的最小公倍数
	int gcd = GCD(m_den, a.m_den);
	int d1 = m_den / gcd, d2 = a.m_den / gcd;
	m_den = d1 * d2 * gcd;
	m_mol = (m_mol * d2 + a.m_mol * d1);
//	Simplify();
}

void rational::CRational::Sub(const CRational& a)
{
	int gcd = GCD(m_den, a.m_den);
	int d1 = m_den / gcd, d2 = a.m_den / gcd;
	m_den = d1 * d2 * gcd;
	m_mol = (m_mol * d2 - a.m_mol * d1);
}

void rational::CRational::Mul(const CRational& a)
{
	m_mol *= a.m_mol;
	m_den *= a.m_den;
}

void rational::CRational::Dev(const CRational& a)
{
	if (a.m_mol == 0) THROW_ERROR(ERR_APP, L"devider cannot be 0");
	m_mol *= a.m_den;
	m_den *= a.m_mol;
}

void rational::CRational::Simplify(void)
{
	int gcd = GCD(m_mol, m_den);
	if (gcd > 1)
	{
		m_mol /= gcd;
		m_den /= gcd;
	}
}

int rational::CRational::GCD(int a, int b)
{
	while (b != 0)
	{
		int r = b;
		b = a % b;
		a = r;
	}
	return a;
}

CRational* CRational::Convert(const wchar_t*& start, const wchar_t* end)
{
	//Rational^ rat = nullptr;
	enum state
	{
		S_START, //开始解析
		S_INT, //整数+分数形式
		S_MOLECULAR, //分子
		S_DENOMINATOR, //分母

		S_DECIMAL, //小数+循环小数
		S_LOOP, //循环节

	} state;

	int value_index = 0;
	//std::wstring str_val[3];		// 3组整数，对于分数形式：整数，分子，分母；对于小数形式：整数，小数，循环节。
	int value[3];					// 各组整数的数值
	size_t length[3];				// 各组整数的字符串长度
	//std::wstring str_int, str_molecular, str_denominator, str_cur_val;
	state = S_START; // 从整数部分开始
	const wchar_t* cur_val = start;
	bool running = true;
	//	while (running)
	for (; ; ++start)
	{
		if ((start != end) && is_number(*start)) continue;	// 读取数字部分

		length[value_index] = start - cur_val;
		value[value_index] = 0;
		if (length[value_index] > 0)
		{
			std::wstring& ss = std::wstring(cur_val, length[value_index]);
			value[value_index] = _wtoi(ss.c_str());
		}
		value_index++;
		if (start == end)
		{
			if (state == S_START) return new CRational(value[0]);
			else if (state == S_DECIMAL) return new CRational(value[0], value[1], length[1], 0, 0);
			else break;
		}

		switch (state)
		{
		case S_START:
			// 读取到‘.’，表示循环小数。保存整数部分。
			if (*start == '.')	state = S_DECIMAL;
			// 分数部分的开始
			else if (*start == '[')state = S_MOLECULAR;
			// 只有整数部分
			else	return new CRational(value[0]);
			break;

		case S_MOLECULAR:	//分子不能为空
			if (length[1] == 0) throw gcnew System::ApplicationException(L"分子不能为空");
			if (*start == '/')	state = S_DENOMINATOR;
			else throw gcnew System::ApplicationException(L"分数中缺少 /");
			break;

		case S_DENOMINATOR: // 分母不能为空
			if (length[2] == 0) throw gcnew System::ApplicationException(L"分母不能为空");
			if (*start == ']')
			{	// 转换
				start++;
				if (value[2] == 0) throw gcnew System::ApplicationException(L"分母不能为 0");
				return new CRational(value[0], value[1], value[2]);
			}
			else throw gcnew System::ApplicationException(L"缺少 ]");
			break;

		case S_DECIMAL:
			if (*start == '[')	state = S_LOOP;
			//非循环小数
			else return new CRational(value[0], value[1], length[1], 0, 0);
			break;

		case S_LOOP:
			if (*start == ']')
			{	// 转换
				start++;
				return new CRational(value[0], value[1], length[1], value[2], length[2]);
			}
			else throw gcnew System::ApplicationException(L"缺少 ]");
			break;
		}
		cur_val = start + 1;
	}
	throw gcnew System::ApplicationException(L"不完整的表达式");
	return nullptr;
}


struct stack_item
{
public:
//	enum OP_TYPE {}	type;
	CRational* val;
	int op;
};

int priority(int op)
{
	switch (op)
	{
	case '=': return 1;
	case '(': return 2;		// 右括号不会入栈
	case '+': case'-': return 3;
	case '*': case'/': return 4;
	default: return -1;
	}
	return -1;
}

void calculate(CRational* a, const CRational* b, int op)
{
	switch (op)
	{
	case '+': a->Add(*b); break;
	case '-': a->Sub(*b); break;
	case '*': a->Mul(*b); break;
	case '/': a->Dev(*b); break;
	default: throw gcnew System::ApplicationException(L"非法符号");
	}
	a->Simplify();
}

// 取出stack头山的3个元素，进行计算，stack的头上，并且返回stack头上的元素
stack_item & calculate_stack(std::vector<stack_item>& stack)
{
	JCASSERT(stack.size() > 3);
	stack_item& b = stack.back();	stack.pop_back();
	stack_item& op = stack.back();	stack.pop_back();
	stack_item& a = stack.back();
	calculate(a.val, b.val, op.op);
	delete b.val;
	return a;
}


void rational::CalculateRational::InternalProcessRecord()
{
	int len = input->Length;
	const wchar_t* wstr = (const wchar_t*)(Runtime::InteropServices::Marshal::StringToHGlobalUni(input)).ToPointer();
	const wchar_t* start = wstr;
	const wchar_t* end = wstr + len;

	std::vector<stack_item> stack;
	stack_item cur;
	cur.val = 0;
	cur.op = '=';	// 最低优先级
	stack.push_back(cur);

	// 开始计算
	while (start < end)
	{
		stack_item cur;
		while (iswspace(*start)) start++;
		if (is_number(*start) || (*start == '[') )
		{
			cur.op = 0;			cur.val = CRational::Convert(start, end);
			stack.push_back(cur);
		}
		else if (*start == '(')
		{	// 检查前一个项目必须是符号
			stack_item& back = stack.back();
			if (back.op == 0) throw gcnew System::ApplicationException(L"非法的左括号");
			stack_item kk;
			kk.val = NULL; kk.op = '('; stack.push_back(kk);
			start++;
		}
		else if (*start == ')')
		{	//检查前一个必须是数字
			start++;
			stack_item& back = stack.back();
			if (back.op != 0) throw gcnew System::ApplicationException(L"非法的右括号");
			while (stack.size() > 2)
			{	// 一直计算直到遇到(
				stack_item& op = stack.at(stack.size() - 2);
				if (op.op == '(')
				{
					stack_item& rr = stack.back();			stack.pop_back();
					stack.pop_back();		// 移除左括号
					stack.push_back(rr);
					break;
				}
				stack_item& res = calculate_stack(stack);
			}
		}
		else
		{	// 符号
			int p0 = priority(*start);
			if (p0 < 0) throw gcnew System::ApplicationException(L"非法的运算符");
			if (stack.size() < 2) throw gcnew System::ApplicationException(L"堆栈错误");
			while (stack.size() >2)
			{
				stack_item& op = stack.at(stack.size() - 2);
				int p1 = priority(op.op);
				if (p1 < 0) throw gcnew System::ApplicationException(L"非法的运算符");
				if (p0 > p1) break;
				//弹出堆栈的前3项计算
				stack_item& res = calculate_stack(stack);
			}
			cur.op = *start;
			cur.val = NULL;
			stack.push_back(cur);
			start++;
		}
	}
	System::Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)wstr));

	while (stack.size() >= 3)
	{
		calculate_stack(stack);
	}
	stack_item& res = stack.back();

	Rational^ r = gcnew Rational(res.val);
	delete res.val;
//	stack_item& b = stack.back();
//	stack_item& op = stack.at(stack.size()-2);
//	if (stack.size() < 4 || op.op == '=')
//	{
//		r = gcnew Rational(b.val);
//	}
//	stack_item& a = stack.at(stack.size() - 3);
//	
//
//
////	Rational^ r = Rational::Set(start, end);
//	std::wstring remain(start);
	WriteObject(r);
	//wprintf_s(L"remain = %s", remain.c_str());
}
