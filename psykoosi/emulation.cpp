/*
 * emulation.cpp
 *
 * This will contain code for emulating machine code for various purposes.  We can use it for
 * verification of our own engine.  We can use it later for finding similar code for obfuscation..etc.
 *
 * I will start with using XEN's x86_emulate() code... this will have to be rewrote later if used
 * commercially.
 *
 *  Created on: Aug 11, 2014
 *      Author: mike guidry
 */


#include <cstddef>
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <fstream>
#include <string>
#include <inttypes.h>
#include <udis86.h>
#include <pe_lib/pe_bliss.h>
#include "virtualmemory.h"
extern "C" {
#include <capstone/capstone.h>
#include "xen/x86_emulate.h"
}
#include "disassemble.h"
#include "analysis.h"
#include "loading.h"
#include "apiproxy_client.h"
#include "emu_hooks.h"
#include "emulation.h"

using namespace psykoosi;
using namespace pe_bliss;
using namespace pe_win;


VirtualMemory *_VM2[MAX_VMS];
Emulation *EmuPtr[MAX_VMS];
Emulation::EmulationThread *EmuThread[MAX_VMS];
BinaryLoader *_BL[MAX_VMS];

static int address_from_seg_offset(enum x86_segment seg, unsigned long offset, struct _x86_emulate_ctxt *ctxt) {
	struct _x86_thread *thread = (struct _x86_thread *)ctxt;
	Emulation *VirtPtr = EmuPtr[thread->ID];
	VirtualMemory *pVM = _VM2[thread->ID];
	Emulation::EmulationThread *emuthread = EmuThread[thread->ID];

	unsigned long _seg = 0;
	uint32_t result = 0;
/*
 * x86_seg_cs,
    x86_seg_ss,
    x86_seg_ds,
    x86_seg_es,
    x86_seg_fs,
    x86_seg_gs,

 */
	switch (seg) {
		case x86_seg_cs:
			_seg = emuthread->registers.cs;
			break;
		case x86_seg_ss:
			_seg = emuthread->registers.ss;
			break;
		case x86_seg_ds:
			_seg = emuthread->registers.ds;
			break;
		case x86_seg_es:
			_seg = emuthread->registers.es;
			break;
		case x86_seg_fs:
			_seg = emuthread->registers.fs;
			break;
		case x86_seg_gs:
			_seg = emuthread->registers.gs;
			break;
		default:
			break;
	}
	result = _seg + offset;

	return result;
}


static int emulated_rep_movs(enum x86_segment src_seg,unsigned long src_offset,enum x86_segment dst_seg, unsigned long dst_offset,unsigned int bytes_per_rep,unsigned long *reps,struct _x86_emulate_ctxt *ctxt) {
	struct _x86_thread *thread = (struct _x86_thread *)ctxt;
	Emulation *VirtPtr = EmuPtr[thread->ID];
	VirtualMemory *pVM = _VM2[thread->ID];
	unsigned long bytes_to_copy = *reps * bytes_per_rep;

    //printf("vm %p rep movs src seg %d offset %x dst seg %d offset %d bytes per %d reps %d id %d ctxt %p\n", _VM2[0],src_seg, src_offset, dst_seg, dst_offset, bytes_per_rep, *reps,  ctxt);

    unsigned char *data = new unsigned char [bytes_to_copy];

	pVM->MemDataRead(address_from_seg_offset(src_seg,src_offset,ctxt), (unsigned char *) data, bytes_to_copy);
	pVM->MemDataWrite(address_from_seg_offset(dst_seg, dst_offset,ctxt), (unsigned char *)data, bytes_to_copy);

	delete data;

	return X86EMUL_OKAY;
}



static int emulated_write(enum x86_segment seg, unsigned long offset, void *p_data, unsigned int bytes, struct _x86_emulate_ctxt *ctxt) {
	struct _x86_thread *thread = (struct _x86_thread *)ctxt;
	Emulation *VirtPtr = EmuPtr[thread->ID];
	VirtualMemory *pVM = _VM2[thread->ID];

	printf("vm %p write seg %d offset %X data %p bytes %d ctxt %p\n", _VM2[0], seg, offset, p_data, bytes, ctxt);

	pVM->MemDataWrite(address_from_seg_offset(seg,offset,ctxt),(unsigned char *) p_data, bytes);

    return X86EMUL_OKAY;
}



static int emulated_cmpxchg(enum x86_segment seg,unsigned long offset,void *p_old,void *p_new,unsigned int bytes,
    struct _x86_emulate_ctxt *ctxt) {
	struct _x86_thread *thread = (struct _x86_thread *)ctxt;
	Emulation *VirtPtr = EmuPtr[thread->ID];
	VirtualMemory *pVM = _VM2[thread->ID];

	//printf("vm %p cmpxchg seg %d offset %x old %p new %p bytes %d ctxt %p\n", seg, offset, p_old, p_new, bytes, ctxt);

	pVM->MemDataWrite(address_from_seg_offset(seg,offset,ctxt),(unsigned char *) p_new, bytes);

	return X86EMUL_OKAY;
}


