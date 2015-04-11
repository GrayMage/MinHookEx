#include <unordered_map>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include "minhook/src/HDE/hde32.c"
#elif
#include "minhook/src/HDE/hde64.c"
#endif

#include "minhook/include/MinHook.h"
#include "minhook/src/buffer.c"
#include "minhook/src/trampoline.c"
#include "minhook/src/hook.c"

using namespace std;

namespace MinHookEx
{
	class CMinHookEx;
	namespace internal
	{
		template<typename X> struct SMethodHookUID;
		template<typename X> struct SFunctionHookUID;

		template<typename X> SMethodHookUID<X> htMethod(X);
		template<typename X> SFunctionHookUID<X> htFunction(X);
		using MinHookEx::CMinHookEx;

		template<typename X> struct SMethodHookUID
		{
		public:
			SMethodHookUID(const SMethodHookUID&) = default;
			SMethodHookUID() = default;
			SMethodHookUID& operator=(const SMethodHookUID&) = delete;
		};

		template<typename X> struct SFunctionHookUID
		{
		public:
			SFunctionHookUID(const SFunctionHookUID&) = default;
			SFunctionHookUID() = default;
			SFunctionHookUID& operator=(const SFunctionHookUID&) = delete;
		};

		template<typename X> SMethodHookUID<X> htMethod(X x)
		{
			return SMethodHookUID<X>();
		}

		template<typename X> SFunctionHookUID<X> htFunction(X x)
		{
			return SFunctionHookUID<X>();
		}
	};

#define MethodHookUID internal::htMethod([](){})
#define FunctionHookUID internal::htFunction([](){})

	class CMinHookEx
	{
	private:
		//------------------------HELPERS-----------------------
#include "Inner/VTableIndex.h"

		template<typename TObj, typename TRet, typename ...TArgs> struct SFuncSTD
		{
			using TDetour = TRet(__stdcall *)(TObj* pVoid, TArgs...);
		};

		template<typename TObj, typename TRet, typename ...TArgs> struct SFuncCDECL
		{
			using TDetour = TRet(__cdecl *)(TObj* pVoid, TArgs...);
		};

		template<typename TObj, typename TRet, typename ...TArgs> struct SFuncFASTCALL
		{
			using TDetour = TRet(__fastcall *)(TObj* pVoid, TArgs...);
		};

		template<typename TObj, typename TRet, typename ...TArgs>
		static SFuncCDECL<TObj, TRet, TArgs...> funcStripper(TRet(__thiscall TObj::*)(TArgs...)) //return type is correct!
		{}

		template<typename TObj, typename TRet, typename ...TArgs>
		static SFuncSTD<TObj, TRet, TArgs...> funcStripper(TRet(__stdcall TObj::*)(TArgs...))
		{}

		template<typename TObj, typename TRet, typename ...TArgs>
		static SFuncCDECL<TObj, TRet, TArgs...> funcStripper(TRet(__cdecl TObj::*)(TArgs...))
		{}

		template<typename TObj, typename TRet, typename ...TArgs>
		static SFuncFASTCALL<TObj, TRet, TArgs...> funcStripper(TRet(__fastcall TObj::*)(TArgs...))
		{}

		template<typename TOrigF, typename TObj> struct SMethodPartsH
		{
			using TOrigFunction = TOrigF;
			using TObject = TObj;
			using TFS = decltype(funcStripper<TObj>(declval<TOrigF TObject::*>()));
			using TDetour = typename TFS::TDetour;
		};

		template<typename TOrigFunc, typename TObj>
		static SMethodPartsH<TOrigFunc, TObj> getMethodParts(TOrigFunc TObj::*) //WORKS!
		{}

		template<typename TRet, typename TObj, typename ...TArgs>
		static SMethodPartsH<TRet __cdecl (TArgs...), TObj> getMethodParts(TRet (__cdecl TObj::*)(TArgs...)) //WORKS!
		{}


		template<typename TMethod> struct HelpTypes
		{
		private:
			using TParts = decltype(getMethodParts(declval<TMethod>()));
		public:
			using TOrigFunc = typename TParts::TOrigFunction;
			using TObject = typename TParts::TObject;
			using TDetour = typename TParts::TDetour;
		};

