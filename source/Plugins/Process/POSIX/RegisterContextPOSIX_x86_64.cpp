//===-- RegisterContextPOSIX_x86_64.cpp -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <cstring>
#include <errno.h>
#include <stdint.h>

#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Host/Endian.h"
#include "llvm/Support/Compiler.h"

#include "ProcessPOSIX.h"
#include "RegisterContextPOSIX_i386.h"
#include "RegisterContext_x86.h"
#include "RegisterContextPOSIX_x86_64.h"
#include "Plugins/Process/elf-core/ProcessElfCore.h"

using namespace lldb_private;
using namespace lldb;

enum
{
    gcc_dwarf_gpr_rax = 0,
    gcc_dwarf_gpr_rdx,
    gcc_dwarf_gpr_rcx,
    gcc_dwarf_gpr_rbx,
    gcc_dwarf_gpr_rsi,
    gcc_dwarf_gpr_rdi,
    gcc_dwarf_gpr_rbp,
    gcc_dwarf_gpr_rsp,
    gcc_dwarf_gpr_r8,
    gcc_dwarf_gpr_r9,
    gcc_dwarf_gpr_r10,
    gcc_dwarf_gpr_r11,
    gcc_dwarf_gpr_r12,
    gcc_dwarf_gpr_r13,
    gcc_dwarf_gpr_r14,
    gcc_dwarf_gpr_r15,
    gcc_dwarf_gpr_rip,
    gcc_dwarf_fpu_xmm0,
    gcc_dwarf_fpu_xmm1,
    gcc_dwarf_fpu_xmm2,
    gcc_dwarf_fpu_xmm3,
    gcc_dwarf_fpu_xmm4,
    gcc_dwarf_fpu_xmm5,
    gcc_dwarf_fpu_xmm6,
    gcc_dwarf_fpu_xmm7,
    gcc_dwarf_fpu_xmm8,
    gcc_dwarf_fpu_xmm9,
    gcc_dwarf_fpu_xmm10,
    gcc_dwarf_fpu_xmm11,
    gcc_dwarf_fpu_xmm12,
    gcc_dwarf_fpu_xmm13,
    gcc_dwarf_fpu_xmm14,
    gcc_dwarf_fpu_xmm15,
    gcc_dwarf_fpu_stmm0,
    gcc_dwarf_fpu_stmm1,
    gcc_dwarf_fpu_stmm2,
    gcc_dwarf_fpu_stmm3,
    gcc_dwarf_fpu_stmm4,
    gcc_dwarf_fpu_stmm5,
    gcc_dwarf_fpu_stmm6,
    gcc_dwarf_fpu_stmm7,
    gcc_dwarf_fpu_ymm0,
    gcc_dwarf_fpu_ymm1,
    gcc_dwarf_fpu_ymm2,
    gcc_dwarf_fpu_ymm3,
    gcc_dwarf_fpu_ymm4,
    gcc_dwarf_fpu_ymm5,
    gcc_dwarf_fpu_ymm6,
    gcc_dwarf_fpu_ymm7,
    gcc_dwarf_fpu_ymm8,
    gcc_dwarf_fpu_ymm9,
    gcc_dwarf_fpu_ymm10,
    gcc_dwarf_fpu_ymm11,
    gcc_dwarf_fpu_ymm12,
    gcc_dwarf_fpu_ymm13,
    gcc_dwarf_fpu_ymm14,
    gcc_dwarf_fpu_ymm15
};

enum
{
    gdb_gpr_rax     =   0,
    gdb_gpr_rbx     =   1,
    gdb_gpr_rcx     =   2,
    gdb_gpr_rdx     =   3,
    gdb_gpr_rsi     =   4,
    gdb_gpr_rdi     =   5,
    gdb_gpr_rbp     =   6,
    gdb_gpr_rsp     =   7,
    gdb_gpr_r8      =   8,
    gdb_gpr_r9      =   9,
    gdb_gpr_r10     =  10,
    gdb_gpr_r11     =  11,
    gdb_gpr_r12     =  12,
    gdb_gpr_r13     =  13,
    gdb_gpr_r14     =  14,
    gdb_gpr_r15     =  15,
    gdb_gpr_rip     =  16,
    gdb_gpr_rflags  =  17,
    gdb_gpr_cs      =  18,
    gdb_gpr_ss      =  19,
    gdb_gpr_ds      =  20,
    gdb_gpr_es      =  21,
    gdb_gpr_fs      =  22,
    gdb_gpr_gs      =  23,
    gdb_fpu_stmm0   =  24,
    gdb_fpu_stmm1   =  25,
    gdb_fpu_stmm2   =  26,
    gdb_fpu_stmm3   =  27,
    gdb_fpu_stmm4   =  28,
    gdb_fpu_stmm5   =  29,
    gdb_fpu_stmm6   =  30,
    gdb_fpu_stmm7   =  31,
    gdb_fpu_fcw     =  32,
    gdb_fpu_fsw     =  33,
    gdb_fpu_ftw     =  34,
    gdb_fpu_cs_64   =  35,
    gdb_fpu_ip      =  36,
    gdb_fpu_ds_64   =  37,
    gdb_fpu_dp      =  38,
    gdb_fpu_fop     =  39,
    gdb_fpu_xmm0    =  40,
    gdb_fpu_xmm1    =  41,
    gdb_fpu_xmm2    =  42,
    gdb_fpu_xmm3    =  43,
    gdb_fpu_xmm4    =  44,
    gdb_fpu_xmm5    =  45,
    gdb_fpu_xmm6    =  46,
    gdb_fpu_xmm7    =  47,
    gdb_fpu_xmm8    =  48,
    gdb_fpu_xmm9    =  49,
    gdb_fpu_xmm10   =  50,
    gdb_fpu_xmm11   =  51,
    gdb_fpu_xmm12   =  52,
    gdb_fpu_xmm13   =  53,
    gdb_fpu_xmm14   =  54,
    gdb_fpu_xmm15   =  55,
    gdb_fpu_mxcsr   =  56,
    gdb_fpu_ymm0    =  57,
    gdb_fpu_ymm1    =  58,
    gdb_fpu_ymm2    =  59,
    gdb_fpu_ymm3    =  60,
    gdb_fpu_ymm4    =  61,
    gdb_fpu_ymm5    =  62,
    gdb_fpu_ymm6    =  63,
    gdb_fpu_ymm7    =  64,
    gdb_fpu_ymm8    =  65,
    gdb_fpu_ymm9    =  66,
    gdb_fpu_ymm10   =  67,
    gdb_fpu_ymm11   =  68,
    gdb_fpu_ymm12   =  69,
    gdb_fpu_ymm13   =  70,
    gdb_fpu_ymm14   =  71,
    gdb_fpu_ymm15   =  72
};

