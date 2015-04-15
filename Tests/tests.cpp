#include "stdafx.h"
#include "CppUnitTest.h"
#include "../MinHookEx/MinHookEx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace MinHookEx;

auto &hooks = CMinHookEx::getInstance();

__declspec(noinline)
int sumImplCDECL(int a, int b)
{
	return a + b;
}

__declspec(noinline)
int __cdecl sumCDECL(int a, int b)
{
	return a + b;
}

__declspec(noinline)
int __stdcall sumSTDCALL(int a, int b)
{
	return a + b;
}

__declspec(noinline)
int __fastcall sumFASTCALL(int a, int b)
{
	return a + b;
}

_declspec(noinline)
int __fastcall sumIdenticalSigFASTCALL(int a, int b)
{
	return a + b;
}

struct S
{
	int sum(int a, int b)
	{
		return a + b;
	}
	int __cdecl sumCDECL(int a, int b)
	{
		return a + b;
	}
	int __stdcall sumSTDCALL(int a, int b)
	{
		return a + b;
	}
	int __fastcall sumFASTCALL(int a, int b)
	{
		return a + b;
	}
	int __thiscall sumTHISCALL(int a, int b)
	{
		return a + b;
	}
	static int sumStatic(int a, int b)
	{
		return a + b;
	}
};

namespace Tests
{		
	TEST_CLASS(GlobalFunctions)
	{
	public:
		
		TEST_METHOD(CDECL_ImplCC)
		{
			hooks.add(FunctionHook, sumImplCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumImplCDECL(1, 2), 1);
			Assert::AreEqual(hooks[sumImplCDECL]->originalFunc(1,2), 3);
		}
		TEST_METHOD(CDECL_CC)
		{
			hooks.add(FunctionHook, sumCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumCDECL(1, 2), 1);
			Assert::AreEqual(hooks[sumCDECL]->originalFunc(1, 2), 3);
		}
		TEST_METHOD(STDCALL_CC)
		{
			hooks.add(FunctionHook, sumSTDCALL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumSTDCALL(1, 2), 1);
			Assert::AreEqual(hooks[sumSTDCALL]->originalFunc(1, 2), 3);
		}
		TEST_METHOD(FASTCALL_CC)
		{
			hooks.add(FunctionHook, sumFASTCALL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumFASTCALL(1, 2), 1);
			Assert::AreEqual(hooks[sumFASTCALL]->originalFunc(1, 2), 3);
		}
		TEST_METHOD(FASTCALL_CC_IdentSig)
		{
			hooks.add(FunctionHook, sumIdenticalSigFASTCALL, [](int a, int b){return 10; }).enable();
			Assert::AreEqual(sumIdenticalSigFASTCALL(1, 2), 10);
			Assert::AreEqual(hooks[sumIdenticalSigFASTCALL]->originalFunc(10, 20), 30);
		}

	};

	TEST_CLASS(MemberFunctions)
	{
	private:
		S s;
		TEST_METHOD(DEFAULT_CC)
		{
			hooks.add(MethodHook, &S::sum, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sum(1, 2), 1);
			Assert::AreEqual(hooks[&S::sum]->originalMethod(&s)(1,2), 3);
		};


		TEST_METHOD(CDECL_CC)
		{
			hooks.add(MethodHook, &S::sumCDECL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumCDECL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumCDECL]->originalMethod(&s)(1, 2), 3);
		};

		TEST_METHOD(STDCALL_CC)
		{
			hooks.add(MethodHook, &S::sumSTDCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumSTDCALL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumSTDCALL]->originalMethod(&s)(1, 2), 3);
		};

		TEST_METHOD(FASTCALL_CC)
		{
			hooks.add(MethodHook, &S::sumFASTCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumFASTCALL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumFASTCALL]->originalMethod(&s)(1, 2), 3);
		};

		TEST_METHOD(THISCALL_CC)
		{
			hooks.add(MethodHook, &S::sumTHISCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumTHISCALL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumTHISCALL]->originalMethod(&s)(1, 2), 3);
		};

		TEST_METHOD(STATIC_METHOD)
		{
			hooks.add(StaticMethodHook, &S::sumStatic, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumStatic(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumStatic]->originalFunc(1, 2), 3);
		};

	};
}