		//---------------------------------------------------------------------------

		class CHook
		{
			friend class CMinHookEx;
		protected:
			LPVOID _pfnTarget;
			bool _bHookValid = false;

			struct SCounter
			{
			private:
				atomic_uint &_i;
				SCounter(const SCounter&) = delete;
				SCounter& operator=(const SCounter&) = delete;
			public:
				SCounter(atomic_uint &i) : _i(i)
				{
					_i++;
				}
				~SCounter()
				{
					_i--;
				}
			};

			CHook(LPVOID pfnTarget) : _pfnTarget(pfnTarget)
			{
				threadCounter = 0;
			}

			CHook() : CHook(nullptr)
			{};

			CHook(const CHook&) = delete;
			CHook& operator=(const CHook&) = delete;
			atomic_uint threadCounter;

			virtual void deleteLater()
			{
				MH_DisableHook(_pfnTarget);
				while(threadCounter > 0)
				{
					this_thread::sleep_for(chrono::milliseconds(10));
				}
				MH_RemoveHook(_pfnTarget);
				delete this;
			}

			virtual ~CHook()
			{};

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

		template<typename TFunc> class CFunctionHook : public CHook
		{
			friend class CMinHookEx;

		private:
			CFunctionHook(const CFunctionHook&) = delete;
			CFunctionHook& operator=(const CFunctionHook&) = delete;
		protected:
			CFunctionHook(LPVOID target) : CHook(target)
			{}
		public:
			TFunc* const originalFunc = 0;
		};

		template<typename X, typename TFunc, typename TRet, typename ...TArgs>
		class CFunctionHookSpec : private CFunctionHook < TFunc >
		{
			friend CMinHookEx;
		private:
			struct SThisKeeper
			{
				friend class CFunctionHookSpec;
			private:
				SThisKeeper() = default;
				SThisKeeper(const SThisKeeper&) = delete;
				SThisKeeper& operator=(const SThisKeeper&) = delete;
				static SThisKeeper* getInstance()
				{
					static SThisKeeper* inst = new SThisKeeper();
					return inst;
				}

				CFunctionHookSpec* _pThis;
			};

			TFunc *_detour;
			TFunc *_originalFunc;

			static TRet __stdcall invokeOriginalSTDCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return pThis->_originalFunc(forward<TArgs>(Args)...);
			}

			static TRet __cdecl invokeOriginalCDECL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return pThis->_originalFunc(forward<TArgs>(Args)...);
			}

			static TRet __fastcall invokeOriginalFASTCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return pThis->_originalFunc(forward<TArgs>(Args)...);
			}

