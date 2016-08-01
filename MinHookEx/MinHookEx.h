#pragma once
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _M_X64
#include "minhook/src/HDE/hde64.c"
#else
#include "minhook/src/HDE/hde32.c"
#endif

#include "minhook/include/MinHook.h"
#include "minhook/src/buffer.c"
#include "minhook/src/trampoline.c"
#include "minhook/src/hook.c"

using namespace std;

class CMinHookEx
{
private:
	//------------------------HELPERS-----------------------
#include "Inner/VTableIndex.h"
	template <typename TFunc, typename TRet, typename ...TArgs>
	class CFunctionHookSpec;

	template<typename U, typename = void> struct S;
#define SGenFunc(CC) template<typename TRet, typename ...TArgs>\
		struct S<TRet CC (TArgs...)>\
		{\
			using type = TRet (*) (TArgs...);\
			template<typename TFunc> using THookSpec = CFunctionHookSpec<TFunc, TRet, TArgs...>;\
		};
#ifndef _M_X64
	SGenFunc(__cdecl);
	SGenFunc(__stdcall);
#endif
	SGenFunc(__fastcall);

	template<typename TF, typename TO>
	struct S<TF TO::*>
	{
		using TFunc = TF;
		using TObj = TO;
	};

#define SGenFallbackForDefaultCC(CC) template<typename TRet, typename TO, typename ...TArgs>\
	struct S<TRet (CC TO::*) (TArgs...), typename enable_if<is_same<TRet (CC)(TArgs...), TRet(TArgs...)>::value>::type>\
	{\
		using TFunc = TRet (TArgs...);\
		using TObj = TO;\
	};

#ifndef _M_X64
	SGenFallbackForDefaultCC(__cdecl);
	SGenFallbackForDefaultCC(__stdcall);
	SGenFallbackForDefaultCC(__fastcall);
#endif
#define emptyCC
	template<typename T> struct SMethodDetourType;
#define SMethodDetourTypeGen(CC) template<typename TRet, typename TObj, typename ...TArgs>\
	struct SMethodDetourType <TRet (CC TObj::*)(TArgs...)>\
	{\
		using TDetour = function<TRet(TObj *pThis, TArgs...)>;\
	};
#ifndef _M_X64
	SMethodDetourTypeGen(__stdcall);
	SMethodDetourTypeGen(__cdecl);
	SMethodDetourTypeGen(__fastcall);
#endif
	SMethodDetourTypeGen(emptyCC);

	template <typename TMethod, typename TRet, typename TObject, typename ...TArgs>
	class CMethodHookSpec;

	template<typename T> struct SHookSpecH;
#define SHookSpecMethodGen(CC) template<typename TRet, typename TObj, typename ...TArgs>\
	struct SHookSpecH<TRet (CC TObj::*)(TArgs...)>\
	{\
		using type = CMethodHookSpec<TRet (CC TObj::*)(TArgs...), TRet, TObj, TArgs...>;\
	};
#ifndef _M_X64
	SHookSpecMethodGen(__stdcall);
	SHookSpecMethodGen(__cdecl);
	SHookSpecMethodGen(__fastcall);
#endif
	SHookSpecMethodGen(emptyCC);

	template<typename TMethod, typename = void> struct HelpTypes;
	template<typename TMethod>
	struct HelpTypes<TMethod, typename enable_if<is_member_function_pointer<TMethod>::value>::type>
	{
		using TOrigFunc = typename S<TMethod>::TFunc;
		using TObject = typename S<TMethod>::TObj;
		using TDetour = typename SMethodDetourType<TMethod>::TDetour;
		using THookSpec = typename SHookSpecH<TMethod>::type;
	};

template<typename TFunc>
	struct HelpTypes<TFunc, typename enable_if<!is_member_function_pointer<TFunc>::value>::type>
	{
		using TDetour = function<TFunc>;
		using THookSpec = typename S<TFunc>::template THookSpec<TFunc>;
	};

	template<typename T, typename = void> struct SCCStripper;
	template<typename T>
	struct SCCStripper<T, typename enable_if<!is_object<T>::value>::type>
	{
		using type = typename S<T>::type;
	};

