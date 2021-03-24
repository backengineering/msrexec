<img src="https://imgur.com/nNnOCPK.png"/>

# MSREXEC - Elevate Arbitrary WRMSR To Kernel Execution

msrexec is a small project that can be used to elevate arbitrary MSR writes to kernel execution on 64 bit Windows-10 systems. This project is part of the VDM (vulnerable driver manipulation) namespace
and can be integrated into any prior VDM projects. Although this project falls under the VDM namespace, Voyager and bluepill can be used to provide arbitrary wrmsr writes.

#### Features

* integration with VDM
* integration with Voyager and Bluepill
* WARNING: does not work under most anti virus hypervisors or HVCI systems...
* Use any vulnerable driver which exposes arbitrary WRMSR to obtain kernel exeuction
* Works under KVA shadowing (you will still need to run as admin however to load the driver, LSTAR points to KiSystemCall64Shadow though and that is taken into consideration...)

# Example - Usage/Demo

In order to create a `vdm::msrexec_ctx` you must first create a lambda which will be passed to the constructor. The lambda must be a wrapper function which will
in turn, be used internally by the class to write to MSR's. In my example im simply forwarding the call to a predefined routine in vdm.hpp.

```cpp
writemsr_t _write_msr = [&](std::uint32_t reg, std::uintptr_t value) -> bool
{
	// put your code here to write MSR....
	// the code is defined in vdm::writemsr for me...
	return vdm::writemsr(reg, value);
};
```

Once you have a lambda defined like this you can go ahead and create a `vdm::msrexec_ctx`. The lambda you pass to `vdm::msrexec_ctx::exec` will be executed in ring-0. Please note that you should be very aware of what you are calling in this lambda as to not make any printfs, malloc's, std::vector::push_back, or anything that might syscall. Also note that the lambda you pass must be of type `std::function<void(void*, get_system_routine_t)>`.

```cpp
vdm::msrexec_ctx msrexec(_write_msr);
msrexec.exec([&](void* krnl_base, get_system_routine_t get_kroutine) -> void
{
	const auto dbg_print =
		reinterpret_cast<dbg_print_t>(
			get_kroutine(krnl_base, "DbgPrint"));

	const auto ex_alloc_pool =
		reinterpret_cast<ex_alloc_pool_t>(
			get_kroutine(krnl_base, "ExAllocatePool"));

	dbg_print("> allocated pool -> 0x%p\n", ex_alloc_pool(NULL, 0x1000));
	dbg_print("> cr4 -> 0x%p\n", __readcr4());
	dbg_print("> hello world!\n");
});
```

Result: 

```
> allocated pool -> 0xFFFFAA8B13AD1000
> cr4 -> 0x0000000000020678
> hello world!
```

# Syscall - Fast System Call

SYSCALL invokes an OS system-call handler at privilege level 0. It does so by ***loading RIP from the IA32_LSTAR MSR*** (after saving the address of the instruction following SYSCALL into RCX). (The WRMSR instruction ensures that the IA32_LSTAR MSR always contain a canonical address.)

SYSCALL also saves RFLAGS into R11 and then masks RFLAGS using the IA32_FMASK MSR (MSR address C0000084H); specifically, the processor clears in RFLAGS every bit corresponding to a bit that is set in the IA32_FMASK MSR.

SYSCALL loads the CS and SS selectors with values derived from bits 47:32 of the IA32_STAR MSR. However, the CS and SS descriptor caches are not loaded from the descriptors (in GDT or LDT) referenced by those selectors. Instead, the descriptor caches are loaded with fixed values. See the Operation section for details. It is the responsibility of OS software to ensure that the descriptors (in GDT or LDT) referenced by those selector values correspond to the fixed values loaded into the descriptor caches; the SYSCALL instruction does not ensure this correspondence.

***The SYSCALL instruction does not save the stack pointer (RSP).*** If the OS system-call handler will change the stack pointer, it is the responsibility of software to save the previous value of the stack pointer. This might be done prior to executing SYSCALL, with software restoring the stack pointer with the instruction following SYSCALL (which will be executed after SYSRET). Alternatively, the OS system-call handler may save the stack pointer and restore it before executing SYSRET.

# ROP - Return-Oriented Programming

ROP or return-oriented programming, is a technique where an attacker gains control of the call stack to hijack program control flow and then executes carefully chosen machine instruction sequences that are already present in the machine's memory, called "gadgets". Note: ***"The SYSCALL instruction does not save the stack pointer (RSP)"***. This allows for an attacker to set up the stack with addresses of ROP gadgets. 

### SMEP - Supervisor Mode Execution Protection

