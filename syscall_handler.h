#pragma once

extern "C" unsigned long long cr4_value;
extern "C" unsigned long kpcr_rsp_offset;
extern "C" unsigned long kpcr_krsp_offset;
extern "C" void syscall_handler(unsigned long long rip);