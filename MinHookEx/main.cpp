#include "MinHookEx.h"
#include <iostream> 
#include <windows.h>
#include <conio.h>

using namespace std;

__declspec(noinline)
int __stdcall sumSTDCALL(int a, int b)
{
	return a + b;
}


struct STest
{
	__declspec(noinline)
	void m(int a)
	{
		cout << "Original: STest::m a = " << a << endl;
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


struct Derived : Base
{
	__declspec(noinline)
		virtual int m(int a, int b) override
	{
		return a + b;
	}
};

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
		int __thiscall sumTHISCALL(int a, int b)
	{
		return a + b;
	}
	__declspec(noinline)
		static int sumStatic(int a, int b)
	{
		return a + b;
	}
};

int main()
{
	auto &hooks = CMinHookEx::getInstance();
	int capchka = 100500;
	hooks.addHook(sumSTDCALL, [=](int a, int b) {cout << "Hi! a = " << capchka << endl; return 1; }).enable();
	
	auto pfn =(void*) 0xDEAD;
	using TFN = int (*)(int a, double b);
	auto pTarget = (TFN)pfn;
/*
	hooks.addFunctionHook(pTarget, [=](int a, double b) {return pTarget(1, 2.0); }).enable();

	
	cout << sumSTDCALL(1, 2) << endl;
	
*/

//	cout << hooks[sumSTDCALL]->originalFunc(1, 2) << endl;

/*
	hooks.addHook(&S::sumTHISCALL, [&](S*pThis, int a, int b)
	{cout << "a = " << a << "; b = " << b << endl; a = 50; b = 60; return a+b; }).enable();
	S s;
//	DebugBreak();
	cout << s.sumTHISCALL(10, 20) << endl;
*/
/*	hooks.addMethodHook(&STest::m, [](STest* pVoid, int a){cout << "Hi! STest::m a = " << a << endl; }).enable();
	STest t, t1, t2;
	t.m(1);
	auto &o = hooks[&STest::m]->object(&t),
		&o1 = hooks[&STest::m]->object(&t1),
		&o2 = hooks[&STest::m]->object(&t2);
	o.originalMethod(1);
	o1.originalMethod(2);
	o2.originalMethod(3);

	Base &b = Derived();

	hooks.addVMTHook(&Base::m, &b, [](Base*pThis, int a, int b){return 1; }).enable();

	cout << b.m(1, 2) << endl;

	hooks.addFunctionHook(sumSTDCALL, [](int a, int b)
	{
		cout << "Hi! b = " << b << endl;
		return 100;
	}).enable();

	cout << sumSTDCALL(10, 20) << endl;
	cout << hooks[sumSTDCALL]->originalFunc(300, 400) << endl;
*/
	

	return 0;
}