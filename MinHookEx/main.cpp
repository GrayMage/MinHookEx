#include "MinHookEx.h"
#include <windows.h>

auto &hooks = CMinHookEx::getInstance();

int main()
{
	//---------------------------EXAMPLE----------------------

	hooks.addHook(MessageBoxA, [](HWND hwnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) //Also works with method pointers, function pointers; capturing lambda can be used as detour
	{
		return hooks[MessageBoxA].originalFunc(hwnd, "Hello!!!", lpCaption, uType);
	}).enable();

	MessageBoxA(nullptr, "SomeText", "Test", MB_OK);

	hooks[MessageBoxA].disable(); //Disable hook

	MessageBoxA(nullptr, "SomeText", "Test", MB_OK);

	hooks.removeAll(); // Removes all hooks and clears hook list

	//Please take a look at Tests for other examples

	return 0;
}