static int emulated_read_helper(enum x86_segment seg, unsigned long offset, void *p_data, unsigned int bytes, struct _x86_emulate_ctxt *ctxt, int fetch_insn) {
	struct _x86_thread *thread = (struct _x86_thread *)ctxt;
	Emulation *VirtPtr = EmuPtr[thread->ID];
	VirtualMemory *pVM = _VM2[thread->ID];
	BinaryLoader *Loader = _BL[0];

	if (Loader->Imports != NULL) {
		BinaryLoader::IAT *iatptr = Loader->Imports;
		while (iatptr != NULL) {
			if (iatptr->Address == offset) break;
			iatptr = iatptr->next;
		}
		if (iatptr != NULL) {
			memcpy((void *)p_data,(void *) &iatptr->Address, sizeof(uint32_t));
		}
	}

    printf("vm %p read seg %d offset %X data %X bytes %d ctxt %p id %d ptr %p\n", _VM2[0], seg, offset, p_data, bytes, ctxt,thread->ID, ctxt);
	pVM->MemDataRead(address_from_seg_offset(seg,offset,ctxt),(unsigned char *) p_data, bytes);

	return X86EMUL_OKAY;
}

static int emulated_read(enum x86_segment seg, unsigned long offset, void *p_data, unsigned int bytes, struct _x86_emulate_ctxt *ctxt) {
	return emulated_read_helper(seg, offset, p_data, bytes, ctxt, 0);
}
static int emulated_read_fetch(enum x86_segment seg, unsigned long offset, void *p_data, unsigned int bytes, struct _x86_emulate_ctxt *ctxt) {
	return emulated_read_helper(seg, offset, p_data, bytes, ctxt, 1);
}

// -- end of functions to support emulator (x86_emulate)



int Emulation::ConnectToProxy(APIClient *proxy) {
	Proxy = proxy;
	
	printf("EMU Proxy: %p\n", proxy);
	
	return proxy != NULL;
}

int Emulation::UsingProxy() {
	printf("UsingProxy: %d\n", Proxy != NULL);
	return Proxy != NULL;
}

int Emulation::SetupThreadStack(EmulationThread *tptr) {
	/*CodeAddr EBP = 0, ESP = 0;
	int Size = 1024 * 1024;
	uint32_t _ESP = 0x0012FD00 + Size;
	
	// check if we are using a real API proxy or not..
	if (!UsingProxy()) {
		// if not.. we put the stack 1 area higher than the last.. its free
		// stacks are generally 1 megabyte..
		if (MasterVM.StackList != NULL) {
			// put it the next 1 megabyte over the last stack..
			ESP = MasterVM.StackList->High + Size;
		} else {
			// loop until we have free space for our stack
			while (!ESP) {
				VirtualMemory::MemPage *pageptr = VM->MemPagePtrIfExists(_ESP);
				if (pageptr == NULL) {
					// ensure the bottom is empty too (1meg lower)
					pageptr = VM->MemPagePtrIfExists(_ESP - Size);
				}
				if (pageptr == NULL) {
					ESP = _ESP;
					break;
				} else {
					_ESP += Size;
				}
			}
		}
	} else {
		// we are using proxy.. so we must allocate locally as well as remotely...
		// and it should come from a bigger region for all threads/stack/heap/code
		
		// lets allocate 64 megabytes if we are using the API proxy...
		// .. tested with 16 locally.. but if we wanna do threaded apps
		// set these variable later for the apps...
		if (!Proxy->Regions) {
			uint32_t DoubleVerified = 0;
			// starting adadress
			uint32_t _RemoteCheck = 0x10000000 + (Size * 64); 
			while (!DoubleVerified) {
				// first we check with our local memory manager...
				uint32_t _Check = _RemoteCheck;
				uint32_t Verified = 0;
				while (!Verified) {
						VirtualMemory::MemPage *pageptr = VM->MemPagePtrIfExists(_ESP);
						if (pageptr == NULL) {
							// ensure the bottom is empty too (1meg lower)
							pageptr = VM->MemPagePtrIfExists(_ESP - (Size * 64));
						}
						if (pageptr == NULL) {
							Verified = _Check;
							break;
						} else {
							_Check += (Size * 64) + 0x00050000;
						}
				}
				// now that we have locally.. we check remotely.. by attempting to allocate
				int Remote_AllocateRet = Proxy->AllocateMemory((uint32_t)Verified, (int)(Size * 64));
				if (Remote_AllocateRet == 1) {
					DoubleVerified = Verified;
					break;
				} else {
					_RemoteCheck = Verified + (Size * 64) + 0x0005000;
				}
			}
			if (!DoubleVerified) {
				printf("Couldnt agree on memory region locally and remotely..\n");
				exit(-1);
			}
		} else {
			// we need to find space inside of the region already allocated on both ends...
			// we could just allocate a fake heap of 1 megabyte and use that for management...
			//APIClient::AllocatedRegion *regionptr = Proxy->Regions;
			
			uint32_t Stack = (uint32_t)HeapAlloc(0, 1024 * 1024);
			if (Stack == 0) {
				printf("couldnt find space for stack in local area\n");
				exit(-1);
			}
			
		}
	}
	*/
	int Size = 1024 * 1024;
	uint32_t ESP = 0, EBP = 0;
	
	ESP = (uint32_t)HeapAlloc(0,Size);

	printf("esp %X\n", ESP);
	if (ESP == 0) {
		printf("couldnt allocate memory for thread stack!\n");
		exit(-1);
		return -1;
	}
	
	// allocate tracking structure
	StackRegions *sptr = new StackRegions;
	if (sptr == NULL) return -1;
	memset((void *)sptr, 0, sizeof(StackRegions));
	
	// keep stack settings for later..
	sptr->High = ESP + Size;
	sptr->Low = ESP;
	sptr->Size = Size;
	MasterVM.StackHigh = ESP + Size;
	MasterVM.StackLow = ESP;
	
	// add to list
	sptr->next = MasterVM.StackList;
	MasterVM.StackList = sptr;
	
	// we should put a final return address on the stack .. *** FIX
	// put 0xDEADDEAD on top of the stack so we know when the program
	// is complete... (so we dont have to deal with conventions,etc)
	uint32_t end_addr = 0xDEADDEAD;
	char *data = (char *)&end_addr;
	uint32_t _where = sptr->High - 32;
	while (_where < sptr->High) {
		MasterVM.Memory->MemDataWrite(_where, (unsigned char *)data, 4);
		_where += sizeof(uint32_t);
	}
	
	ESP = sptr->High - 32;
	EBP = ESP;
	
	// set thread registers..
	SetRegister(tptr, REG_EBP, EBP);
	SetRegister(tptr, REG_ESP, ESP);

	// put to shadow for when we execute..
	CopyRegistersToShadow(tptr);
}