static const
uint32_t g_gpr_regnums[k_num_gpr_registers] =
{
    gpr_rax,
    gpr_rbx,
    gpr_rcx,
    gpr_rdx,
    gpr_rdi,
    gpr_rsi,
    gpr_rbp,
    gpr_rsp,
    gpr_r8,
    gpr_r9,
    gpr_r10,
    gpr_r11,
    gpr_r12,
    gpr_r13,
    gpr_r14,
    gpr_r15,
    gpr_rip,
    gpr_rflags,
    gpr_cs,
    gpr_fs,
    gpr_gs,
    gpr_ss,
    gpr_ds,
    gpr_es,
    gpr_eax,
    gpr_ebx,
    gpr_ecx,
    gpr_edx,
    gpr_edi,
    gpr_esi,
    gpr_ebp,
    gpr_esp,
    gpr_eip,
    gpr_eflags
};

static const uint32_t
g_fpu_regnums[k_num_fpr_registers] =
{
    fpu_fcw,
    fpu_fsw,
    fpu_ftw,
    fpu_fop,
    fpu_ip,
    fpu_cs,
    fpu_dp,
    fpu_ds,
    fpu_mxcsr,
    fpu_mxcsrmask,
    fpu_stmm0,
    fpu_stmm1,
    fpu_stmm2,
    fpu_stmm3,
    fpu_stmm4,
    fpu_stmm5,
    fpu_stmm6,
    fpu_stmm7,
    fpu_xmm0,
    fpu_xmm1,
    fpu_xmm2,
    fpu_xmm3,
    fpu_xmm4,
    fpu_xmm5,
    fpu_xmm6,
    fpu_xmm7,
    fpu_xmm8,
    fpu_xmm9,
    fpu_xmm10,
    fpu_xmm11,
    fpu_xmm12,
    fpu_xmm13,
    fpu_xmm14,
    fpu_xmm15
};

static const uint32_t
g_avx_regnums[k_num_avx_registers] =
{
    fpu_ymm0,
    fpu_ymm1,
    fpu_ymm2,
    fpu_ymm3,
    fpu_ymm4,
    fpu_ymm5,
    fpu_ymm6,
    fpu_ymm7,
    fpu_ymm8,
    fpu_ymm9,
    fpu_ymm10,
    fpu_ymm11,
    fpu_ymm12,
    fpu_ymm13,
    fpu_ymm14,
    fpu_ymm15
};

// Number of register sets provided by this context.
enum
{
    k_num_extended_register_sets = 1,
    k_num_register_sets = 3
};

static const RegisterSet
g_reg_sets[k_num_register_sets] =
{
    { "General Purpose Registers",  "gpr", k_num_gpr_registers, g_gpr_regnums },
    { "Floating Point Registers",   "fpu", k_num_fpr_registers, g_fpu_regnums },
    { "Advanced Vector Extensions", "avx", k_num_avx_registers, g_avx_regnums }
};

// Computes the offset of the given FPR in the extended data area.
#define FPR_OFFSET(regname) \
    (offsetof(RegisterContextPOSIX_x86_64::FPR, xstate) + \
     offsetof(RegisterContextPOSIX_x86_64::FXSAVE, regname))

// Computes the offset of the YMM register assembled from register halves.
#define YMM_OFFSET(regname) \
    (offsetof(RegisterContextPOSIX_x86_64::YMM, regname))

// Number of bytes needed to represent a i386 GPR
#define GPR_i386_SIZE(reg) sizeof(((RegisterContextPOSIX_i386::GPR*)NULL)->reg)

// Number of bytes needed to represent a FPR.
#define FPR_SIZE(reg) sizeof(((RegisterContextPOSIX_x86_64::FXSAVE*)NULL)->reg)

// Number of bytes needed to represent the i'th FP register.
#define FP_SIZE sizeof(((RegisterContextPOSIX_x86_64::MMSReg*)NULL)->bytes)

// Number of bytes needed to represent an XMM register.
#define XMM_SIZE sizeof(RegisterContextPOSIX_x86_64::XMMReg)

// Number of bytes needed to represent a YMM register.
#define YMM_SIZE sizeof(RegisterContextPOSIX_x86_64::YMMReg)

