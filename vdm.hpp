#pragma once
#include <windows.h>
#include <cstdint>

#include "loadup.hpp"
#include "raw_driver.hpp"
#define IOCTL_WRMSR 0x229384

#pragma pack (push, 1)
typedef struct _write_msr_t
{
	std::uint32_t reg;
	std::uintptr_t value;
} write_msr_t, * pwrite_msr_t;
#pragma pack (pop)

namespace vdm
{
	inline HANDLE drv_handle;
	inline auto load_drv() -> std::tuple<HANDLE, std::string, NTSTATUS>
	{
		const auto [result, key] =
			driver::load(
				raw_driver,
				sizeof raw_driver
			);

		if (result != STATUS_SUCCESS)
			return { {}, {}, result };

		std::string symlink("\\\\.\\" + key);
		vdm::drv_handle = CreateFileA(
			symlink.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			NULL,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		return { vdm::drv_handle, key, result };
	}

	inline auto unload_drv(HANDLE drv_handle, std::string drv_key) -> NTSTATUS
	{
		if (!CloseHandle(drv_handle))
			return STATUS_FAIL_CHECK;

		return driver::unload(drv_key);
	}

	inline auto writemsr(std::uint32_t reg, std::uintptr_t value) -> bool
	{
		std::uint32_t bytes_handled;
		write_msr_t io_data{ reg, value };

		return DeviceIoControl
		(
			vdm::drv_handle, IOCTL_WRMSR,
			&io_data, sizeof io_data,
			&io_data, sizeof io_data,
			(LPDWORD)&bytes_handled, nullptr
		);
	}
}