/*
init isnt in the constructor because we have to give the ability to use the API proxy
if we use the proxy.. then it changes the memory allocation strategies...
*/
uint32_t Emulation::Init(uint32_t ReqAddr) {
	int Size = 1024 * 1024 * 16;
	// lets allocate 16k below the images requested image base..
	// so our custom allocator has some room to play with..
	// + 1 meg for stack..
	//uint32_t Start = (ReqAddr) ? (ReqAddr - ((1024 * 1024) + (1024 * 16))) : 0x00401000;
	printf("Init %X\n", ReqAddr);
	uint32_t Start = (ReqAddr) ? ReqAddr : 0x00401000;
	uint32_t RemAddr = Start;
	if (!UsingProxy()) {
		MasterVM.RegionLow = Start;
		MasterVM.RegionHigh = MasterVM.HeapLow + Size + (1024 * 1024);
		
		MasterVM.HeapLow = MasterVM.RegionLow;
		MasterVM.HeapHigh = MasterVM.RegionHigh - (1024 * 1024);
	} else {
		int count = 0;
		int Verified = 0;
		
		while (!Verified) {
			RemAddr = Proxy->AllocateMemory((uint32_t)Start, Size);
			if (RemAddr != NULL) {
				VirtualMemory::Memory_Section *memptr = VM->Section_FindByAddrandFile(NULL, RemAddr);
				if (memptr == NULL) {
					memptr = VM->Section_FindByAddrandFile(NULL, RemAddr + Size);
				}
				if (memptr == NULL) {
					Verified = 1;
					printf("REMOTE Verified %p\n", RemAddr);
				}
			} else {
				if (ReqAddr != NULL) {
					printf("Couldnt allocate around requesting address %X\n", ReqAddr);
					return 0;
				}
				//printf("failed %X\n", RemAddr);
				Proxy->DeallocateMemory(RemAddr);
				Start += 0x00050000;
				if (count++ > 1000) {
					printf("1000 failures to allocate memory on both ends\n");
					exit(-1);
				} else if (count > 500 && Start < 0x20000000) {
					Start = 0x20000000;
					printf("500 failures.. starting at 0x2...\n"); 
				}
			}
		}
		
		printf("verified %p %d\n", RemAddr, Size);
		
		MasterVM.RegionLow = RemAddr;
		MasterVM.RegionHigh = RemAddr + Size;
		
		MasterVM.HeapLow = MasterVM.RegionLow;
		MasterVM.HeapHigh = MasterVM.RegionHigh - (1024 * 1024) - (1024 * 32);
		MasterVM.HeapLast = 0;
	}
	
	// add section so a new DLL wont overwrite it...
	VM->Add_Section(MasterVM.RegionLow, 1, MasterVM.RegionHigh - MasterVM.RegionLow, (VirtualMemory::SectionType)0, 0, 0, "REGION", (unsigned char *)"\x00");
	
	MasterThread = NewThread(&MasterVM);
	if (MasterThread == NULL) {
		printf("couldnt start thread\n");
		exit(-1);
	}
	EmuThread[0] = MasterThread;
	
	MasterThread->thread_ctx.ID = 0;
	MasterThread->thread_ctx.emulation_ctx.addr_size = 32;
	MasterThread->thread_ctx.emulation_ctx.sp_size = 32;
	MasterThread->thread_ctx.emulation_ctx.regs = &MasterThread->registers;

	// grabbed these from entry point on an app in IDA pro.. (after dlls+tls etc all loaded
	SetRegister(MasterThread, REG_EAX, 0);
	SetRegister(MasterThread, REG_EBX, 1);
	SetRegister(MasterThread, REG_ECX, 2);
	SetRegister(MasterThread, REG_EDX, 3);
	SetRegister(MasterThread, REG_ESI, 4);
	SetRegister(MasterThread, REG_EDI, 5);
	
	printf("end init()\n");
	//SetupThreadStack(MasterThread);
	return RemAddr;
}



Emulation::Emulation(VirtualMemory *_VM) {
	for (int i = 0; i < MAX_VMS; i++) {
		_VM2[i] = NULL;
		EmuPtr[i] = NULL;
		EmuThread[i] = NULL;
	}

	Proxy = NULL;
	VM = _VM2[0] = _VM;
	EmuPtr[0] = this;
	std::memset((void *)&MasterVM, 0, sizeof(VirtualMachine));
	//MasterVM.LogList = NULL;
	// count of virtual machines and incremental ID
	Current_VM_ID = 0;
	// current VM being executed...
	VM_Exec_ID = 0;

	// default settings for virtual memory logging
	Global_ChangeLog_Read = 0;
	Global_ChangeLog_Write = 0;
	Global_ChangeLog_Verify = 0;

	MasterVM.Memory = _VM;
	
	std::memset((void *)&MasterVM.emulate_ops, 0, sizeof(struct hack_x86_emulate_ops));
	
	MasterVM.emulate_ops.read = (void *)&emulated_read;
	MasterVM.emulate_ops.insn_fetch = (void *)&emulated_read_fetch;
	MasterVM.emulate_ops.write = (void *)&emulated_write;
	MasterVM.emulate_ops.rep_movs = (void *)&emulated_rep_movs;
	MasterVM.emulate_ops.cmpxchg = (void *)&emulated_cmpxchg;

}