	template<typename T> 
	struct SCCStripper<T, typename enable_if<is_object<T>::value>::type>
	{
	private:
		template<typename U> struct S;
#define SGen(CC) template<typename TRet, typename TObj, typename ...TArgs>\
		struct S<TRet (CC TObj::*)(TArgs...) const>\
		{\
			using type = TRet (*) (TArgs...);\
		};
#ifndef _M_X64
		SGen(__stdcall);
		SGen(__cdecl);
		SGen(__fastcall);
#endif
		SGen(emptyCC);
	public:
		using type = typename S<decltype(&T::operator())>::type;
	};

	//---------------------------------------------------------------------------

	class CHook
	{
		friend class CMinHookEx;
	protected:
		LPVOID _pfnTarget;
		bool _bHookValid = true;

		struct SCounter
		{
		private:
			atomic_uint &_i;
			SCounter(const SCounter &) = delete;
			SCounter& operator=(const SCounter &) = delete;
		public:
			explicit SCounter(atomic_uint &i) : _i(i)
			{
				++_i;
			}

			~SCounter()
			{
				--_i;
			}
		};

		//---------------TLS Data Keepers-------------
		static void** getHookTLSPtr()
		{
			__declspec(thread) static void *pHook;
			return &pHook;
		}

		static void** getObjectTLSPtr()
		{
			__declspec(thread) static void *pObject;
			return &pObject;
		}

		//--------------------------------------------

		class CMemoryFunction
		{
		private:
			bool _bValid;
			bool _needsCleanup;
			void *_addr;
			size_t _size;
			DWORD _oldProtect;
		public:
			bool isValid()
			{
				return _bValid;
			}

			void* address()
			{
				return _addr;
			}

			void flush()
			{
				FlushInstructionCache(GetCurrentProcess(), _addr, _size);
			}

			explicit CMemoryFunction(size_t size, void *location = nullptr) : _bValid(false), _size(size)
			{
				_needsCleanup = location == nullptr;
				_addr = _needsCleanup ? malloc(size) : location;
				_bValid = _addr != nullptr;
				if (_bValid)
					VirtualProtect(_addr, _size, PAGE_EXECUTE_READWRITE, &_oldProtect);
			}

			explicit CMemoryFunction(initializer_list<unsigned __int8> &&byteCode, void *location = nullptr) :
				CMemoryFunction(byteCode.size(), location)
			{
				if (_bValid)
				{
					auto p = (unsigned __int8*)_addr;
					int i = 0;
					for (auto &e : byteCode)
					{
						p[i++] = e;
					}
				}
				flush();
			}

			virtual ~CMemoryFunction()
			{
				if (_needsCleanup && _bValid)
				{
					VirtualProtect(_addr, _size, _oldProtect, &_oldProtect);
					free(_addr);
					flush();
				}
			}
		};