			static TRet __cdecl invokeDetourCDECL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour(forward<TArgs>(Args)...);
			}

			static TRet __fastcall invokeDetourFASTCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour(forward<TArgs>(Args)...);
			}

			static TRet __stdcall invokeDetourSTDCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour(forward<TArgs>(Args)...);
			}

			LPVOID detourProxyAddr()
			{
				auto detourInvokerCDECL = &CFunctionHookSpec::invokeDetourCDECL;
				auto detourInvokerFASTCALL = &CFunctionHookSpec::invokeDetourFASTCALL;
				auto detourInvokerSTDCALL = &CFunctionHookSpec::invokeDetourSTDCALL;

				return is_same<TFunc, TRet __stdcall (TArgs...)>::value ?
					(LPVOID*&)detourInvokerSTDCALL :
					is_same<TFunc, TRet __fastcall (TArgs...)>::value ?
					(LPVOID*&)detourInvokerFASTCALL : (LPVOID*&)detourInvokerCDECL;
			}

			TFunc* originalFuncInvokerAddr()
			{
				auto originalInvokerSTDCALL = &CFunctionHookSpec::invokeOriginalSTDCALL;
				auto originalInvokerCDECL = &CFunctionHookSpec::invokeOriginalCDECL;
				auto originalInvokerFASTCALL = &CFunctionHookSpec::invokeOriginalFASTCALL;

				return is_same<TFunc, TRet __fastcall (TArgs...)>::value ?
					(TFunc*&)originalInvokerFASTCALL :
					is_same<TFunc, TRet __cdecl (TArgs...)>::value ?
					(TFunc*&)originalInvokerCDECL : (TFunc*&)originalInvokerSTDCALL;
			}

			CFunctionHookSpec(TFunc *target, TFunc *detour) : CFunctionHook(target), _detour(detour)
			{
				LPVOID _pfnOriginal;

				auto detourInvoker = detourProxyAddr();

				_bHookValid = MH_CreateHook(target, (LPVOID*&)detourInvoker, &_pfnOriginal) == MH_OK;

				_originalFunc = _bHookValid ? (TFunc*)_pfnOriginal : (TFunc*)target; //Used to call unhooked method via invoker

				SThisKeeper::getInstance()->_pThis = this;
				const_cast<TFunc*>(originalFunc) = originalFuncInvokerAddr();
			}

			virtual ~CFunctionHookSpec()
			{
				delete SThisKeeper::getInstance();
			}
		};

		//-------------------------------------------------------------------------
		template<typename TMethod>
		class CMethodHook : public CHook
		{
		private:
			virtual void setObjectThisPtr(typename HelpTypes<TMethod>::TObject* pThis) = 0;

			CMethodHook(const CMethodHook&) = delete;
			CMethodHook& operator=(const CMethodHook&) = delete;

		protected:

			CMethodHook(LPVOID target) : CHook(target)
			{}

			struct SProxyObject
			{
				friend CMethodHook;
			private:
				SProxyObject(typename HelpTypes<TMethod>::TOrigFunc* origMethod) : originalMethod(origMethod)
				{}
				SProxyObject(const SProxyObject&) = delete;
				SProxyObject& operator=(const SProxyObject&) = delete;

			public:
				typename HelpTypes<TMethod>::TOrigFunc* const originalMethod; //user interface; calls original method invoker; if we would have a method pointer here we'd get ugly method-by-pointer call syntax
			} *_proxyObject;

			void initProxyObject(typename HelpTypes<TMethod>::TOrigFunc* origMethod)
			{
				_proxyObject = new SProxyObject(origMethod);
			}

			virtual ~CMethodHook()
			{
				delete _proxyObject;
			}

		public:
			SProxyObject& object(typename HelpTypes<TMethod>::TObject *pThis)
			{
				setObjectThisPtr(pThis);
				return *_proxyObject;
			}

		};

		template<typename X, typename TMethod, typename TRet, typename TObject, typename ...TArgs>
		class CMethodHookSpec : private CMethodHook < TMethod >
		{
			friend class CMinHookEx;

		private:
			unordered_map<thread::id, TObject*> _thisPointers;

			struct SThisKeeper
			{
				friend class CMethodHookSpec;
			private:
				SThisKeeper() = default;
				SThisKeeper(const SThisKeeper&) = delete;
				SThisKeeper& operator=(const SThisKeeper&) = delete;
				static SThisKeeper* getInstance()
				{
					static SThisKeeper* inst = new SThisKeeper();
					return inst;
				}

				CMethodHookSpec* _pThis;
			};

			TObject* objectThisPtr()
			{
				return _thisPointers[this_thread::get_id()];
			}

			virtual void setObjectThisPtr(TObject* pThis) override
			{
				_thisPointers[this_thread::get_id()] = pThis;
			}

			TMethod _originalMethod;
			typename HelpTypes<TMethod>::TDetour _detour;

			static TRet __stdcall invokeOriginalSTDCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return (pThis->objectThisPtr()->*(pThis->_originalMethod))(forward<TArgs>(Args)...);
			}

			static TRet __cdecl invokeOriginalCDECL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return (pThis->objectThisPtr()->*(pThis->_originalMethod))(forward<TArgs>(Args)...);
			}

			static TRet __fastcall invokeOriginalFASTCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				return (pThis->objectThisPtr()->*(pThis->_originalMethod))(forward<TArgs>(Args)...);
			}

			TRet __thiscall invokeDetour(TArgs...Args) //Can't be static because of __thiscall
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour((TObject*)this, forward<TArgs>(Args)...);
			}

			TRet __cdecl invokeDetourCDECL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);

				return pThis->_detour((TObject*)this, forward<TArgs>(Args)...);
			}

			TRet __fastcall invokeDetourFASTCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour((TObject*)this, forward<TArgs>(Args)...);
			}

			TRet __stdcall invokeDetourSTDCALL(TArgs...Args)
			{
				auto pThis = SThisKeeper::getInstance()->_pThis;
				SCounter c(pThis->threadCounter);
				return pThis->_detour((TObject*)this, forward<TArgs>(Args)...);
			}

			LPVOID detourProxyAddr()
			{
				auto detourInvoker = &CMethodHookSpec::invokeDetour;
				auto detourInvokerCDECL = &CMethodHookSpec::invokeDetourCDECL;
				auto detourInvokerFASTCALL = &CMethodHookSpec::invokeDetourFASTCALL;
				auto detourInvokerSTDCALL = &CMethodHookSpec::invokeDetourSTDCALL;
				
				return is_same<TMethod, TRet(__stdcall TObject::*)(TArgs...)>::value ?
					(LPVOID*&)detourInvokerSTDCALL :
					is_same<TMethod, TRet(__fastcall TObject::*)(TArgs...)>::value ?
					(LPVOID*&)detourInvokerFASTCALL :
					is_same<TMethod, TRet(__cdecl TObject::*)(TArgs...)>::value ?
					(LPVOID*&)detourInvokerCDECL : (LPVOID*&)detourInvoker;
			}

			using TOrigFunc = typename HelpTypes<TMethod>::TOrigFunc;

			TOrigFunc* originalMethodInvokerAddr()
			{
				auto originalInvokerSTDCALL = &CMethodHookSpec::invokeOriginalSTDCALL;
				auto originalInvokerCDECL = &CMethodHookSpec::invokeOriginalCDECL;
				auto originalInvokerFASTCALL = &CMethodHookSpec::invokeOriginalFASTCALL;

				return is_same<TOrigFunc, TRet __fastcall (TArgs...)>::value ?
					(TOrigFunc*&)originalInvokerFASTCALL :
					is_same<TOrigFunc, TRet __cdecl (TArgs...)>::value ?
					(TOrigFunc*&)originalInvokerCDECL : (TOrigFunc*&)originalInvokerSTDCALL;
			}

			CMethodHookSpec(LPVOID target, typename HelpTypes<TMethod>::TDetour detour) : CMethodHook(target),
				_detour(detour)
			{
				LPVOID _pfnOriginal;
				auto detourInvoker = detourProxyAddr();

				_bHookValid = MH_CreateHook(target, (LPVOID*&)detourInvoker, &_pfnOriginal) == MH_OK;

				_originalMethod = _bHookValid ? *(TMethod*)&_pfnOriginal : *(TMethod*)&target; //Used to call unhooked method via invoker

				SThisKeeper::getInstance()->_pThis = this;

				initProxyObject(originalMethodInvokerAddr());
			};

			virtual ~CMethodHookSpec()
			{
				delete SThisKeeper::getInstance();
			}
			//------------------------------------------------------------------------
		};

		CMinHookEx()
		{
			MH_Initialize();
		}
		CMinHookEx(const CMinHookEx&) = delete;
		CMinHookEx& operator=(const CMinHookEx&) = delete;

		unordered_map<LPVOID, CHook*> _hooks;

		template <typename X, typename TMethod, typename TRet, typename TObject, typename ...TArgs>
		CMethodHookSpec<X, TMethod, TRet, TObject, TArgs...>
			CMethodHookSpecH(TRet(TObject::*)(TArgs...))
		{}

		template <typename X, typename TMethod, typename TRet, typename TObject, typename ...TArgs>
		CMethodHookSpec<X, TMethod, TRet, TObject, TArgs...>
			CMethodHookSpecH(TRet(__cdecl TObject::*)(TArgs...))
		{}

		template <typename X, typename TMethod, typename TRet, typename TObject, typename ...TArgs>
		CMethodHookSpec<X, TMethod, TRet, TObject, TArgs...>
			CMethodHookSpecH(TRet(__stdcall TObject::*)(TArgs...))
		{}

		template <typename X, typename TMethod, typename TRet, typename TObject, typename ...TArgs>
		CMethodHookSpec<X, TMethod, TRet, TObject, TArgs...>
			CMethodHookSpecH(TRet(__fastcall TObject::*)(TArgs...))
		{}

		template <typename X, typename TFunc, typename TRet, typename ...TArgs>
		CFunctionHookSpec<X, TFunc, TRet, TArgs...>
			CFunctionHookSpecH(TRet(__cdecl *)(TArgs...))
		{}

		template <typename X, typename TFunc, typename TRet, typename ...TArgs>
		CFunctionHookSpec<X, TFunc, TRet, TArgs...>
			CFunctionHookSpecH(TRet(__stdcall*)(TArgs...))
		{}

		template <typename X, typename TFunc, typename TRet, typename ...TArgs>
		CFunctionHookSpec<X, TFunc, TRet, TArgs...>
			CFunctionHookSpecH(TRet(__fastcall*)(TArgs...))
		{}

		bool hookExists(LPVOID target)
		{
			return _hooks.find(target) != _hooks.end();
		}

	public:
		~CMinHookEx()
		{
			removeAll();
		}

		void removeAll()
		{
			for(auto &x : _hooks)
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

		template<typename X, typename TMethod>
		CMethodHook<TMethod>& add(internal::SMethodHookUID<X> methodHookUID, TMethod target, typename HelpTypes<TMethod>::TDetour detour)
		{
			static bool bUsed = false;
			if(bUsed) throw(invalid_argument("Invalid hook UID, use MethodHook or FunctionHook macros"));
			bUsed = true;

			if(hookExists((void*&)target))
				_hooks.at((void*&)target)->deleteLater();

			using TMH = decltype(CMethodHookSpecH<X, TMethod>(declval<TMethod>()));
			auto h = new TMH((LPVOID)(void*&)target, detour);
			_hooks[(void*&)target] = h;
			return *h;
		}

		template<typename X, typename TMethod>
		CMethodHook<TMethod>& add(internal::SMethodHookUID<X> methodHookUID, TMethod target, typename HelpTypes<TMethod>::TObject *object, typename HelpTypes<TMethod>::TDetour detour)
		{
			static bool bUsed = false;
			if(bUsed) throw(exception("Invalid hook UID, use MethodHook or FunctionHook macros"));
			bUsed = true;

			if(hookExists((void*&)target))
				_hooks.at((void*&)target)->deleteLater();

			int vtblOffset = VTableIndex(target);
			int *vtbl = (int*)*(int*)object;
			LPVOID t = (LPVOID)vtbl[vtblOffset];
			using TMH = decltype(CMethodHookSpecH<X, TMethod>(declval<TMethod>()));
			auto h = new TMH(t, detour);

			_hooks[(void*&)target] = h;

			return *h;
		}

		template<typename X, typename TFunc>
		CFunctionHook<TFunc>& add(internal::SFunctionHookUID<X> functionHookUID, TFunc* target, decltype(target) detour)
		{
			static bool bUsed = false;
			if(bUsed) throw(exception("Invalid hook UID, use MethodHook or FunctionHook macros"));
			bUsed = true;

			if(hookExists(target))
				_hooks.at(target)->deleteLater();

			using TMH = decltype(CFunctionHookSpecH<X, TFunc>(declval<TFunc*>()));
			auto h = new TMH(target, detour);

			_hooks[target] = h;
			return *h;
		}
		
		template<typename TFunc>
		CFunctionHook<TFunc>* operator[](TFunc *target)
		{
			return (CFunctionHook<TFunc>*)_hooks[target];
		}
		
		template<typename TMethod>
		CMethodHook<TMethod>* operator[](TMethod target)
		{
			return (CMethodHook<TMethod>*)_hooks[(void*&)target];
		}
	};
};