Emulation::EmulationThread *Emulation::NewThread(Emulation::VirtualMachine *VM) {
	EmulationThread *tptr = new EmulationThread;
	if (tptr == NULL) return NULL;
	
	std::memset(tptr, 0, sizeof(EmulationThread));
	
	tptr->thread_ctx.ID = VM->thread_id++;
	tptr->thread_ctx.emulation_ctx.addr_size = 32;
	tptr->thread_ctx.emulation_ctx.sp_size = 32;
	tptr->thread_ctx.emulation_ctx.regs = &tptr->registers;

	SetRegister(tptr, REG_EAX, 0);
	SetRegister(tptr, REG_EBX, 1);
	SetRegister(tptr, REG_ECX, 2);
	SetRegister(tptr, REG_EDX, 3);
	SetRegister(tptr, REG_ESI, 4);
	SetRegister(tptr, REG_EDI, 5);
	
	tptr->EmuVMEM = (VirtualMemory *)VM;
	tptr->VM = (VirtualMemory *)VM;
	
	SetupThreadStack(tptr);
	
	return tptr;	
}


Emulation::~Emulation() {

	for (int i = Current_VM_ID; i > 0; i++) {
		//destroy vm one at a time
	}
}

void Emulation::DeleteMemoryAddresses(MemAddresses  *mptr) {
	MemAddresses *mptr2 = NULL;
	for (; mptr != NULL;) {
		mptr2 = mptr->next;
		delete mptr;
		mptr = mptr2;
	}
}

void Emulation::ClearLogEntry(EmulationThread *thread, EmulationLog *log) {
	EmulationLog *lptr = thread->LogList, *lptr2 = NULL;
	RegChanges *rptr = NULL, *rptr2 = NULL;


	if (lptr == NULL) return;

	for (rptr = lptr->Changes; rptr != NULL; rptr = rptr->next) {
		delete rptr;
	}

	DeleteMemoryAddresses(lptr->Read);
	DeleteMemoryAddresses(lptr->Wrote);

	if (thread->LogList == log) {
		thread->LogList = log->next;
	} else {
		while (lptr != log) {
			lptr2 = lptr->next;
			lptr = lptr->next;
		}
		if (lptr == NULL)
			// shouldnt ever happen
			throw;

		lptr2->next = log->next;

	}
	delete log;
}

void Emulation::ClearLogs(EmulationThread *thread) {
	while (thread->LogList != NULL) {
		ClearLogEntry(thread, thread->LogList);
	}
}


/* pre is for simulation
   post is for logging
*/
int Emulation::PostExecute(EmulationThread *thread, uint32_t EIP) {
	return 0;
	if (Loader->Imports) {
		BinaryLoader::IAT *iatptr = Loader->FindIAT(EIP);
		if (iatptr != NULL) {
			Hooks::APIHook *hptr = NULL;
			
			if (!simulation) {
				hptr = APIHooks.HookFind(iatptr->module, iatptr->function);
			}
			
			if (hptr) {
				char *buffer = FindBuffer(thread, hptr);
				Hooks::ProtocolExchange *xptr = APIHooks.NextProtocolExchange(hptr->id, hptr->side);
				
				
			}
		}
	}	
}


// this will take a current thread context.. and find the buffer for the function thats hooked
// *** Finish this.. and try to remove the constants.. maybe programmatically handle this
char *Emulation::FindBuffer(EmulationThread *thread, Hooks::APIHook *hptr) {
	uint32_t ESP = thread->thread_ctx.emulation_ctx.regs->esp;
	char *ret = NULL;
	
	switch (hptr->find_buffer) {
		case BUF_ESP_p4:
			ret = (char *)(ESP + 4);
			break;
		case BUF_ESP_p8:
			ret = (char *)(ESP + 8);
			break;
		case BUF_ESP_p12:
			ret = (char *)(ESP + 12);
			break;
		case BUF_ESP_p16:
			ret = (char *)(ESP + 16);
			break;
		case BUF_ESP_p20:
			ret = (char *)(ESP + 20);
			break;
		case BUF_ESP_p24:
			ret = (char *)(ESP + 24);
			break;
	}
	
	return ret;
}