		class CDetourBridge : public CMemoryFunction
		{
#pragma pack(push)
#pragma pack(1)
#ifdef _M_X64
			struct SDetourBridge
			{
			/*
				0:  50                      push   rax
				1:  53                      push   rbx
				2:  51                      push   rcx
				3:  52                      push   rdx
				4:  48 b8 11 11 11 11 11    movabs rax,0x1111111111111111
				b:  11 11 11
				e:  ff d0                   call   rax
				10: 48 bb 33 33 33 33 33    movabs rbx,0x3333333333333333
				17: 33 33 33
				1a: 48 89 18                mov    QWORD PTR [rax],rbx
				1d: 5a                      pop    rdx
				1e: 59                      pop    rcx
				1f: 5b                      pop    rbx
				20: 48 83 ec 08             sub    rsp,0x8
				24: 48 b8 44 44 44 44 44    movabs rax,0x4444444444444444
				2b: 44 44 44
				2e: 48 89 04 24             mov    QWORD PTR [rsp],rax
				32: 48 8b 44 24 08          mov    rax,QWORD PTR [rsp+0x8]
				37: c2 08 00                ret    0x8
				*/
				uint32_t pushRaxRbxRcxRdx = 0x52515350;
				uint16_t movabsRax = 0xb848;
				void *getTLSpHook = &getHookTLSPtr;
				uint16_t callRax = 0xd0ff;
				uint16_t movabsRbx = 0xbb48;
				void *pHook;
				uint16_t movRaxRbx0 = 0x8948;
				uint8_t movRaxRbx1 = 0x18;
				uint16_t popRdxRcx = 0x595a;
				uint8_t popRbx = 0x5b;
				uint32_t subRsp8 = 0x08ec8348;
				uint16_t movRax = 0xb848;
				void *detourAddr;
				uint32_t movRspRax = 0x24048948;
				uint64_t restoreRaxRet08 = 0x08c20824448b48;
			};
#else
			struct SDetourBridge
			{
				/*
				0:  60                      pusha
				1:  9c						pushf
				2:  b8 11 11 11 11          mov    eax,0x11111111
				7:  ff d0                   call   eax
				9:  8d 00                   lea    eax,[eax]
				b:  c7 00 33 33 33 33       mov    DWORD PTR [eax],0x33333333
				11: 9d						popf
				12: 61                      popa
				13: 68 44 44 44 44          push   0x44444444
				18: c3                      ret
				*/
				uint16_t pushadPushf = 0x9c60;
				uint8_t movEax = 0xb8;
				void *getTLSpHook = &getHookTLSPtr;
				uint16_t callEax = 0xd0ff;
				uint16_t movEax1 = 0xc7;
				void *pHook;
				uint16_t popadPopf = 0x619d;
				uint8_t push = 0x68;
				void *detourAddr;
				uint8_t ret = 0xc3;
			};

#endif
#pragma pack(pop)
		public:

			CDetourBridge(void *detourAddr, void *pHook) : CMemoryFunction(sizeof(SDetourBridge))
			{
				auto p = (SDetourBridge*)address();
				new(p) SDetourBridge();
				p->detourAddr = detourAddr;
				p->pHook = pHook;
				flush();
			}
		};

		class COriginalBridge : public CMemoryFunction
		{
		private:
#pragma pack(push)
#pragma pack(1)
#ifdef _M_X64
			struct SOriginalBridge
			{
			/*
				0:  50                      push   rax
				1:  53                      push   rbx
				2:  51                      push   rcx
				3:  52                      push   rdx
				4:  48 b8 11 11 11 11 11    movabs rax,0x1111111111111111
				b:  11 11 11
				e:  ff d0                   call   rax
				10: 48 bb 22 22 22 22 22    movabs rbx,0x2222222222222222
				17: 22 22 22
				1a: 48 89 18                mov    QWORD PTR [rax],rbx
				1d: 48 b8 33 33 33 33 33    movabs rax,0x3333333333333333
				24: 33 33 33
				27: ff d0                   call   rax
				29: 48 bb 44 44 44 44 44    movabs rbx,0x4444444444444444
				30: 44 44 44
				33: 48 89 18                mov    QWORD PTR [rax],rbx
				36: 5a                      pop    rdx
				37: 59                      pop    rcx
				38: 5b                      pop    rbx
				39: 48 83 ec 08             sub    rsp,0x8
				3d: 48 b8 55 55 55 55 55    movabs rax,0x5555555555555555
				44: 55 55 55
				47: 48 89 04 24             mov    QWORD PTR [rsp],rax
				4b: 48 8b 44 24 08          mov    rax,QWORD PTR [rsp+0x8]
				50: c2 08 00                ret    0x8
				*/
				uint32_t pushRaxRbxRcxRdx = 0x52515350;
				uint16_t movabsRax = 0xb848;
				void *getTLSpHook = &getHookTLSPtr;
				uint16_t callRax = 0xd0ff;
				uint16_t movabsRbx = 0xbb48;
				void *pHook;
				uint16_t movRaxRbx0 = 0x8948;
				uint8_t movRaxRbx1 = 0x18;
				uint16_t movabsRax1 = 0xb848;
				void *getTLSpObject = &getObjectTLSPtr;
				uint16_t callRax1 = 0xd0ff;
				uint16_t movabsRbx1 = 0xbb48;
				void *pObject;
				uint16_t movRaxRbx01 = 0x8948;
				uint8_t movRaxRbx11 = 0x18;
				uint16_t popRdxRcx = 0x595a;
				uint8_t popRbx = 0x5b;
				uint32_t subRsp8 = 0x08ec8348;
				uint16_t movRax = 0xb848;
				void *originalInvokerAddr;
				uint32_t movRspRax = 0x24048948;
				uint8_t ret081 = 0xc2;
				uint16_t ret082 = 0x08;
			};
#else
			struct SOriginalBridge
			{
				/*
				0:  60                      pusha
				1:  9c						pushf
				2:  b8 11 11 11 11          mov    eax,0x11111111
				7:  ff d0                   call   eax
				9:  c7 00 22 22 22 22       mov    DWORD PTR [eax],0x22222222
				f:  b8 33 33 33 33          mov    eax,0x33333333
				14: ff d0                   call   eax
				16: c7 00 44 44 44 44       mov    DWORD PTR [eax],0x44444444
				1c: 9d						popf
				1d: 61                      popa
				1e: 68 55 55 55 00          push   0x555555
				23: c3                      ret
				*/
				uint16_t pushadPushf = 0x9c60;
				uint8_t MovEax = 0xb8;
				void *getTLSpHook = &getHookTLSPtr;
				uint16_t callEax = 0xd0ff;
				uint16_t movDerefEax = 0xc7;
				void *pHook;
				uint8_t movEax = 0xb8;
				void *getTLSpObject = &getObjectTLSPtr;
				uint16_t callEax1 = 0xd0ff;
				uint16_t movDerefEax1 = 0xc7;
				void *pObject;
				uint16_t popaPopf = 0x619d;
				uint8_t push = 0x68;
				void *originalInvokerAddr;
				uint8_t ret = 0xc3;
			};
#endif
#pragma pack(pop)
		public:
			COriginalBridge() :CMemoryFunction(sizeof(SOriginalBridge))
			{
				new(address()) SOriginalBridge;
			}

