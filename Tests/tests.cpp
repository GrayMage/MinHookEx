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

__declspec(noinline)
int __fastcall intIntIntDetourFASTCALL(int a, int b)
{
	return 5;
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

struct Derived: public Base
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
		TEST_METHOD(FASTCALL_FUNC_DETOUR)
		{
			hooks.addHook(sumCDECL, intIntIntDetourFASTCALL).enable();
			Assert::AreEqual(5, sumCDECL(5, 5));
			Assert::AreEqual(10, hooks[sumCDECL].originalFunc(5, 5));
		}

		TEST_METHOD(CDECL_ImplCC)
		{
			hooks.addHook(sumImplCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(1, sumImplCDECL(1, 2));
			Assert::AreEqual(3, hooks[sumImplCDECL].originalFunc(1,2));
		}
		TEST_METHOD(CDECL_CC)
		{
			hooks.addHook(sumCDECL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(1, sumCDECL(1, 2));
			Assert::AreEqual(3, hooks[sumCDECL].originalFunc(1, 2));
		}
		TEST_METHOD(STDCALL_CC)
		{
			hooks.addHook(sumSTDCALL, [](int a, int b) {return 1; }).enable();
			Assert::AreEqual(1, sumSTDCALL(1, 2));
			Assert::AreEqual(3, hooks[sumSTDCALL].originalFunc(1, 2));
		}
		TEST_METHOD(FASTCALL_CC)
		{
			hooks.addHook(sumFASTCALL, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(1, sumFASTCALL(1, 2));
			Assert::AreEqual(3, hooks[sumFASTCALL].originalFunc(1, 2));
		}
		TEST_METHOD(FASTCALL_CC_IdentSig)
		{
			hooks.addHook(sumFASTCALL, [](int a, int b) {return 25; }).enable();
			hooks.addHook(sumIdenticalSigFASTCALL, [](int a, int b){return 10; }).enable();
			Assert::AreEqual(25, sumFASTCALL(1, 2));
			Assert::AreEqual(3, hooks[sumFASTCALL].originalFunc(1, 2));
			Assert::AreEqual(10, sumIdenticalSigFASTCALL(1, 2));
			Assert::AreEqual(30, hooks[sumIdenticalSigFASTCALL].originalFunc(10, 20));
		}
		TEST_METHOD(HGetKeyState)
		{
			hooks.addHook(GetKeyState, [](int key){return (SHORT)0x8000; }).enable();
			Assert::AreEqual((SHORT)0x8000, GetKeyState(VK_LSHIFT));
		}
	};

	TEST_CLASS(MemberFunctions)
	{
		S s;
		TEST_METHOD(DEFAULT_CC)
		{
			hooks.addHook(&S::sum, [](S*pThis, int a, int b){return 10; }).enable();
			Assert::AreEqual(10, s.sum(1, 2));
			Assert::AreEqual(3, hooks[&S::sum].object(&s).originalMethod(1,2));
		};

		TEST_METHOD(CDECL_CC)
		{
			hooks.addHook(&S::sumCDECL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(1, s.sumCDECL(1, 2));
			Assert::AreEqual(3, hooks[&S::sumCDECL].object(&s).originalMethod(1, 2));
		};

		TEST_METHOD(STDCALL_CC)
		{
			hooks.addHook(&S::sumSTDCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(1, s.sumSTDCALL(1, 2));
			Assert::AreEqual(3, hooks[&S::sumSTDCALL].object(&s).originalMethod(1, 2));
		};

		TEST_METHOD(FASTCALL_CC)
		{
			hooks.addHook(&S::sumFASTCALL, [](S*pThis, int a, int b){return 1; }).enable();
			Assert::AreEqual(1, s.sumFASTCALL(1, 2));
			Assert::AreEqual(3, hooks[&S::sumFASTCALL].object(&s).originalMethod(1, 2));
		};

		TEST_METHOD(STATIC_METHOD)
		{
			hooks.addHook(&S::sumStatic, [](int a, int b){return 1; }).enable();
			Assert::AreEqual(1, s.sumStatic(1, 2));
			Assert::AreEqual(3, hooks[&S::sumStatic].originalFunc(1, 2));
		};

		TEST_METHOD(VMTHook)
		{
			Base &b = Derived();
			hooks.addHook(&Base::m, &b, [](Base *pThis, int a, int b){return 1; }).enable();
			auto e = b.m(100, 100);
			Assert::AreEqual(1, e);
		}

		TEST_METHOD(CAPTURING_LAMBDA_DETOUR)
		{
			int test = 10;
			hooks.addHook(&S::sum, [=](S*, int a, int b) {return test; }).enable();
			Assert::AreEqual(10, s.sum(1, 1));
		}

		TEST_METHOD(COPY_ORIG_PTR)
		{
			S s1;
			S s2;
			hooks.addHook(&S::sum, [](S *pThis, int a, int b){pThis->val = 100; return 1; }).enable();
			auto &o1 = hooks[&S::sum].object(&s1);
			hooks[&S::sum].object(&s2).originalMethod(5, 5);
			o1.originalMethod(10, 10);
			Assert::AreEqual(20, s1.val);
			Assert::AreEqual(10, s2.val);
		};
	};
}