// catch IAT (called functions in DLLs) and redirect to either
// foreign machine/wine using API proxy.. or simulate from a log
int Emulation::PreExecute(EmulationThread *thread) {
	uint32_t EIP = thread->thread_ctx.emulation_ctx.regs->eip; 
	if (EIP == 0xDEADDEAD || EIP == 0) {
		printf("Done.. EIP %X\n", EIP);
		exit(-1);
	}
	
	//printf("PRE: %p\n", thread->thread_ctx.emulation_ctx.regs->eip);
	// do some pre-checks on the thread before we execute
	// such as redirects of IAT for API Proxy...
	if (Loader->Imports) {
		BinaryLoader::IAT *iatptr = Loader->FindIAT(EIP);
		if (iatptr != NULL) {
			uint32_t eax_ret = 0;
			uint32_t ret_fix = 0;
			uint32_t esp = 0;
			uint32_t ret_eip = 0;
			Hooks::APIHook *hptr = NULL;
			
			if (simulation) {
				hptr = APIHooks.HookFind(iatptr->module, iatptr->function);
			}
			
			// we found a redirected function for the API Proxy
			printf("Proxy Func/IAT: %s %s\n", iatptr->module, iatptr->function);

			VirtualMachine *_VM = (VirtualMachine *)thread->VM;
			
			if (!simulation || hptr == NULL) {
				// call the function on the proxy server...
				int call_ret = Proxy->CallFunction(iatptr->module, iatptr->function,0,
					thread->thread_ctx.emulation_ctx.regs->esp,
					thread->thread_ctx.emulation_ctx.regs->ebp,
					MasterVM.HeapLow, (MasterVM.HeapHigh - MasterVM.HeapLow), &eax_ret, _VM->StackHigh, &ret_fix);
	
				// *** FIX
				if (call_ret != 1) {	
					printf("ERROR Making call.. fix logic later! (reconnect, etc, etc, local emu)\n");
					exit(-1);
				}
				

			} else {
				// we are simulating.. lets load from the database...
				Hooks::ProtocolExchange *xptr = APIHooks.NextProtocolExchange(hptr->id, hptr->side);
				if (xptr == NULL) {
					printf("Missing protocl exchange... hook id %d side %d module %s func %s\n",
						hptr->id, hptr->side, hptr->module_name, hptr->function_name);
						throw;
				}
				
				// how many bytes & whats the return info for this call..
				eax_ret = xptr->call_ret;
				ret_fix = xptr->ret_fix;
				
				// now we have to copy the real data.. this requires differences for each
				// we need to use a strategy (pretty generic way to tell our hooking functions
				// that a particular buffer can be found on the stack, or at an address dereferenced
				// from the stack...
				char *buffer = FindBuffer(thread, hptr);
				
				// copy the data in place..
				memcpy(buffer, xptr->buf, xptr->size);
				
				// we are done!
				
			}
			
			printf("Ret fix %d ESP %X\n", ret_fix,
				thread->thread_ctx.emulation_ctx.regs->esp);
			
			// return the return value in EAX.. 
			SetRegister(thread, REG_EAX, eax_ret);
			
			// find return address from call instruction
			ret_eip = 0;
			esp = thread->thread_ctx.emulation_ctx.regs->esp;
			VM->MemDataRead(esp, (unsigned char *)&ret_eip, sizeof(uint32_t));
			
			
			printf("RET EIP from Call: %X [from esp %X]\n", ret_eip, esp);
			
			// accomodate calling convention return fixes (callee cleans up)
			//thread->thread_ctx.emulation_ctx.regs->esp += ret_fix;
			esp += ret_fix;
			
			printf("ESP after ret %X\n",thread->thread_ctx.emulation_ctx.regs->esp );
			
			// mimic a return from a called function..
			// EIP = *ESP.. add esp, sizeof(DWORD_PTR)
			SetRegister(thread, REG_EIP, ret_eip);
			esp += sizeof(uint32_t);
			SetRegister(thread, REG_ESP, esp);
			
			return 1;
		}
	}
	return 0;
}


Emulation::EmulationLog *Emulation::StepInstruction(EmulationThread *_thread, CodeAddr Address) {
	EmulationThread *thread = NULL;
	int r = 0, retry_count = 0;
	if (_thread == NULL) thread = MasterThread;
	_BL[0] = Loader;
	EmulationLog *ret = NULL;

	// If we are specifically saying to hit a target address... if not use registers
	if (Address != 0)
		SetRegister(thread, REG_EIP, Address);
		
	if (PreExecute(thread) == 1) {
		thread->last_successful = 1;
		return 0;
	}
		
	CopyRegistersToShadow(thread);

	thread->EmuVMEM->Configure(VirtualMemory::SettingType::SETTINGS_VM_LOGID, ++thread->LogID);
	thread->EmuVMEM->Configure(VirtualMemory::SettingType::SETTINGS_VM_CPU_CYCLE, ++thread->CPUCycle);

	VirtualMachine *_VM = (VirtualMachine *)(thread->VM);
	
	//printf("CTXT: %X ops %X\n",(void *)&thread->thread_ctx.emulation_ctx,(void *)&_VM->emulate_ops);
emu:
	r = x86_emulate((struct x86_emulate_ctxt *)&thread->thread_ctx.emulation_ctx, (const x86_emulate_ops *)&_VM->emulate_ops) == X86EMUL_OKAY;
	// set to 0 since we added more if's below.. maybe rewrite using switch()
	// trying to keep it simple for tons of execs.. might not matter!
	thread->last_successful = 0;
	
	if (r == X86EMUL_OKAY) {
		thread->last_successful = 1;
	} else if (r == X86EMUL_UNHANDLEABLE) {
		printf("UNHANDLEABLE\n");
		// see why things are returning this!! maybe ops are responding
		// incorrectly!
		thread->last_successful = 1;
	} else if (r == X86EMUL_EXCEPTION) {
	} else if (r == X86EMUL_RETRY) {
		printf("X86EMUL_RETRY\n");
		// retry.. like it says..
		if (retry_count++ < 3)
			goto emu;
	} else if (r == X86EMUL_CMPXCHG_FAILED) {
		// maybe just retry this.. not usre what it means about accessor.. we can test soon
		// *** FIX	
	}
	
	if (!thread->last_successful) {
		printf("%X failed! [%d reason]\n", r);
		return NULL;
	}

	// create change logs of registers that were modified...
	if ((ret = CreateLog(thread)) == NULL) {
		printf("error with CreateLog.. probably memory related!\n");
		exit(-1);
	}
	// retrieve Virtual Memory changes from the VM subsystem...
	ret->VMChangeLog = thread->EmuVMEM->ChangeLog_Retrieve(thread->LogID, &ret->VMChangeLog_Count);
	
	// save registers for this specific execution as well for this exact cpu cycle
	std::memcpy(&ret->registers_shadow, &ret->registers_shadow, sizeof(cpu_user_regs_t));
	std::memcpy(&ret->registers, &ret->registers, sizeof(cpu_user_regs_t));

	// now update shadow registers for our next execution
	CopyRegistersToShadow(thread);

	// return the changelog to teh caller
	return ret;
}