			void setData(void *thisPtr, void *targetObjectPtr, void *originalInvokerAddr)
			{
				auto p = (SOriginalBridge*)address();
				p->originalInvokerAddr = originalInvokerAddr;
				p->pHook = thisPtr;
				p->pObject = targetObjectPtr;
				flush();
			}
		};

		class COriginalBridgePool
		{
		private:
			mutex m;
			list<COriginalBridge *> _availableBridges;
		public:
			COriginalBridge* get()
			{
				lock_guard<mutex> lock(m);
				if (_availableBridges.empty()) return new COriginalBridge();
				auto r = _availableBridges.front();
				_availableBridges.pop_front();
				return r;
			}

			void release(COriginalBridge *bridge)
			{
				lock_guard<mutex> lock(m);
				_availableBridges.push_back(bridge);
			}

			~COriginalBridgePool()
			{
				for (auto p : _availableBridges)
					delete p;
			}
		};

		explicit CHook(LPVOID pfnTarget) : _pfnTarget(pfnTarget)
		{
			threadCounter = 0;
		}

		CHook() : CHook(nullptr) {};

		CHook(const CHook &) = delete;
		CHook& operator=(const CHook &) = delete;
		atomic_uint threadCounter;

		virtual void deleteLater()
		{
			MH_DisableHook(_pfnTarget);
			while (threadCounter > 0)
			{
				this_thread::sleep_for(chrono::milliseconds(10));
			}
			MH_RemoveHook(_pfnTarget);
			delete this;
		}

		virtual ~CHook() {};

	public:

		bool enable() const
		{
			return _bHookValid && (MH_EnableHook(_pfnTarget) == MH_OK);
		}

		bool disable() const
		{
			return _bHookValid && (MH_DisableHook(_pfnTarget) == MH_OK);
		}
	};

	template <typename TFunc>
	class CFunctionHook : public CHook
	{
		friend class CMinHookEx;

	private:
		CFunctionHook(const CFunctionHook &) = delete;
		CFunctionHook& operator=(const CFunctionHook &) = delete;
	protected:
		explicit CFunctionHook(LPVOID target) : CHook(target) {}

	public:
		TFunc * const originalFunc = 0;
	};

