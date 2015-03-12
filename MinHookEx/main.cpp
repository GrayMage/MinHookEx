#include "MinHookEx.h"
#include <iostream> 
#include <windows.h>
#include <stdarg.h>
#include <functional>


using namespace std;

struct test
{
	int c = 7;
	int testFunc(int a) 
	{
		cout << "I'm testFunc from testinner c = " << c << endl; return 1;
	}
};

struct test2
{
	int c = 7;
	int _cdecl testFunc(int a)
	{
		cout << "I'm testFunc from test2 cdecl inner c = " <<c << endl; return 1;
	}
};


int myTestFunc1(test *pVoid, int a)
{
	cout << "I'm myTestFunc1 cdecl" << endl; return 0;
}

int __fastcall myTestFunc2(test *pVoid, int a)
{
	cout << "I'm myTestFunc2 fastcall" << endl; return 0;
}

int __stdcall myTestFunc3(test *pVoid, float a)
{
	cout << "I'm myTestFunc3 overload stdcall" << endl; return 0;
}

int __cdecl myTestFunc4(test *pVoid, int a)
{
	cout << "I'm myTestFunc4 explicit cdecl" << endl; return 0;
}

int main()
{
	CMinHookEx &mh = CMinHookEx::getInstance();

	MessageBeep(100);
	Sleep(1000);
	mh.functionHook(MessageBeep, [](UINT) {cout << "Hi from Beep" << endl; return TRUE; }).enable();
	MessageBeep(100);
	Sleep(1000);
	mh[MessageBeep]->originalFunc(100);
	Sleep(2000);

	mh.functionHook(myTestFunc1, [](test *p, int b){cout << "hook for testfunc1: b=" << b << endl; return 0; }).enable();
	mh.functionHook(myTestFunc2, [](test *p, int b){cout << "hook for testfunc2: b=" << b << endl; return 0; }).enable();
	mh.functionHook(myTestFunc3, [](test *p, float b){cout << "hook for testfunc3: b=" << b << endl; return 0; }).enable();
	mh.functionHook(myTestFunc4, [](test *p, int b){cout << "hook for testfunc4: b=" << b << endl; return 0; }).enable();

	myTestFunc1(0, 11);
	myTestFunc2(0, 12);
	myTestFunc3(0, 13);
	myTestFunc4(0, 14);

	mh[myTestFunc1]->originalFunc(0, 2);
	mh[myTestFunc4]->originalFunc(0, 3);

	mh.methodHook(&test::testFunc, [](test *pThis, int a) 
	{
		cout << "hook for test::testFunc, a = " << a << endl;
		pThis->c = 100500;
		CMinHookEx::getInstance()[&test::testFunc]->object(pThis).originalMethod(22); 
		return 0; 
	}).enable();
	mh.methodHook(&test2::testFunc, [](test2 *pThis, int a) {cout << "hook for test::testFunc, a = " << a << endl; pThis->c = 100500; return 0; }).enable();

	test t1;
	test2 t2;

	t1.testFunc(15);
	t2.testFunc(17);

/*
	mh[&test::testFunc]->object(&t1).originalMethod(22);
	mh[&test2::testFunc]->object(&t2).originalMethod(45);
*/
	Sleep(2000);
	return 0;
}