// Note that the size and offset will be updated by platform-specific classes.
#define DEFINE_GPR(reg, alt, kind1, kind2, kind3, kind4)           \
    { #reg, alt, 0, 0, eEncodingUint,                              \
      eFormatHex, { kind1, kind2, kind3, kind4, gpr_##reg }, NULL, NULL }

// Dummy data for RegisterInfo::value_regs as expected by DumpRegisterSet. 
static uint32_t value_regs = LLDB_INVALID_REGNUM;

#define DEFINE_GPR_i386(reg_i386, reg_x86_64, alt, kind1, kind2, kind3, kind4) \
    { #reg_i386, alt, GPR_i386_SIZE(reg_i386), 0, eEncodingUint,   \
      eFormatHex, { kind1, kind2, kind3, kind4, gpr_##reg_i386 }, &value_regs, NULL }

#define DEFINE_FPR(reg, kind1, kind2, kind3, kind4)                \
    { #reg, NULL, FPR_SIZE(reg), FPR_OFFSET(reg), eEncodingUint,   \
      eFormatHex, { kind1, kind2, kind3, kind4, fpu_##reg }, NULL, NULL }

#define DEFINE_FP(reg, i)                                          \
    { #reg#i, NULL, FP_SIZE, LLVM_EXTENSION FPR_OFFSET(reg[i]),    \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { gcc_dwarf_fpu_##reg##i, gcc_dwarf_fpu_##reg##i,            \
        LLDB_INVALID_REGNUM, gdb_fpu_##reg##i, fpu_##reg##i }, NULL, NULL }

#define DEFINE_XMM(reg, i)                                         \
    { #reg#i, NULL, XMM_SIZE, LLVM_EXTENSION FPR_OFFSET(reg[i]),   \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { gcc_dwarf_fpu_##reg##i, gcc_dwarf_fpu_##reg##i,            \
        LLDB_INVALID_REGNUM, gdb_fpu_##reg##i, fpu_##reg##i }, NULL, NULL }

#define DEFINE_YMM(reg, i)                                         \
    { #reg#i, NULL, YMM_SIZE, LLVM_EXTENSION YMM_OFFSET(reg[i]),   \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { gcc_dwarf_fpu_##reg##i, gcc_dwarf_fpu_##reg##i,            \
        LLDB_INVALID_REGNUM, gdb_fpu_##reg##i, fpu_##reg##i }, NULL, NULL }

#define DEFINE_DR(reg, i)                                              \
    { #reg#i, NULL, 0, 0, eEncodingUint, eFormatHex,                   \
      { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, \
      LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM }, NULL, NULL }

#define REG_CONTEXT_SIZE (GetGPRSize() + sizeof(RegisterContextPOSIX_x86_64::FPR))

static RegisterInfo
g_register_infos[k_num_registers] =
{
    // General purpose registers.
    DEFINE_GPR(rax,    NULL,    gcc_dwarf_gpr_rax,   gcc_dwarf_gpr_rax,   LLDB_INVALID_REGNUM,       gdb_gpr_rax),
    DEFINE_GPR(rbx,    NULL,    gcc_dwarf_gpr_rbx,   gcc_dwarf_gpr_rbx,   LLDB_INVALID_REGNUM,       gdb_gpr_rbx),
    DEFINE_GPR(rcx,    NULL,    gcc_dwarf_gpr_rcx,   gcc_dwarf_gpr_rcx,   LLDB_INVALID_REGNUM,       gdb_gpr_rcx),
    DEFINE_GPR(rdx,    NULL,    gcc_dwarf_gpr_rdx,   gcc_dwarf_gpr_rdx,   LLDB_INVALID_REGNUM,       gdb_gpr_rdx),
    DEFINE_GPR(rdi,    NULL,    gcc_dwarf_gpr_rdi,   gcc_dwarf_gpr_rdi,   LLDB_INVALID_REGNUM,       gdb_gpr_rdi),
    DEFINE_GPR(rsi,    NULL,    gcc_dwarf_gpr_rsi,   gcc_dwarf_gpr_rsi,   LLDB_INVALID_REGNUM,       gdb_gpr_rsi),
    DEFINE_GPR(rbp,    "fp",    gcc_dwarf_gpr_rbp,   gcc_dwarf_gpr_rbp,   LLDB_REGNUM_GENERIC_FP,    gdb_gpr_rbp),
    DEFINE_GPR(rsp,    "sp",    gcc_dwarf_gpr_rsp,   gcc_dwarf_gpr_rsp,   LLDB_REGNUM_GENERIC_SP,    gdb_gpr_rsp),
    DEFINE_GPR(r8,     NULL,    gcc_dwarf_gpr_r8,    gcc_dwarf_gpr_r8,    LLDB_INVALID_REGNUM,       gdb_gpr_r8),
    DEFINE_GPR(r9,     NULL,    gcc_dwarf_gpr_r9,    gcc_dwarf_gpr_r9,    LLDB_INVALID_REGNUM,       gdb_gpr_r9),
    DEFINE_GPR(r10,    NULL,    gcc_dwarf_gpr_r10,   gcc_dwarf_gpr_r10,   LLDB_INVALID_REGNUM,       gdb_gpr_r10),
    DEFINE_GPR(r11,    NULL,    gcc_dwarf_gpr_r11,   gcc_dwarf_gpr_r11,   LLDB_INVALID_REGNUM,       gdb_gpr_r11),
    DEFINE_GPR(r12,    NULL,    gcc_dwarf_gpr_r12,   gcc_dwarf_gpr_r12,   LLDB_INVALID_REGNUM,       gdb_gpr_r12),
    DEFINE_GPR(r13,    NULL,    gcc_dwarf_gpr_r13,   gcc_dwarf_gpr_r13,   LLDB_INVALID_REGNUM,       gdb_gpr_r13),
    DEFINE_GPR(r14,    NULL,    gcc_dwarf_gpr_r14,   gcc_dwarf_gpr_r14,   LLDB_INVALID_REGNUM,       gdb_gpr_r14),
    DEFINE_GPR(r15,    NULL,    gcc_dwarf_gpr_r15,   gcc_dwarf_gpr_r15,   LLDB_INVALID_REGNUM,       gdb_gpr_r15),
    DEFINE_GPR(rip,    "pc",    gcc_dwarf_gpr_rip,   gcc_dwarf_gpr_rip,   LLDB_REGNUM_GENERIC_PC,    gdb_gpr_rip),
    DEFINE_GPR(rflags, "flags", LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_REGNUM_GENERIC_FLAGS, gdb_gpr_rflags),
    DEFINE_GPR(cs,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_cs),
    DEFINE_GPR(fs,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_fs),
    DEFINE_GPR(gs,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_gs),
    DEFINE_GPR(ss,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_ss),
    DEFINE_GPR(ds,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_ds),
    DEFINE_GPR(es,     NULL,    LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,       gdb_gpr_es),
    // i386 registers
    DEFINE_GPR_i386(eax,    rax,    NULL,    gcc_eax,    dwarf_eax,    LLDB_INVALID_REGNUM,       gdb_eax),
    DEFINE_GPR_i386(ebx,    rbx,    NULL,    gcc_ebx,    dwarf_ebx,    LLDB_INVALID_REGNUM,       gdb_ebx),
    DEFINE_GPR_i386(ecx,    rcx,    NULL,    gcc_ecx,    dwarf_ecx,    LLDB_INVALID_REGNUM,       gdb_ecx),
    DEFINE_GPR_i386(edx,    rdx,    NULL,    gcc_edx,    dwarf_edx,    LLDB_INVALID_REGNUM,       gdb_edx),
    DEFINE_GPR_i386(edi,    rdi,    NULL,    gcc_edi,    dwarf_edi,    LLDB_INVALID_REGNUM,       gdb_edi),
    DEFINE_GPR_i386(esi,    rsi,    NULL,    gcc_esi,    dwarf_esi,    LLDB_INVALID_REGNUM,       gdb_esi),
    DEFINE_GPR_i386(ebp,    rbp,    "fp",    gcc_ebp,    dwarf_ebp,    LLDB_REGNUM_GENERIC_FP,    gdb_ebp),
    DEFINE_GPR_i386(esp,    rsp,    "sp",    gcc_esp,    dwarf_esp,    LLDB_REGNUM_GENERIC_SP,    gdb_esp),
    DEFINE_GPR_i386(eip,    rip,    "pc",    gcc_eip,    dwarf_eip,    LLDB_REGNUM_GENERIC_PC,    gdb_eip),
    DEFINE_GPR_i386(eflags, rflags, "flags", gcc_eflags, dwarf_eflags, LLDB_REGNUM_GENERIC_FLAGS, gdb_eflags),
    // i387 Floating point registers.
    DEFINE_FPR(fcw,       LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_fcw),
    DEFINE_FPR(fsw,       LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_fsw),
    DEFINE_FPR(ftw,       LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_ftw),
    DEFINE_FPR(fop,       LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_fop),
    DEFINE_FPR(ip,        LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_ip),
    // FIXME: Extract segment from ip.
    DEFINE_FPR(ip,        LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_cs_64),
    DEFINE_FPR(dp,        LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_dp),
    // FIXME: Extract segment from dp.
    DEFINE_FPR(dp,        LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_ds_64),
    DEFINE_FPR(mxcsr,     LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fpu_mxcsr),
    DEFINE_FPR(mxcsrmask, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),

    // FP registers.
    DEFINE_FP(stmm, 0),
    DEFINE_FP(stmm, 1),
    DEFINE_FP(stmm, 2),
    DEFINE_FP(stmm, 3),
    DEFINE_FP(stmm, 4),
    DEFINE_FP(stmm, 5),
    DEFINE_FP(stmm, 6),
    DEFINE_FP(stmm, 7),

    // XMM registers
    DEFINE_XMM(xmm, 0),
    DEFINE_XMM(xmm, 1),
    DEFINE_XMM(xmm, 2),
    DEFINE_XMM(xmm, 3),
    DEFINE_XMM(xmm, 4),
    DEFINE_XMM(xmm, 5),
    DEFINE_XMM(xmm, 6),
    DEFINE_XMM(xmm, 7),
    DEFINE_XMM(xmm, 8),
    DEFINE_XMM(xmm, 9),
    DEFINE_XMM(xmm, 10),
    DEFINE_XMM(xmm, 11),
    DEFINE_XMM(xmm, 12),
    DEFINE_XMM(xmm, 13),
    DEFINE_XMM(xmm, 14),
    DEFINE_XMM(xmm, 15),

    // Copy of YMM registers assembled from xmm and ymmh
    DEFINE_YMM(ymm, 0),
    DEFINE_YMM(ymm, 1),
    DEFINE_YMM(ymm, 2),
    DEFINE_YMM(ymm, 3),
    DEFINE_YMM(ymm, 4),
    DEFINE_YMM(ymm, 5),
    DEFINE_YMM(ymm, 6),
    DEFINE_YMM(ymm, 7),
    DEFINE_YMM(ymm, 8),
    DEFINE_YMM(ymm, 9),
    DEFINE_YMM(ymm, 10),
    DEFINE_YMM(ymm, 11),
    DEFINE_YMM(ymm, 12),
    DEFINE_YMM(ymm, 13),
    DEFINE_YMM(ymm, 14),
    DEFINE_YMM(ymm, 15),

    // Debug registers for lldb internal use
    DEFINE_DR(dr, 0),
    DEFINE_DR(dr, 1),
    DEFINE_DR(dr, 2),
    DEFINE_DR(dr, 3),
    DEFINE_DR(dr, 4),
    DEFINE_DR(dr, 5),
    DEFINE_DR(dr, 6),
    DEFINE_DR(dr, 7)
};

bool RegisterContextPOSIX_x86_64::IsGPR(unsigned reg)
{
    return reg <= k_last_gpr;   // GPR's come first.
}

bool RegisterContextPOSIX_x86_64::IsAVX(unsigned reg)
{
    return (k_first_avx <= reg && reg <= k_last_avx);
}

bool RegisterContextPOSIX_x86_64::IsFPR(unsigned reg)
{
    return (k_first_fpr <= reg && reg <= k_last_fpr);
}

bool RegisterContextPOSIX_x86_64::IsFPR(unsigned reg, FPRType fpr_type)
{
    bool generic_fpr = IsFPR(reg);
    if (fpr_type == eXSAVE)
      return generic_fpr || IsAVX(reg);

    return generic_fpr;
}

RegisterContextPOSIX_x86_64::RegisterContextPOSIX_x86_64(Thread &thread,
                                               uint32_t concrete_frame_idx,
                                               RegisterInfoInterface *register_info)
    : RegisterContext(thread, concrete_frame_idx)
{
    m_register_info_ap.reset(register_info);

    // Initialize m_iovec to point to the buffer and buffer size
    // using the conventions of Berkeley style UIO structures, as required
    // by PTRACE extensions.
    m_iovec.iov_base = &m_fpr.xstate.xsave;
    m_iovec.iov_len = sizeof(m_fpr.xstate.xsave);

    ::memset(&m_fpr, 0, sizeof(RegisterContextPOSIX_x86_64::FPR));

    // elf-core yet to support ReadFPR()
    ProcessSP base = CalculateProcess();
    if (base.get()->GetPluginName() ==  ProcessElfCore::GetPluginNameStatic())
        return;
    
    m_fpr_type = eNotValid;
}

RegisterContextPOSIX_x86_64::~RegisterContextPOSIX_x86_64()
{
}

RegisterContextPOSIX_x86_64::FPRType RegisterContextPOSIX_x86_64::GetFPRType()
{
    if (m_fpr_type == eNotValid)
    {
        // TODO: Use assembly to call cpuid on the inferior and query ebx or ecx
        m_fpr_type = eXSAVE; // extended floating-point registers, if available
        if (false == ReadFPR())
            m_fpr_type = eFXSAVE; // assume generic floating-point registers
    }
    return m_fpr_type;
}

void
RegisterContextPOSIX_x86_64::Invalidate()
{
}

void
RegisterContextPOSIX_x86_64::InvalidateAllRegisters()
{
}

unsigned
RegisterContextPOSIX_x86_64::GetRegisterOffset(unsigned reg)
{
    assert(reg < k_num_registers && "Invalid register number.");
    return GetRegisterInfo()[reg].byte_offset;
}

unsigned
RegisterContextPOSIX_x86_64::GetRegisterSize(unsigned reg)
{
    assert(reg < k_num_registers && "Invalid register number.");
    return GetRegisterInfo()[reg].byte_size;
}

size_t
RegisterContextPOSIX_x86_64::GetRegisterCount()
{
    size_t num_registers = k_num_gpr_registers + k_num_fpr_registers;
    if (GetFPRType() == eXSAVE)
      return num_registers + k_num_avx_registers;
    return num_registers;
}

size_t
RegisterContextPOSIX_x86_64::GetGPRSize()
{
    return m_register_info_ap->GetGPRSize();
}

const RegisterInfo *
RegisterContextPOSIX_x86_64::GetRegisterInfo()
{
    // Commonly, this method is overridden and g_register_infos is copied and specialized.
    // So, use GetRegisterInfo() rather than g_register_infos in this scope.
    return m_register_info_ap->GetRegisterInfo(g_register_infos);
}

const RegisterInfo *
RegisterContextPOSIX_x86_64::GetRegisterInfoAtIndex(size_t reg)
{
    if (reg < k_num_registers)
        return &GetRegisterInfo()[reg];
    else
        return NULL;
}

size_t
RegisterContextPOSIX_x86_64::GetRegisterSetCount()
{
    size_t sets = 0;
    for (size_t set = 0; set < k_num_register_sets; ++set)
        if (IsRegisterSetAvailable(set))
            ++sets;

    return sets;
}

const RegisterSet *
RegisterContextPOSIX_x86_64::GetRegisterSet(size_t set)
{
    if (IsRegisterSetAvailable(set))
        return &g_reg_sets[set];
    else
        return NULL;
}

const char *
RegisterContextPOSIX_x86_64::GetRegisterName(unsigned reg)
{
    assert(reg < k_num_registers && "Invalid register offset.");
    return GetRegisterInfo()[reg].name;
}

lldb::ByteOrder
RegisterContextPOSIX_x86_64::GetByteOrder()
{
    // Get the target process whose privileged thread was used for the register read.
    lldb::ByteOrder byte_order = eByteOrderInvalid;
    Process *process = CalculateProcess().get();

    if (process)
        byte_order = process->GetByteOrder();
    return byte_order;
}

// Parse ymm registers and into xmm.bytes and ymmh.bytes.
bool RegisterContextPOSIX_x86_64::CopyYMMtoXSTATE(uint32_t reg, lldb::ByteOrder byte_order)
{
    if (!IsAVX(reg))
        return false;

    if (byte_order == eByteOrderLittle)
    {
      ::memcpy(m_fpr.xstate.fxsave.xmm[reg - fpu_ymm0].bytes,
               m_ymm_set.ymm[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::XMMReg));
      ::memcpy(m_fpr.xstate.xsave.ymmh[reg - fpu_ymm0].bytes,
               m_ymm_set.ymm[reg - fpu_ymm0].bytes + sizeof(RegisterContextPOSIX_x86_64::XMMReg),
               sizeof(RegisterContextPOSIX_x86_64::YMMHReg));
      return true;
    }

    if (byte_order == eByteOrderBig)
    {
      ::memcpy(m_fpr.xstate.fxsave.xmm[reg - fpu_ymm0].bytes,
               m_ymm_set.ymm[reg - fpu_ymm0].bytes + sizeof(RegisterContextPOSIX_x86_64::XMMReg),
               sizeof(RegisterContextPOSIX_x86_64::XMMReg));
      ::memcpy(m_fpr.xstate.xsave.ymmh[reg - fpu_ymm0].bytes,
               m_ymm_set.ymm[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::YMMHReg));
      return true;
    }
    return false; // unsupported or invalid byte order
}

// Concatenate xmm.bytes with ymmh.bytes
bool RegisterContextPOSIX_x86_64::CopyXSTATEtoYMM(uint32_t reg, lldb::ByteOrder byte_order)
{
    if (!IsAVX(reg))
        return false;

    if (byte_order == eByteOrderLittle)
    {
      ::memcpy(m_ymm_set.ymm[reg - fpu_ymm0].bytes,
               m_fpr.xstate.fxsave.xmm[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::XMMReg));
      ::memcpy(m_ymm_set.ymm[reg - fpu_ymm0].bytes + sizeof(RegisterContextPOSIX_x86_64::XMMReg),
               m_fpr.xstate.xsave.ymmh[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::YMMHReg));
      return true;
    }
    if (byte_order == eByteOrderBig)
    {
      ::memcpy(m_ymm_set.ymm[reg - fpu_ymm0].bytes + sizeof(RegisterContextPOSIX_x86_64::XMMReg),
               m_fpr.xstate.fxsave.xmm[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::XMMReg));
      ::memcpy(m_ymm_set.ymm[reg - fpu_ymm0].bytes,
               m_fpr.xstate.xsave.ymmh[reg - fpu_ymm0].bytes,
               sizeof(RegisterContextPOSIX_x86_64::YMMHReg));
      return true;
    }
    return false; // unsupported or invalid byte order
}

bool
RegisterContextPOSIX_x86_64::IsRegisterSetAvailable(size_t set_index)
{
    // Note: Extended register sets are assumed to be at the end of g_reg_sets...
    size_t num_sets = k_num_register_sets - k_num_extended_register_sets;
    if (GetFPRType() == eXSAVE) // ...and to start with AVX registers.
        ++num_sets;

    return (set_index < num_sets);
}   

uint32_t
RegisterContextPOSIX_x86_64::ConvertRegisterKindToRegisterNumber(uint32_t kind,
                                                                 uint32_t num)
{
    const Process *process = CalculateProcess().get();
    if (process)
    {
        const ArchSpec arch = process->GetTarget().GetArchitecture();;
        switch (arch.GetCore())
        {
        default:
            assert(false && "CPU type not supported!");
            break;

        case ArchSpec::eCore_x86_32_i386:
        case ArchSpec::eCore_x86_32_i486:
        case ArchSpec::eCore_x86_32_i486sx:
        {
            if (kind == eRegisterKindGeneric)
            {
                switch (num)
                {
                case LLDB_REGNUM_GENERIC_PC:    return gpr_eip;
                case LLDB_REGNUM_GENERIC_SP:    return gpr_esp;
                case LLDB_REGNUM_GENERIC_FP:    return gpr_ebp;
                case LLDB_REGNUM_GENERIC_FLAGS: return gpr_eflags;
                case LLDB_REGNUM_GENERIC_RA:
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }

            if (kind == eRegisterKindGCC || kind == eRegisterKindDWARF)
            {
                switch (num)
                {
                case dwarf_eax:  return gpr_eax;
                case dwarf_edx:  return gpr_edx;
                case dwarf_ecx:  return gpr_ecx;
                case dwarf_ebx:  return gpr_ebx;
                case dwarf_esi:  return gpr_esi;
                case dwarf_edi:  return gpr_edi;
                case dwarf_ebp:  return gpr_ebp;
                case dwarf_esp:  return gpr_esp;
                case dwarf_eip:  return gpr_eip;
                case dwarf_xmm0: return fpu_xmm0;
                case dwarf_xmm1: return fpu_xmm1;
                case dwarf_xmm2: return fpu_xmm2;
                case dwarf_xmm3: return fpu_xmm3;
                case dwarf_xmm4: return fpu_xmm4;
                case dwarf_xmm5: return fpu_xmm5;
                case dwarf_xmm6: return fpu_xmm6;
                case dwarf_xmm7: return fpu_xmm7;
                case dwarf_stmm0: return fpu_stmm0;
                case dwarf_stmm1: return fpu_stmm1;
                case dwarf_stmm2: return fpu_stmm2;
                case dwarf_stmm3: return fpu_stmm3;
                case dwarf_stmm4: return fpu_stmm4;
                case dwarf_stmm5: return fpu_stmm5;
                case dwarf_stmm6: return fpu_stmm6;
                case dwarf_stmm7: return fpu_stmm7;
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }

            if (kind == eRegisterKindGDB)
            {
                switch (num)
                {
                case gdb_eax     : return gpr_eax;
                case gdb_ebx     : return gpr_ebx;
                case gdb_ecx     : return gpr_ecx;
                case gdb_edx     : return gpr_edx;
                case gdb_esi     : return gpr_esi;
                case gdb_edi     : return gpr_edi;
                case gdb_ebp     : return gpr_ebp;
                case gdb_esp     : return gpr_esp;
                case gdb_eip     : return gpr_eip;
                case gdb_eflags  : return gpr_eflags;
                case gdb_cs      : return gpr_cs;
                case gdb_ss      : return gpr_ss;
                case gdb_ds      : return gpr_ds;
                case gdb_es      : return gpr_es;
                case gdb_fs      : return gpr_fs;
                case gdb_gs      : return gpr_gs;
                case gdb_stmm0   : return fpu_stmm0;
                case gdb_stmm1   : return fpu_stmm1;
                case gdb_stmm2   : return fpu_stmm2;
                case gdb_stmm3   : return fpu_stmm3;
                case gdb_stmm4   : return fpu_stmm4;
                case gdb_stmm5   : return fpu_stmm5;
                case gdb_stmm6   : return fpu_stmm6;
                case gdb_stmm7   : return fpu_stmm7;
                case gdb_fcw     : return fpu_fcw;
                case gdb_fsw     : return fpu_fsw;
                case gdb_ftw     : return fpu_ftw;
                case gdb_fpu_cs  : return fpu_cs;
                case gdb_ip      : return fpu_ip;
                case gdb_fpu_ds  : return fpu_ds; //fpu_fos
                case gdb_dp      : return fpu_dp; //fpu_foo
                case gdb_fop     : return fpu_fop;
                case gdb_xmm0    : return fpu_xmm0;
                case gdb_xmm1    : return fpu_xmm1;
                case gdb_xmm2    : return fpu_xmm2;
                case gdb_xmm3    : return fpu_xmm3;
                case gdb_xmm4    : return fpu_xmm4;
                case gdb_xmm5    : return fpu_xmm5;
                case gdb_xmm6    : return fpu_xmm6;
                case gdb_xmm7    : return fpu_xmm7;
                case gdb_mxcsr   : return fpu_mxcsr;
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }
            else if (kind == eRegisterKindLLDB)
            {
                return num;
            }

            break;
        }

        case ArchSpec::eCore_x86_64_x86_64:
        {
            if (kind == eRegisterKindGeneric)
            {
                switch (num)
                {
                case LLDB_REGNUM_GENERIC_PC:    return gpr_rip;
                case LLDB_REGNUM_GENERIC_SP:    return gpr_rsp;
                case LLDB_REGNUM_GENERIC_FP:    return gpr_rbp;
                case LLDB_REGNUM_GENERIC_FLAGS: return gpr_rflags;
                case LLDB_REGNUM_GENERIC_RA:
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }

            if (kind == eRegisterKindGCC || kind == eRegisterKindDWARF)
            {
                switch (num)
                {
                case gcc_dwarf_gpr_rax:  return gpr_rax;
                case gcc_dwarf_gpr_rdx:  return gpr_rdx;
                case gcc_dwarf_gpr_rcx:  return gpr_rcx;
                case gcc_dwarf_gpr_rbx:  return gpr_rbx;
                case gcc_dwarf_gpr_rsi:  return gpr_rsi;
                case gcc_dwarf_gpr_rdi:  return gpr_rdi;
                case gcc_dwarf_gpr_rbp:  return gpr_rbp;
                case gcc_dwarf_gpr_rsp:  return gpr_rsp;
                case gcc_dwarf_gpr_r8:   return gpr_r8;
                case gcc_dwarf_gpr_r9:   return gpr_r9;
                case gcc_dwarf_gpr_r10:  return gpr_r10;
                case gcc_dwarf_gpr_r11:  return gpr_r11;
                case gcc_dwarf_gpr_r12:  return gpr_r12;
                case gcc_dwarf_gpr_r13:  return gpr_r13;
                case gcc_dwarf_gpr_r14:  return gpr_r14;
                case gcc_dwarf_gpr_r15:  return gpr_r15;
                case gcc_dwarf_gpr_rip:  return gpr_rip;
                case gcc_dwarf_fpu_xmm0: return fpu_xmm0;
                case gcc_dwarf_fpu_xmm1: return fpu_xmm1;
                case gcc_dwarf_fpu_xmm2: return fpu_xmm2;
                case gcc_dwarf_fpu_xmm3: return fpu_xmm3;
                case gcc_dwarf_fpu_xmm4: return fpu_xmm4;
                case gcc_dwarf_fpu_xmm5: return fpu_xmm5;
                case gcc_dwarf_fpu_xmm6: return fpu_xmm6;
                case gcc_dwarf_fpu_xmm7: return fpu_xmm7;
                case gcc_dwarf_fpu_xmm8: return fpu_xmm8;
                case gcc_dwarf_fpu_xmm9: return fpu_xmm9;
                case gcc_dwarf_fpu_xmm10: return fpu_xmm10;
                case gcc_dwarf_fpu_xmm11: return fpu_xmm11;
                case gcc_dwarf_fpu_xmm12: return fpu_xmm12;
                case gcc_dwarf_fpu_xmm13: return fpu_xmm13;
                case gcc_dwarf_fpu_xmm14: return fpu_xmm14;
                case gcc_dwarf_fpu_xmm15: return fpu_xmm15;
                case gcc_dwarf_fpu_stmm0: return fpu_stmm0;
                case gcc_dwarf_fpu_stmm1: return fpu_stmm1;
                case gcc_dwarf_fpu_stmm2: return fpu_stmm2;
                case gcc_dwarf_fpu_stmm3: return fpu_stmm3;
                case gcc_dwarf_fpu_stmm4: return fpu_stmm4;
                case gcc_dwarf_fpu_stmm5: return fpu_stmm5;
                case gcc_dwarf_fpu_stmm6: return fpu_stmm6;
                case gcc_dwarf_fpu_stmm7: return fpu_stmm7;
                case gcc_dwarf_fpu_ymm0: return fpu_ymm0;
                case gcc_dwarf_fpu_ymm1: return fpu_ymm1;
                case gcc_dwarf_fpu_ymm2: return fpu_ymm2;
                case gcc_dwarf_fpu_ymm3: return fpu_ymm3;
                case gcc_dwarf_fpu_ymm4: return fpu_ymm4;
                case gcc_dwarf_fpu_ymm5: return fpu_ymm5;
                case gcc_dwarf_fpu_ymm6: return fpu_ymm6;
                case gcc_dwarf_fpu_ymm7: return fpu_ymm7;
                case gcc_dwarf_fpu_ymm8: return fpu_ymm8;
                case gcc_dwarf_fpu_ymm9: return fpu_ymm9;
                case gcc_dwarf_fpu_ymm10: return fpu_ymm10;
                case gcc_dwarf_fpu_ymm11: return fpu_ymm11;
                case gcc_dwarf_fpu_ymm12: return fpu_ymm12;
                case gcc_dwarf_fpu_ymm13: return fpu_ymm13;
                case gcc_dwarf_fpu_ymm14: return fpu_ymm14;
                case gcc_dwarf_fpu_ymm15: return fpu_ymm15;
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }

            if (kind == eRegisterKindGDB)
            {
                switch (num)
                {
                case gdb_gpr_rax     : return gpr_rax;
                case gdb_gpr_rbx     : return gpr_rbx;
                case gdb_gpr_rcx     : return gpr_rcx;
                case gdb_gpr_rdx     : return gpr_rdx;
                case gdb_gpr_rsi     : return gpr_rsi;
                case gdb_gpr_rdi     : return gpr_rdi;
                case gdb_gpr_rbp     : return gpr_rbp;
                case gdb_gpr_rsp     : return gpr_rsp;
                case gdb_gpr_r8      : return gpr_r8;
                case gdb_gpr_r9      : return gpr_r9;
                case gdb_gpr_r10     : return gpr_r10;
                case gdb_gpr_r11     : return gpr_r11;
                case gdb_gpr_r12     : return gpr_r12;
                case gdb_gpr_r13     : return gpr_r13;
                case gdb_gpr_r14     : return gpr_r14;
                case gdb_gpr_r15     : return gpr_r15;
                case gdb_gpr_rip     : return gpr_rip;
                case gdb_gpr_rflags  : return gpr_rflags;
                case gdb_gpr_cs      : return gpr_cs;
                case gdb_gpr_ss      : return gpr_ss;
                case gdb_gpr_ds      : return gpr_ds;
                case gdb_gpr_es      : return gpr_es;
                case gdb_gpr_fs      : return gpr_fs;
                case gdb_gpr_gs      : return gpr_gs;
                case gdb_fpu_stmm0   : return fpu_stmm0;
                case gdb_fpu_stmm1   : return fpu_stmm1;
                case gdb_fpu_stmm2   : return fpu_stmm2;
                case gdb_fpu_stmm3   : return fpu_stmm3;
                case gdb_fpu_stmm4   : return fpu_stmm4;
                case gdb_fpu_stmm5   : return fpu_stmm5;
                case gdb_fpu_stmm6   : return fpu_stmm6;
                case gdb_fpu_stmm7   : return fpu_stmm7;
                case gdb_fpu_fcw     : return fpu_fcw;
                case gdb_fpu_fsw     : return fpu_fsw;
                case gdb_fpu_ftw     : return fpu_ftw;
                case gdb_fpu_cs_64   : return fpu_cs;
                case gdb_fpu_ip      : return fpu_ip;
                case gdb_fpu_ds_64   : return fpu_ds;
                case gdb_fpu_dp      : return fpu_dp;
                case gdb_fpu_fop     : return fpu_fop;
                case gdb_fpu_xmm0    : return fpu_xmm0;
                case gdb_fpu_xmm1    : return fpu_xmm1;
                case gdb_fpu_xmm2    : return fpu_xmm2;
                case gdb_fpu_xmm3    : return fpu_xmm3;
                case gdb_fpu_xmm4    : return fpu_xmm4;
                case gdb_fpu_xmm5    : return fpu_xmm5;
                case gdb_fpu_xmm6    : return fpu_xmm6;
                case gdb_fpu_xmm7    : return fpu_xmm7;
                case gdb_fpu_xmm8    : return fpu_xmm8;
                case gdb_fpu_xmm9    : return fpu_xmm9;
                case gdb_fpu_xmm10   : return fpu_xmm10;
                case gdb_fpu_xmm11   : return fpu_xmm11;
                case gdb_fpu_xmm12   : return fpu_xmm12;
                case gdb_fpu_xmm13   : return fpu_xmm13;
                case gdb_fpu_xmm14   : return fpu_xmm14;
                case gdb_fpu_xmm15   : return fpu_xmm15;
                case gdb_fpu_mxcsr   : return fpu_mxcsr;
                case gdb_fpu_ymm0    : return fpu_ymm0;
                case gdb_fpu_ymm1    : return fpu_ymm1;
                case gdb_fpu_ymm2    : return fpu_ymm2;
                case gdb_fpu_ymm3    : return fpu_ymm3;
                case gdb_fpu_ymm4    : return fpu_ymm4;
                case gdb_fpu_ymm5    : return fpu_ymm5;
                case gdb_fpu_ymm6    : return fpu_ymm6;
                case gdb_fpu_ymm7    : return fpu_ymm7;
                case gdb_fpu_ymm8    : return fpu_ymm8;
                case gdb_fpu_ymm9    : return fpu_ymm9;
                case gdb_fpu_ymm10   : return fpu_ymm10;
                case gdb_fpu_ymm11   : return fpu_ymm11;
                case gdb_fpu_ymm12   : return fpu_ymm12;
                case gdb_fpu_ymm13   : return fpu_ymm13;
                case gdb_fpu_ymm14   : return fpu_ymm14;
                case gdb_fpu_ymm15   : return fpu_ymm15;
                default:
                    return LLDB_INVALID_REGNUM;
                }
            }
            else if (kind == eRegisterKindLLDB)
            {
                return num;
            }
        }
        }
    }

    return LLDB_INVALID_REGNUM;
}

