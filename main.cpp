#include "msrexec.hpp"
#include "vdm.hpp"
#include <iostream>

int __cdecl main(int argc, char** argv)
{
	const auto [drv_handle, drv_key, drv_status] = vdm::load_drv();
	if (drv_status != STATUS_SUCCESS)
	{
		std::printf("> failed to load driver... reason -> 0x%x\n", drv_status);
		return {};
	}

	std::printf("drv handle -> 0x%x, drv key -> %s, drv status -> 0x%x\n",
		drv_handle, drv_key.c_str(), drv_status);
	std::getchar();

	std::printf("ntoskrnl base address -> 0x%p\n", utils::kmodule::get_base("ntoskrnl.exe"));
	std::printf("NtShutdownSystem -> 0x%p\n", utils::kmodule::get_export("ntoskrnl.exe", "NtShutdownSystem"));

	vdm::writemsr_t _write_msr = 
		[&](std::uint32_t reg, std::uintptr_t value) -> void
	{ vdm::writemsr(reg, value); };

	sizeof write_msr_t;
	vdm::msrexec_ctx msrexec(_write_msr);
	msrexec.exec([&]() -> void { int a = 10; });

	const auto unload_result = 
		vdm::unload_drv(drv_handle, drv_key);

	if (unload_result != STATUS_SUCCESS)
	{
		std::printf("> unable to unload driver... reason -> 0x%x\n", unload_result);
		return {};
	}
}