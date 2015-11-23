#include "stdafx.h"
#include "CppUnitTest.h"
#include "../MinHookEx/MinHookEx.h"
#include <conio.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

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
	int val;
	__declspec(noinline)
	int sum(int a, int b)
	{
		return val = a + b;
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
	__declspec(noinline)
	static int sumStatic(int a, int b)
	{
		return a + b;
	}
};

struct Base
{
	__declspec(noinline)
	virtual int m(int a, int b)
	{
		return a - b;
	}
};

struct Derived:public Base
{
	__declspec(noinline)
	virtual int m(int a, int b) override
	{
		return a + b;
	}
};

namespace Tests
{	
	TEST_CLASS(GlobalFunctions)
	{
		TEST_METHOD(CDECL_ImplCC)
		{
			hooks.addFunctionHook(sumImplCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumImplCDECL(1, 2), 1);
			Assert::AreEqual(hooks[sumImplCDECL].originalFunc(1,2), 3);
		}
		TEST_METHOD(CDECL_CC)
		{
			hooks.addFunctionHook(sumCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumCDECL(1, 2), 1);
			Assert::AreEqual(hooks[sumCDECL].originalFunc(1, 2), 3);
		}
		TEST_METHOD(STDCALL_CC)
		{
			Assert::IsTrue(hooks.addFunctionHook(sumSTDCALL, [](int a, int b){return 1; }).enable());
/*
			Assert::AreEqual(sumSTDCALL(1, 2), 1);
			Assert::AreEqual(hooks[sumSTDCALL].originalFunc(1, 2), 3);
*/
		}
		TEST_METHOD(FASTCALL_CC)
		{
			hooks.addFunctionHook(sumFASTCALL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(sumFASTCALL(1, 2), 1);
			Assert::AreEqual(hooks[sumFASTCALL].originalFunc(1, 2), 3);
		}
		TEST_METHOD(FASTCALL_CC_IdentSig)
		{
			hooks.addFunctionHook(sumIdenticalSigFASTCALL, [](int a, int b){return 10; }).enable();
			Assert::AreEqual(sumIdenticalSigFASTCALL(1, 2), 10);
			Assert::AreEqual(hooks[sumIdenticalSigFASTCALL].originalFunc(10, 20), 30);
		}
		TEST_METHOD(HGetKeyState)
		{
			hooks.addFunctionHook(GetKeyState, [](int key){return (SHORT)0x8000; }).enable();
			Assert::AreEqual((SHORT)0x8000, GetKeyState(VK_LSHIFT));
		}
	};

	TEST_CLASS(MemberFunctions)
	{
		S s;
		TEST_METHOD(DEFAULT_CC)
		{
			hooks.addMethodHook(&S::sum, [](S*pThis, int a, int b){return 10; }).enable();
			Assert::AreEqual(s.sum(1, 2), 10);
			Assert::AreEqual(hooks[&S::sum].object(&s).originalMethod(1,2), 3);
		};

		TEST_METHOD(CDECL_CC)
		{
			hooks.addMethodHook(&S::sumCDECL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumCDECL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumCDECL].object(&s).originalMethod(1, 2), 3);
		};

		TEST_METHOD(STDCALL_CC)
		{
			hooks.addMethodHook(&S::sumSTDCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumSTDCALL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumSTDCALL].object(&s).originalMethod(1, 2), 3);
		};

		TEST_METHOD(FASTCALL_CC)
		{
			hooks.addMethodHook(&S::sumFASTCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumFASTCALL(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumFASTCALL].object(&s).originalMethod(1, 2), 3);
		};

		TEST_METHOD(STATIC_METHOD)
		{
			hooks.addFunctionHook(&S::sumStatic, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(s.sumStatic(1, 2), 1);
			Assert::AreEqual(hooks[&S::sumStatic].originalFunc(1, 2), 3);
		};

		TEST_METHOD(VMTHook)
		{
			Base &b = Derived();
			hooks.addVMTHook(&Base::m, &b, [](Base *pThis, int a, int b){return 1; }).enable();
			auto e = b.m(100, 100);
			Assert::AreEqual(1, e);
		}

		TEST_METHOD(COPY_ORIG_PTR)
		{
			S s1;
			S s2;
			hooks.addMethodHook(&S::sum, [](S *pThis, int a, int b){pThis->val = 100; return 1; }).enable();
			auto &o1 = hooks[&S::sum].object(&s1);
			hooks[&S::sum].object(&s2).originalMethod(5, 5);
			o1.originalMethod(10, 10);
			Assert::AreEqual(20, s1.val);
			Assert::AreEqual(10, s2.val);
		};
	};
}