#pragma once

#ifndef _AARCH64_ARCH_H_
#define _AARCH64_ARCH_H_

#include <common/defines.h>

static inline usize cpuid() {
    u64 id;
    asm volatile("mrs %[x], mpidr_el1" : [x] "=r"(id));
    return id & 0xff;
}

// instruct compiler not to reorder instructions around the fence.
static inline void compiler_fence() {
    asm volatile("" ::: "memory");
}

static inline u64 get_clock_frequency() {
    u64 result;
    asm volatile("mrs %[freq], cntfrq_el0" : [freq] "=r"(result));
    return result;
}

static inline u64 get_timestamp() {
    u64 result;
    compiler_fence();
    asm volatile("mrs %[cnt], cntpct_el0" : [cnt] "=r"(result));
    compiler_fence();
    return result;
}

// instruction synchronization barrier.
static inline void arch_isb() {
    asm volatile("isb" ::: "memory");
}

// data synchronization barrier.
static inline void arch_dsb_sy() {
    asm volatile("dsb sy" ::: "memory");
}

static inline void arch_fence() {
    arch_dsb_sy();
    arch_isb();
}

// for `device_get/put_*`, there's no need to protect them with architectual
// barriers, since they are intended to access device memory regions. These
// regions are already marked as nGnRnE in `kernel_pt`.

static inline void device_put_u32(u64 addr, u32 value) {
    compiler_fence();
    *(volatile u32 *)addr = value;
    compiler_fence();
}

static inline u32 device_get_u32(u64 addr) {
    compiler_fence();
    u32 value = *(volatile u32 *)addr;
    compiler_fence();
    return value;
}

// read Exception Syndrome Register (EL1).
static inline u64 arch_get_esr() {
    u64 result;
    arch_fence();
    asm volatile("mrs %[x], esr_el1" : [x] "=r"(result));
    arch_fence();
    return result;
}

// reset Exception Syndrome Register (EL1) to zero.
static inline void arch_reset_esr() {
    arch_fence();
    asm volatile("msr esr_el1, %[x]" : : [x] "r"(0ll));
    arch_fence();
}

// read Exception Link Register (EL1).
static inline u64 arch_get_elr() {
    u64 result;
    arch_fence();
    asm volatile("mrs %[x], elr_el1" : [x] "=r"(result));
    arch_fence();
    return result;
}

// set vector base (virtual) address register (EL1).
static inline void arch_set_vbar(void *ptr) {
    arch_fence();
    asm volatile("msr vbar_el1, %[x]" : : [x] "r"(ptr));
    arch_fence();
}

// flush TLB entries.
static inline void arch_tlbi_vmalle1is() {
    arch_fence();
    asm volatile("tlbi vmalle1is");
    arch_fence();
}

// set Translation Table Base Register 0 (EL1).
static inline void arch_set_ttbr0(u64 addr) {
    arch_fence();
    asm volatile("msr ttbr0_el1, %[x]" : : [x] "r"(addr));
    arch_tlbi_vmalle1is();
}

// set Translation Table Base Register 1 (EL1).
static inline void arch_set_ttbr1(u64 addr) {
    arch_fence();
    asm volatile("msr ttbr1_el1, %[x]" : : [x] "r"(addr));
    arch_tlbi_vmalle1is();
}

// read & set tid (may be used as a pointer?)
// No need to add fence since added in arch_set_tid
static inline u64 arch_get_tid() {
    u64 tid;
    // arch_fence();
    asm volatile("mrs %[x], tpidr_el1" : [x] "=r"(tid));
    // arch_fence();
    return tid;
}
static inline void arch_set_tid(u64 tid) {
    arch_fence();
    asm volatile("msr tpidr_el1, %[x]" : : [x] "r"(tid));
    arch_fence();
}

// read & set user stack pointer
static inline u64 arch_get_usp() {
    u64 usp;
    arch_fence();
    asm volatile("mrs %[x], sp_el0" : [x] "=r"(usp));
    arch_fence();
    return usp;
}
static inline void arch_set_usp(u64 usp) {
    arch_fence();
    asm volatile("msr sp_el0, %[x]" : : [x] "r"(usp));
    arch_fence();
}

// set-event instruction.
static inline void arch_sev() {
    asm volatile("sev" ::: "memory");
}

// wait-for-event instruction.
static inline void arch_wfe() {
    asm volatile("wfe" ::: "memory");
}

// wait-for-interrupt instruction.
static inline void arch_wfi() {
    asm volatile("wfi" ::: "memory");
}

// yield instruction.
static inline void arch_yield() {
    asm volatile("yield" ::: "memory");
}

static inline bool arch_enable_trap() {
    u64 t;
    asm volatile("mrs %[x], daif" : [x] "=r"(t));
    if (t == 0)
        return true;
    asm volatile("msr daif, %[x]" ::[x] "r"(0ll));
    return false;
}

static inline bool arch_disable_trap() {
    u64 t;
    asm volatile("mrs %[x], daif" : [x] "=r"(t));
    if (t != 0)
        return false;
    asm volatile("msr daif, %[x]" ::[x] "r"(0xfll << 6));
    return true;
}

void delay_us(u64 n);

#endif