// initialize a virtual machine.. and link to a parent one
// if it exists.. (for deep fuzzing/distribution)
Emulation::VirtualMachine *Emulation::NewVirtualMachine(VirtualMachine *parent) {
	VirtualMachine *vptr = new VirtualMachine;
	std::memset(vptr, 0, sizeof(VirtualMachine));
	if (vptr == NULL) return NULL;
	
	if (parent != NULL) {
		vptr->parent = parent;
		vptr->Memory = parent->Memory;
		vptr->Threads = parent->Threads;
	}
}

uint32_t Emulation::HeapAlloc(uint32_t ReqAddr, int Size) {
	Emulation::HeapAllocations *ret = NULL;
	//Emulation::HeapAllocations *aptr = new HeapAllocations;
	//memset(aptr, 0, sizeof(HeapAllocations));

	//CustomHeapArea *aptr = NULL;
	Emulation::HeapAllocations *hptr = NULL;

	//for (aptr = (CustomHeapArea *)tinfo->memory_areas; aptr != NULL; aptr = aptr->next) {
		uint32_t SpaceLeft = MasterVM.HeapHigh - MasterVM.HeapLast;

		if (SpaceLeft <= 0) {
			for (hptr = MasterVM.HeapList; hptr != NULL; hptr = hptr->next) {
				// if free'd heap.. can we take this place??
				if (hptr->free && Size <= hptr->size) {

					uint32_t SizeLeft = (hptr->size - Size);
					// if the size left after we give out this block again is more than 16k..
					// lets put it up for grabs..
					if (SizeLeft > (1024 * 16)) {
						Emulation::HeapAllocations *leftover = (Emulation::HeapAllocations *)
						new Emulation::HeapAllocations;
						memset(leftover, 0, sizeof(Emulation::HeapAllocations));

						if (leftover != NULL) {
							leftover->Address = hptr->Address + Size;
							leftover->size = SizeLeft;

							leftover->next = MasterVM.HeapList;
							MasterVM.HeapList = leftover;
						}
					}
					hptr->size = Size;

		
					printf("CustomHeapAlloc [%d] returning %p\r\n", Size, hptr->Address);
					//exit(-1);

					return (uint32_t) hptr->Address;
				}

			}
		}

			

	//aptr = NULL;
	hptr = NULL;

	//for (aptr = (CustomHeapArea *)tinfo->memory_areas; aptr != NULL; aptr = aptr->next) {
		 SpaceLeft = MasterVM.HeapHigh - MasterVM.HeapLast;
		
		//if (SpaceLeft <= 0) continue;

		hptr = (Emulation::HeapAllocations *) new Emulation::HeapAllocations;
		if (hptr == NULL) {
			printf("out of space\n");
			exit(-1);
		}
		
		hptr->size = Size;

		if (MasterVM.HeapLast == 0) {
			hptr->Address = MasterVM.HeapLow;
		} else {
			hptr->Address = MasterVM.HeapLast;
		}

		MasterVM.HeapLast = hptr->Address + Size;

		//// ensure we free the space.. fuzzing = fine.. but backdoors. we dont want that memory getting transferred during shadow copy/sync
		// *** FIX do on vmem
		//ZeroMemory((void *)hptr->Address, Size);

		hptr->next = MasterVM.HeapList;
		MasterVM.HeapList = hptr;
		//break;
	//}

	printf("CustomHeapAlloc [%d] returning %p [aptr %p hptr %p]\r\n", Size, hptr->Address, 0, hptr);
	//OutputDebugString(ebuf);
	
	//asm("int3");

	return (uint32_t)hptr->Address;
	
}

// mark heap block as freed
int Emulation::HeapFree(uint32_t Address) {
	Emulation::HeapAllocations *hptr = MasterVM.HeapList;
	while (hptr != NULL) {
		
		if (hptr->Address == Address) {
			break;
		}
		hptr = hptr->next;
	}
	
	if (hptr == NULL)
	return -1;
	
	hptr->free = 1;
	
	if (hptr->proxy) {
		// if proxied.. lets clear the memory on the other side.. ***
	}
	
	return 1;
}



Emulation::EmulationThread *Emulation::ExecuteLoop(
	VirtualMemory *vmem, Emulation::CodeAddr StartAddr,
	 Emulation::CodeAddr EndAddr,
	  struct cpu_user_regs *registers,
	  int new_thread) {

	EmulationLog *logptr = NULL;
	EmulationThread *thread = NULL;
	int done = 0, count = 0;
	CodeAddr EIP = StartAddr;

	if (new_thread) {
		thread = NewThread(&MasterVM);//, EIP, registers);
		if (thread == NULL) {
			return NULL;
		}
	} else {
		thread = MasterThread;
	}

	if (thread == MasterThread) {
		std::memcpy((void *)&MasterThread->registers, registers, sizeof(struct cpu_user_regs));
	}

	while (!done) {
		logptr = StepInstruction(thread, EIP);

		if (count++ > 30) break;

		if (thread->registers.eip >= EndAddr)
			done = 1;
	}


	printf("Executed %d instructions\n", count);

	return thread;
}




