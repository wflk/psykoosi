//===-- llvm/MC/MCInst.h - MCInst class -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the MCInst and MCOperand classes, which
// is the basic representation used to represent low-level machine code
// instructions.
//
//===----------------------------------------------------------------------===//

/* Capstone Disassembly Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2014 */

#ifndef CS_MCINST_H
#define CS_MCINST_H

#include <stdint.h>

#include "include/capstone.h"

typedef struct MCInst MCInst;
typedef struct cs_struct cs_struct;
typedef struct MCOperand MCOperand;

/// MCOperand - Instances of this class represent operands of the MCInst class.
/// This is a simple discriminated union.
struct MCOperand {
	enum {
		kInvalid = 0,                 ///< Uninitialized.
		kRegister,                ///< Register operand.
		kImmediate,               ///< Immediate operand.
		kFPImmediate,             ///< Floating-point immediate operand.
	} MachineOperandType;
	unsigned char Kind;

	union {
		unsigned RegVal;
		int64_t ImmVal;
		double FPImmVal;
	};
};

bool MCOperand_isValid(const MCOperand *op);

bool MCOperand_isReg(const MCOperand *op);

bool MCOperand_isImm(const MCOperand *op);

bool MCOperand_isFPImm(const MCOperand *op);

bool MCOperand_isInst(const MCOperand *op);

void MCInst_clear(MCInst *m);

/// getReg - Returns the register number.
unsigned MCOperand_getReg(const MCOperand *op);

/// setReg - Set the register number.
void MCOperand_setReg(MCOperand *op, unsigned Reg);

int64_t MCOperand_getImm(MCOperand *op);

void MCOperand_setImm(MCOperand *op, int64_t Val);

double MCOperand_getFPImm(const MCOperand *op);

void MCOperand_setFPImm(MCOperand *op, double Val);

const MCInst *MCOperand_getInst(const MCOperand *op);

void MCOperand_setInst(MCOperand *op, const MCInst *Val);

// create Reg operand in the next slot
void MCOperand_CreateReg0(MCInst *inst, unsigned Reg);

// create Reg operand use the last-unused slot
MCOperand *MCOperand_CreateReg1(MCInst *inst, unsigned Reg);

// create Imm operand in the next slot
void MCOperand_CreateImm0(MCInst *inst, int64_t Val);

// create Imm operand in the last-unused slot
MCOperand *MCOperand_CreateImm1(MCInst *inst, int64_t Val);

/// MCInst - Instances of this class represent a single low-level machine
/// instruction.
struct MCInst {
	unsigned Opcode;
	MCOperand Operands[48];
	unsigned OpcodePub;
	unsigned size;	// number of operands
	cs_insn *flat_insn;	// insn to be exposed to public
	uint64_t address;	// address of this insn
	cs_struct *csh;	// save the main csh
	uint8_t x86_imm_size;	// save immediate size to print immediate properly

	// (Optional) instruction prefix, which can be up to 5 bytes.
	// A prefix byte gets value 0 when irrelevant.
	// This is copied from cs_x86 struct
	uint8_t x86_prefix[4];
};

void MCInst_Init(MCInst *inst);

void MCInst_clear(MCInst *inst);

// do not free operand after inserting
void MCInst_insert0(MCInst *inst, int index, MCOperand *Op);

void MCInst_setOpcode(MCInst *inst, unsigned Op);

unsigned MCInst_getOpcode(const MCInst*);

void MCInst_setOpcodePub(MCInst *inst, unsigned Op);

unsigned MCInst_getOpcodePub(const MCInst*);

MCOperand *MCInst_getOperand(MCInst *inst, unsigned i);

unsigned MCInst_getNumOperands(const MCInst *inst);

// This addOperand2 function doesnt free Op
void MCInst_addOperand2(MCInst *inst, MCOperand *Op);

#endif