	template <typename TFunc, typename TRet, typename ...TArgs>
	class CFunctionHookSpec : CFunctionHook<TFunc>
	{
		friend CMinHookEx;
	private:
		function<TFunc> _detour;
		TFunc *_originalFunc;

		static TRet __stdcall invokeOriginalSTDCALL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			return pThis->_originalFunc(Args...);
		}

		static TRet __cdecl invokeOriginalCDECL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			return pThis->_originalFunc(Args...);
		}

		static TRet __fastcall invokeOriginalFASTCALL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			return pThis->_originalFunc(Args...);
		}

		static TRet __cdecl invokeDetourCDECL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour(Args...);
		}

		static TRet __fastcall invokeDetourFASTCALL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour(Args...);
		}

		static TRet __stdcall invokeDetourSTDCALL(TArgs ...Args)
		{
			auto pThis = (CFunctionHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour(Args...);
		}

		LPVOID detourProxyAddr()
		{
			auto detourInvokerCDECL = &CFunctionHookSpec::invokeDetourCDECL;
			auto detourInvokerFASTCALL = &CFunctionHookSpec::invokeDetourFASTCALL;
			auto detourInvokerSTDCALL = &CFunctionHookSpec::invokeDetourSTDCALL;

			return is_same<TFunc, TRet __stdcall(TArgs ...)>::value ?
				       (LPVOID*&)detourInvokerSTDCALL :
				       is_same<TFunc, TRet __fastcall(TArgs ...)>::value ?
				       (LPVOID*&)detourInvokerFASTCALL : (LPVOID*&)detourInvokerCDECL;
		}

		TFunc* originalFuncInvokerAddr()
		{
			auto originalInvokerSTDCALL = &CFunctionHookSpec::invokeOriginalSTDCALL;
			auto originalInvokerCDECL = &CFunctionHookSpec::invokeOriginalCDECL;
			auto originalInvokerFASTCALL = &CFunctionHookSpec::invokeOriginalFASTCALL;

			return is_same<TFunc, TRet __fastcall(TArgs ...)>::value ?
				       (TFunc*&)originalInvokerFASTCALL :
				       is_same<TFunc, TRet __cdecl(TArgs ...)>::value ?
				       (TFunc*&)originalInvokerCDECL : (TFunc*&)originalInvokerSTDCALL;
		}

		CDetourBridge _detourBridge;
		COriginalBridge _originalBridge;

		CFunctionHookSpec(TFunc *target, function<TRet(TArgs...)> detour) : CFunctionHook(target), _detour(detour), _detourBridge(detourProxyAddr(), this)
		{
			LPVOID _pfnOriginal = nullptr;

			_bHookValid = _bHookValid && _detourBridge.isValid() && _originalBridge.isValid() &&
				(MH_CreateHook(target, _detourBridge.address(), &_pfnOriginal) == MH_OK);

			_originalFunc = _bHookValid ? (TFunc*)_pfnOriginal : (TFunc*)target; //Used to call unhooked method via invoker

			_originalBridge.setData(this, nullptr, originalFuncInvokerAddr());
			const_cast<TFunc*>(originalFunc) = (TFunc*)_originalBridge.address();
		}
	};

	//-------------------------------------------------------------------------
	template <typename TMethod>
	class CMethodHook : public CHook
	{
	private:
		CMethodHook(const CMethodHook &) = delete;
		CMethodHook& operator=(const CMethodHook &) = delete;

		COriginalBridgePool _originalBridgePool;

		class CProxyObject
		{
			friend CMethodHook;
		private:
			using TOriginalMethod = typename HelpTypes<TMethod>::TOrigFunc*;

			CProxyObject(const CProxyObject &) = default;
			CProxyObject& operator=(const CProxyObject &) = delete;

			COriginalBridgePool &_origBridgePool;
			COriginalBridge *_originalBridge;

			CProxyObject(COriginalBridgePool &originalBridgePool, void *thisPtr,
			             void *objectThisPtr, void *originalInvokerAddr) : _origBridgePool(originalBridgePool)
			{
				_originalBridge = _origBridgePool.get();
				_originalBridge->setData(thisPtr, objectThisPtr, originalInvokerAddr);
				const_cast<TOriginalMethod>(originalMethod) = (TOriginalMethod)_originalBridge->address();
			}

		public:
			const TOriginalMethod originalMethod = nullptr;

			~CProxyObject()
			{
				_origBridgePool.release(_originalBridge);
			}
		};

	protected:
		explicit CMethodHook(LPVOID target) : CHook(target) {}

		typename HelpTypes<TMethod>::TOrigFunc *_originalInvoker;

		virtual ~CMethodHook() {}

		using TObject = typename HelpTypes<TMethod>::TObject;

	public:
		CProxyObject object(TObject *thisPtr)
		{
			return CProxyObject(_originalBridgePool, this, thisPtr, _originalInvoker);
		}
	};

	template <typename TMethod, typename TRet, typename TObject, typename ...TArgs>
	class CMethodHookSpec : CMethodHook<TMethod>
	{
		friend class CMinHookEx;

	private:
		typename HelpTypes<TMethod>::TDetour _detour;

		static TRet __stdcall invokeOriginalSTDCALL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			auto pObjectThisPtr = (TObject*)*getObjectTLSPtr();
			return (pObjectThisPtr ->* (pThis->_originalMethod))(Args...);
		}

		static TRet __cdecl invokeOriginalCDECL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			auto pObjectThisPtr = (TObject*)*getObjectTLSPtr();
			return (pObjectThisPtr ->* (pThis->_originalMethod))(Args...);
		}

		static TRet __fastcall invokeOriginalFASTCALL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			auto pObjectThisPtr = (TObject*)*getObjectTLSPtr();
			return (pObjectThisPtr ->* (pThis->_originalMethod))(Args...);
		}

		TRet __thiscall invokeDetour(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour((TObject*)this, Args...);
		}

		TRet __cdecl invokeDetourCDECL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour((TObject*)this, Args...);
		}

		TRet __fastcall invokeDetourFASTCALL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour((TObject*)this, Args...);
		}

		TRet __stdcall invokeDetourSTDCALL(TArgs ...Args)
		{
			auto pThis = (CMethodHookSpec*)*getHookTLSPtr();
			SCounter c(pThis->threadCounter);
			return pThis->_detour((TObject*)this, Args...);
		}

		LPVOID detourProxyAddr()
		{
			auto detourInvoker = &CMethodHookSpec::invokeDetour;
			auto detourInvokerCDECL = &CMethodHookSpec::invokeDetourCDECL;
			auto detourInvokerFASTCALL = &CMethodHookSpec::invokeDetourFASTCALL;
			auto detourInvokerSTDCALL = &CMethodHookSpec::invokeDetourSTDCALL;

			return is_same<TMethod, TRet(__stdcall TObject::*)(TArgs ...)>::value ?
				       (LPVOID*&)detourInvokerSTDCALL :
				       is_same<TMethod, TRet(__fastcall TObject::*)(TArgs ...)>::value ?
				       (LPVOID*&)detourInvokerFASTCALL :
				       is_same<TMethod, TRet(__cdecl TObject::*)(TArgs ...)>::value ?
				       (LPVOID*&)detourInvokerCDECL : (LPVOID*&)detourInvoker;
		}

		TMethod _originalMethod;
		using TOrigFunc = typename HelpTypes<TMethod>::TOrigFunc;

		TOrigFunc* originalMethodInvokerAddr()
		{
			auto originalInvokerSTDCALL = &CMethodHookSpec::invokeOriginalSTDCALL;
			auto originalInvokerCDECL = &CMethodHookSpec::invokeOriginalCDECL;
			auto originalInvokerFASTCALL = &CMethodHookSpec::invokeOriginalFASTCALL;

			return is_same<TOrigFunc, TRet __fastcall(TArgs ...)>::value ?
				       (TOrigFunc*&)originalInvokerFASTCALL :
				       is_same<TOrigFunc, TRet __cdecl(TArgs ...)>::value ?
				       (TOrigFunc*&)originalInvokerCDECL : (TOrigFunc*&)originalInvokerSTDCALL;
		}

		CDetourBridge _detourBridge;

		CMethodHookSpec(LPVOID target, typename HelpTypes<TMethod>::TDetour detour) : CMethodHook(target),
		                                                                              _detour(detour), _detourBridge(detourProxyAddr(), this)
		{
			LPVOID _pfnOriginal = nullptr;

			_bHookValid = _bHookValid && _detourBridge.isValid() && (MH_CreateHook(target, _detourBridge.address(), &_pfnOriginal) == MH_OK);

			_originalMethod = _bHookValid ? *(TMethod*)&_pfnOriginal : *(TMethod*)&target; //Used to call unhooked method via invoker

			_originalInvoker = originalMethodInvokerAddr();
		};
	};

	CMinHookEx()
	{
		MH_Initialize();
	}

	CMinHookEx(const CMinHookEx &) = delete;
	CMinHookEx& operator=(const CMinHookEx &) = delete;

	unordered_map<LPVOID, CHook*> _hooks;

	bool hookExists(LPVOID target)
	{
		return _hooks.find(target) != _hooks.end();
	}

	~CMinHookEx()
	{
		removeAll();
	}