// this initializes a new virtual machine.. and prepares some information so that the VM has data from its original
// and it will clone on change for any modifications during exec
Emulation::EmulationThread *Emulation::NewVirtualMachineChild(VirtualMemory *ParentMemory, Emulation::CodeAddr EIP,
		struct cpu_user_regs *registers) {
return NULL;
/*
	EmulationThread *Thread = new EmulationThread;
	std::memset((void *)Thread, 0, sizeof(EmulationThread));

	Thread->ID = ++Current_VM_ID;
	Thread->CPUStart = Thread->CPUCycle = Master.CPUCycle;

	Thread->EmuVMEM->SetParent(ParentMemory);
	Thread->EmuVMEM->Configure(VirtualMemory::SettingType::SETTINGS_VM_ID, Thread->ID);
	//Thread->EmuVMEM->Configure(VirtualMemory::SettingType::SETTINGS_VM_LOGID, 0);
*/
}

// this is to destroy a virtual machine (if we have exhausted all instructions through particular branches..
// *** this should queue a new virtual machine for a previously untested branch
void Emulation::DestroyVirtualMachineChild(Emulation::EmulationThread *Thread) {
/*
	// dont do the initial static
	if (Thread == &Master) return;

	Thread->EmuVMEM->ReleaseParent();
	_VM2[Thread->ID] = NULL;
	EmuThread[Thread->ID] = NULL;

	delete Thread;
	*/
	return;
}

Emulation::RegChanges *Emulation::CreateChangeEntry(Emulation::RegChanges **changelist, int which, unsigned char *orig,
		unsigned char *cur, int size) {
	RegChanges *change = new RegChanges;

	std::memset(change, 0, sizeof(RegChanges));


	// ** add 64bit support here...
	switch (size) {
		case sizeof(uint32_t):
			uint32_t orig_32, new_32;
			std::memcpy(&orig_32, orig, sizeof(uint32_t));
			std::memcpy(&new_32, cur, sizeof(uint32_t));
			std::memcpy(&change->RawResult, cur, sizeof(uint32_t));
			change->Type |= (new_32 > orig_32) ? CHANGE_INCREASE : CHANGE_DECREASE;
			change->Result = new_32;
			break;
		case sizeof(uint16_t):
			uint16_t orig_16, new_16;
			std::memcpy(&orig_16, orig, sizeof(uint16_t));
			std::memcpy(&new_16, cur, sizeof(uint16_t));
			std::memcpy(&change->RawResult, cur, sizeof(uint16_t));
			change->Type |= (new_16 > orig_16) ? CHANGE_INCREASE : CHANGE_DECREASE;
			change->Result = new_16;
			break;
		case sizeof(uint8_t):
			uint8_t orig_8, new_8;
			std::memcpy(&orig_8, orig, sizeof(uint8_t));
			std::memcpy(&new_8, cur, sizeof(uint8_t));
			std::memcpy(&change->RawResult, cur, sizeof(uint8_t));
			change->Type |= (new_8 > orig_8) ? CHANGE_INCREASE : CHANGE_DECREASE;
			change->Result = new_8;
			break;
	}

	change->Register = which;
	change->next = *changelist;
	*changelist = change;

	return change;
}

void Emulation::CopyRegistersToShadow(EmulationThread *thread) {
	std::memcpy(&thread->registers_shadow, &thread->registers, sizeof(cpu_user_regs_t));
}


void Emulation::SetRegister(EmulationThread *thread, int Monitor, uint32_t value) {
	if (Monitor & REG_EAX) {
		thread->registers.eax = (uint32_t)value;
	}
	if (Monitor & REG_EIP) {
		thread->registers.eip = (uint32_t) value;
	}
	if (Monitor & REG_EBX) {
		thread->registers.ebx = (uint32_t)value;
	}
	if (Monitor & REG_ECX) {
		thread->registers.ecx = (uint32_t)value;
	}
	if (Monitor & REG_EDX) {
		thread->registers.edx = (uint32_t)value;
	}
	if (Monitor & REG_ESI) {
		thread->registers.esi = (uint32_t)value;
	}
	if (Monitor & REG_EDI) {
		thread->registers.edi = (uint32_t)value;
	}
	if (Monitor & REG_ESP) {
		thread->registers.esp = (uint32_t)value;
	}
	if (Monitor & REG_EBP) {
		thread->registers.ebp = (uint32_t)value;
	}
	if (Monitor & REG_EFLAGS) {
		thread->registers.eflags = (uint32_t)value;
	}
	if (Monitor & REG_CS) {
		thread->registers.cs = (uint16_t)value;
	}
	if (Monitor & REG_ES) {
		thread->registers.es = (uint16_t)value;
	}
	if (Monitor & REG_DS) {
		thread->registers.ds = (uint16_t)value;
	}
	if (Monitor & REG_FS) {
		thread->registers.fs = (uint16_t)value;
	}
	if (Monitor & REG_GS) {
		thread->registers.gs = (uint16_t)value;
	}
	if (Monitor & REG_SS) {
		thread->registers.ss = (uint16_t)value;
	}
}


