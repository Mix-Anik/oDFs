/*
 * pmvm.h - minimal QVM interpreter for the oDFs test host
 *
 * Executes a .qvm exactly like the engine's interpreted VM (qcommon/vm_interpreted.c):
 * same opcode semantics, same stack frame layout, same data masking. No optimisation
 * (macro ops, instruction fixups) is applied. Floats are single precision C floats; build
 * the host with -ffp-contract=off so no operation is fused.
 */
#ifndef PMVM_H
#define PMVM_H

#include <stdint.h>

typedef struct pmvm_s pmvm_t;

/* args[0] is the syscall number (already inverted, so trap_Trace -25 arrives as 24),
   args[1..] are the arguments as 32 bit values; returns the value handed back to the VM */
typedef int ( *pmvmSyscall_t )( pmvm_t *vm, const int32_t *args );

struct pmvm_s {
	uint8_t			*data;			/* data segment + stack, dataMask + 1 + guard bytes */
	uint32_t		dataMask;
	uint32_t		dataAlloc;
	uint32_t		exactDataLength;	/* data + lit + bss of the qvm */
	int32_t			programStack;
	int32_t			*code;			/* two ints per instruction: op, value */
	int				instructionCount;
	pmvmSyscall_t	syscall;
	void			*user;
	char			error[256];		/* set on a fatal VM error */
};

/* returns NULL on failure, with the reason in err */
pmvm_t	*PMVM_Load( const char *path, pmvmSyscall_t syscall, void *user, char *err, int errSize );
void	PMVM_Free( pmvm_t *vm );

/* enters the VM at instruction 0 (vmMain) with up to 4 arguments; returns -1 and sets
   vm->error on a fatal error (the VM is unusable afterwards) */
int		PMVM_Call( pmvm_t *vm, int nargs, const int32_t *args );

/* masked pointer into the data segment, like VMA() in the engine */
static inline void *PMVM_Ptr( const pmvm_t *vm, int32_t addr ) {
	return vm->data + ( (uint32_t)addr & vm->dataMask );
}

#endif