public:

	void removeAll()
	{
		for (auto &x : _hooks)
		{
			x.second->deleteLater();
		}
		_hooks.clear();
	}

	static CMinHookEx& getInstance()
	{
		static CMinHookEx instance;

		return instance;
	}

	template <typename TMethod, typename TDetour>
	CMethodHook<TMethod>& addHook(TMethod target, TDetour detour)
	{
		static_assert(is_same<typename SCCStripper<TDetour>::type, typename SCCStripper<typename HelpTypes<TMethod>::TDetour>::type>::value, "Unable to hook method: invalid detour signature! Must be TRet (TObject*, TArgs...)");

		if (hookExists((void*&)target))
			_hooks.at((void*&)target)->deleteLater();

		auto h = new typename HelpTypes<TMethod>::THookSpec((LPVOID)(void*&)target, detour);
		_hooks[(void*&)target] = h;
		return *h;
	}
	
	template <typename TVirtualMethod, typename TDetour>
	CMethodHook<TVirtualMethod>& addHook(TVirtualMethod target, typename HelpTypes<TVirtualMethod>::TObject *object, TDetour detour)
	{
		static_assert(is_same<typename SCCStripper<TDetour>::type, typename SCCStripper<typename HelpTypes<TVirtualMethod>::TDetour>::type>::value, "Unable to hook virtual method: invalid detour signature! Must be TRet (TObject*, TArgs...)");

		if (hookExists((void*&)target))
			_hooks.at((void*&)target)->deleteLater();

		int vtblOffset = VTableIndex(target);
		size_t *vtbl = (size_t*)*(size_t*)object;
		LPVOID t = (LPVOID)vtbl[vtblOffset];
		auto h = new typename HelpTypes<TVirtualMethod>::THookSpec(t, detour);
		_hooks[(void*&)target] = h;
		return *h;
	}

	template <typename TFunc, typename TDetour>
	CFunctionHook<TFunc>& addHook(TFunc *target, TDetour detour)
	{
		static_assert(is_same<typename SCCStripper<TFunc>::type, typename SCCStripper<typename remove_pointer<TDetour>::type>::type>::value, "Unable to hook function: detour signature differs from target!");
		
		if (hookExists(target))
			_hooks.at(target)->deleteLater();

		auto h = new typename HelpTypes<TFunc>::THookSpec(target, detour);
		_hooks[target] = h;
		return *h;
	}

	template<typename TFunc>
	CFunctionHook<TFunc>& operator[](TFunc *target)
	{
		return *(CFunctionHook<TFunc>*)_hooks[target];
	}

	template <typename TMethod>
	CMethodHook<TMethod>& operator[](TMethod target)
	{
		return *(CMethodHook<TMethod>*)_hooks[(void*&)target];
	}

	template<typename TFunc>
	CFunctionHook<TFunc>& at(TFunc *target)
	{
		return *(CFunctionHook<TFunc>*)_hooks[target];
	}

	template <typename TMethod>
	CMethodHook<TMethod>& at(TMethod target)
	{
		return *(CMethodHook<TMethod>*)_hooks[(void*&)target];
	}
};