Emulation::EmulationLog *Emulation::CreateLog(EmulationThread *thread) {
	EmulationLog *logptr;
	int Monitor = 0;

	if (!thread->last_successful) return NULL;

	logptr = new EmulationLog;

	std::memset(logptr, 0, sizeof(EmulationLog));


	logptr->LogID = thread->LogID;

	logptr->Address = thread->registers_shadow.eip;
	// this might change if it changes EIP jmp,call,etc.. should grab from the database...
	logptr->Size = thread->registers.eip - thread->registers_shadow.eip;

	if (thread->registers.eip != thread->registers_shadow.eip) {
		Monitor |= REG_EIP;
		//printf("changed EIP %d %p -> %p\n", Monitor & REG_EIP, thread->registers_shadow.eip, thread->registers.eip);
		CreateChangeEntry(&logptr->Changes, REG_EIP, (unsigned char *)&thread->registers_shadow.eip,  (unsigned char *)&thread->registers.eip, sizeof(uint32_t));
	}
	if (thread->registers.eax != thread->registers_shadow.eax) {
		Monitor |= REG_EAX;
		CreateChangeEntry(&logptr->Changes, REG_EAX,  (unsigned char *)&thread->registers_shadow.eax, (unsigned char *) &thread->registers.eax, sizeof(uint32_t));
	}
	if (thread->registers.ebx != thread->registers_shadow.ebx) {
		Monitor |= REG_EBX;
		CreateChangeEntry(&logptr->Changes, REG_EBX, (unsigned char *)&thread->registers_shadow.ebx, (unsigned char *) &thread->registers.ebx, sizeof(uint32_t));
	}
	if (thread->registers.ecx != thread->registers_shadow.ecx) {
		Monitor |= REG_ECX;
		CreateChangeEntry(&logptr->Changes, REG_ECX, (unsigned char *) &thread->registers_shadow.ecx, (unsigned char *) &thread->registers.ecx, sizeof(uint32_t));
	}
	if (thread->registers.edx != thread->registers_shadow.edx) {
		Monitor |= REG_EDX;
		CreateChangeEntry(&logptr->Changes, REG_EDX, (unsigned char *) &thread->registers_shadow.edx, (unsigned char *) &thread->registers.edx, sizeof(uint32_t));
	}
	if (thread->registers.esp != thread->registers_shadow.esp) {
		Monitor |= REG_ESP;
		CreateChangeEntry(&logptr->Changes, REG_ESP,  (unsigned char *)&thread->registers_shadow.esp,  (unsigned char *)&thread->registers.esp, sizeof(uint32_t));
	}
	if (thread->registers.ebp != thread->registers_shadow.ebp) {
		Monitor |= REG_EBP;
		CreateChangeEntry(&logptr->Changes, REG_EBP,  (unsigned char *)&thread->registers_shadow.ebp, (unsigned char *) &thread->registers.ebp, sizeof(uint32_t));
	}
	if (thread->registers.esi != thread->registers_shadow.esi) {
		Monitor |= REG_ESI;
		CreateChangeEntry(&logptr->Changes, REG_ESI,  (unsigned char *)&thread->registers_shadow.esi,  (unsigned char *)&thread->registers.esi, sizeof(uint32_t));
	}
	if (thread->registers.edi != thread->registers_shadow.edi) {
		Monitor |= REG_EDI;
		CreateChangeEntry(&logptr->Changes, REG_EDI,  (unsigned char *)&thread->registers_shadow.edi, (unsigned char *) &thread->registers.edi, sizeof(uint32_t));
	}
	if (thread->registers.eflags != thread->registers_shadow.eflags) {
		Monitor |= REG_EFLAGS;
		CreateChangeEntry(&logptr->Changes, REG_EFLAGS, (unsigned char *) &thread->registers_shadow.eflags,  (unsigned char *)&thread->registers.eflags, sizeof(uint32_t));
	}
	if (thread->registers.cs != thread->registers_shadow.cs) {
		Monitor |= REG_CS;
		CreateChangeEntry(&logptr->Changes, REG_CS, (unsigned char *) &thread->registers_shadow.cs, (unsigned char *) &thread->registers.cs, sizeof(uint16_t));
	}
	if (thread->registers.es != thread->registers_shadow.es) {
		Monitor |= REG_ES;
		CreateChangeEntry(&logptr->Changes, REG_ES, (unsigned char *) &thread->registers_shadow.es,  (unsigned char *)&thread->registers.es, sizeof(uint16_t));
	}
	if (thread->registers.ds != thread->registers_shadow.ds) {
		Monitor |= REG_DS;
		CreateChangeEntry(&logptr->Changes, REG_DS,  (unsigned char *)&thread->registers_shadow.ds,  (unsigned char *)&thread->registers.ds, sizeof(uint16_t));
	}
	if (thread->registers.fs != thread->registers_shadow.fs) {
		Monitor |= REG_FS;
		CreateChangeEntry(&logptr->Changes, REG_FS, (unsigned char *) &thread->registers_shadow.fs, (unsigned char *) &thread->registers.fs, sizeof(uint16_t));
	}
	if (thread->registers.gs != thread->registers_shadow.gs) {
		Monitor |= REG_GS;
		CreateChangeEntry(&logptr->Changes, REG_GS, (unsigned char *) &thread->registers_shadow.gs, (unsigned char *) &thread->registers.gs, sizeof(uint16_t));
	}
	if (thread->registers.ss != thread->registers_shadow.ss) {
		Monitor |= REG_SS;
		CreateChangeEntry(&logptr->Changes, REG_SS,  (unsigned char *)&thread->registers_shadow.ss, (unsigned char *) &thread->registers.ss, sizeof(uint16_t));
	}

	// duh! we need it in our structure!
	logptr->Monitor = Monitor;

	logptr->next = thread->LogList;
	thread->LogList = logptr;

	return logptr;
}
