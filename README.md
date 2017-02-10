# MinHookEx
MinHookEx - C++ wrapper around TsudaKageyu's Minimalistic x86/x64 API Hooking Library for Windows

MinHookEx provides C++ interface for TsudaKageyu's MinHook library.

Currently only Microsoft Visual Studio 2015 is supported.

#Usage

1. Include MinHookEx/MinHookEx.h in your source file
2. Get reference to CMinHookEx instance: auto &hooks = CMinHookEx::getInstance()
3. Add necessary hooks: 	
  hooks.addHook(MessageBoxA, [](HWND hwnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
	{
		return hooks[MessageBoxA].originalFunc(hwnd, "Hello!!!", lpCaption, uType);
	}).enable();

That's it! While MessageBoxA hook enabled, every call to this funcion will be redirected to your lambda.

Capturing lambdas are also supported.

MinHookEx allows you to intercept functions, methods and virtual methods. You can use function or method pointer as target.

If you're hooking a function or a static method, detour's signature must be identical to target's one.
If you're hooking a regular or virtual method, pointer to object must be the first parameter of detour function (this pointer).

See included tests for examples.