SMEP or Supervisor Mode Execution Protection, prevents a logical processor with a lower CPL from executing code mapped into virtual memory with the super supervisor bit set. This is relevant to this project as one could not simply set LSTAR to a user controlled page. However, with ROP one could disable SMEP by executing the following gadgets:

```nasm
pop rcx
ret
```

```nasm
mov cr4, rcx
ret
```

However, when the syscall instruction is executed, the address of the next instruction (the one after the syscall instruction) is placed into RCX. In order to preserve RIP, it should be placed onto the stack before any addresses of gadgets are placed onto the stack.

```nasm
lea rax, finish
push rax
```

Changing IA32_LSTAR to a ROP chain as described above will work just fine on CPU's that don't support SMAP. Windows 10 will use SMAP if your CPU supports it. This means RSP is inaccessible since it is a user controlled page. 

### SMAP - Supervisor Mode Access Prevention (Win10 19H1 and up...)

SMAP or Supervisor Mode Access Prevention is a CPU protection which prevents accessing data controlled by a higher CPL. In other words, if SMAP is set in CR4, a logical
processor executing kernel code cannot access usermode controlled pages (user supervisor).

This is an issue with ROP as RSP after a syscall contains a usermode address. Interfacing with this usermode stack in any way will cause a fault. However, you can essentially disable SMAP from usermode. There is a bit in the RFLAGS register which can be set to nullify SMAP. The instruction to set this bit is called `STAC` (Set AC Flag in EFLAGS Register). However this instruction is privilaged and will throw a #UD. However as [@drew](https://twitter.com/drewbervisor) pointed out, you can `POPFQ` a RFLAGS value with that bit set and the CPU will not throw any exceptions. I assumed that since `STAC` cannot be used in usermode, that `POPFQ` would also throw an exception, however this is not the case... Again thank you [@drew](https://twitter.com/drewbervisor), without this key information the project would have been a complete mess as there are no usable `mov cr4, [non rax registers] ; ret` gadgets which exist across windows versions.

```nasm
pushfq                          ; thank you drew :)
pop rax                         ; this will set the AC flag in RFLAGS which "disables SMAP"...
or rax, 040000h                 ;
push rax                        ;
popfq                           ;
```

RFLAGS is restored after the syscall instruction. The original RFLAGS value is pushed onto the stack prior to all of the gadgets and other values.

```nasm
syscall                         ; LSTAR points at a pop rcx gadget... 
                                ; it will put m_smep_off into rcx...
finish:
popfq                           ; restore EFLAGS...
pop r10                         ; restore r10...
ret
```

<img src="https://imgur.com/OVC3LGH.png"/>

### SFMASK - If a bit in this is set, the corresponding bit in rFLAGS is cleared.

On Win10 this MSR is set to `0x4700` or `0100 0111 0000 0000`, as you can see bit 18 is not set, which means the AC flag is not cleared when syscall is execute. This means you can disable SMAP from usermode... credits to [@drew](https://twitter.com/drewbervisor) for pointing this out. I think Microsoft is unaware that you can set AC from usermode. (I was also...)

# Credit - Special Thanks

* [@drew](https://twitter.com/drewbervisor) - pointing out AC bit in RFLAGS can be set in usermode. I originally assumed since the `STAC` instruction could not be executed in usermode that `POPFQ` would throw an exception if AC bit was high and CPL was greater then zero. Without this key information the project would have been a complete mess. Thank you!
* [@0xnemi](https://twitter.com/0xnemi) / [@everdox](https://twitter.com/nickeverdox) - [mov ss/pop ss exploit](https://www.youtube.com/watch?v=iU_No7gdcwc) 0xnemi's use of syscall and the fact that RSP is not changed + use of ROP made me think about how there are alot of vulnerable drivers that expose arbitrary wrmsr which could be used to change LSTAR and effectivlly replicate his solution...
* [@Ch3rn0byl](https://twitter.com/notCh3rn0byl) - donation of a few vulnerable drivers which exposed arbitrary WRMSR/helped test with KVA shadowing enabled/disabled. 
* [@namazso](https://twitter.com/namazso) - originally hinting at this project many months ago. its finally done :)
* [@btbd](https://github.com/btbd) - pointing out that LSTAR points to KiSystemCall64Shadow and not KiSystemCall64 when KVA shadowing is enabled, reguardless of AddressPolicy...
* [Device Driver Debauchery and MSR Madness](https://vimeo.com/335216903)

# License

TL;DR: if you use this project, rehost it, put it on github, include `_xeroxz` in your release.

BSD 3-Clause License

Copyright (c) 2021, _xeroxz
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.