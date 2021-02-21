#include "msrexec.hpp"
#include "vdm.hpp"
#include <iostream>

using ex_alloc_pool_t = void* (*)(std::uint32_t, std::size_t);
using dbg_print_t = void(*)(const char*, ...);

int __cdecl main(int argc, char** argv)
{
	const auto [drv_handle, drv_key, drv_status] = vdm::load_drv();
	if (drv_status != STATUS_SUCCESS)
	{
		std::printf("> failed to load driver... reason -> 0x%x\n", drv_status);
		return {};
	}

	std::printf("drv handle -> 0x%x, drv key -> %s, drv status -> 0x%x\n", drv_handle, drv_key.c_str(), drv_status);
	std::printf("ntoskrnl base address -> 0x%p\n", utils::kmodule::get_base("ntoskrnl.exe"));
	std::printf("NtShutdownSystem -> 0x%p\n", utils::kmodule::get_export("ntoskrnl.exe", "NtShutdownSystem"));

	writemsr_t _write_msr = 
		[&](std::uint32_t reg, std::uintptr_t value) -> void
	{
		// put your code here to write MSR....
		// the code is defined in vdm::writemsr for me...
		vdm::writemsr(reg, value); 
	};

	const auto ex_alloc_pool =
		reinterpret_cast<ex_alloc_pool_t>(
			utils::kmodule::get_export(
				"ntoskrnl.exe", "ExAllocatePool"));

	const auto dbg_print = 
		reinterpret_cast<dbg_print_t>(
			utils::kmodule::get_export(
				"ntoskrnl.exe", "DbgPrint"));

	vdm::msrexec_ctx msrexec(_write_msr);
	std::printf("press enter to run 100 syscall tests...\n");
	std::getchar();

	for (auto idx = 0u; idx < 100; ++idx)
	{
		msrexec.exec([&ex_alloc_pool, &dbg_print]() -> void
		{ 
			dbg_print("> allocated pool -> 0x%p\n",
				ex_alloc_pool(NULL, 0x1000));
		});
	}

	const auto unload_result = 
		vdm::unload_drv(drv_handle, drv_key);

	if (unload_result != STATUS_SUCCESS)
	{
		std::printf("> unable to unload driver... reason -> 0x%x\n", unload_result);
		return {};
	}

	std::printf("completed tests...\n");
	std::getchar();
}