/*
 * pmvm.c - minimal QVM interpreter (see pmvm.h)
 *
 * Mirrors VM_LoadQVM / VM_LoadInstructions / VM_CallInterpreted2 of the engine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmvm.h"

#define VM_MAGIC		0x12721444
#define VM_MAGIC_VER2	0x12721445

#define PROGRAM_STACK_SIZE	0x10000
#define PROGRAM_STACK_EXTRA	( 32 * 1024 )
#define VM_DATA_GUARD_SIZE	1024
#define MAX_VMMAIN_CALL_ARGS	4
#define OPSTACK_SIZE		512

typedef enum {
	OP_UNDEF, OP_IGNORE, OP_BREAK, OP_ENTER, OP_LEAVE, OP_CALL, OP_PUSH, OP_POP,
	OP_CONST, OP_LOCAL, OP_JUMP,
	OP_EQ, OP_NE, OP_LTI, OP_LEI, OP_GTI, OP_GEI, OP_LTU, OP_LEU, OP_GTU, OP_GEU,
	OP_EQF, OP_NEF, OP_LTF, OP_LEF, OP_GTF, OP_GEF,
	OP_LOAD1, OP_LOAD2, OP_LOAD4, OP_STORE1, OP_STORE2, OP_STORE4, OP_ARG, OP_BLOCK_COPY,
	OP_SEX8, OP_SEX16, OP_NEGI, OP_ADD, OP_SUB, OP_DIVI, OP_DIVU, OP_MODI, OP_MODU, OP_MULI, OP_MULU,
	OP_BAND, OP_BOR, OP_BXOR, OP_BCOM, OP_LSH, OP_RSHI, OP_RSHU,
	OP_NEGF, OP_ADDF, OP_SUBF, OP_DIVF, OP_MULF, OP_CVIF, OP_CVFI,
	OP_MAX
} opcode_t;

/* operand size in the file: 0, 1 or 4 bytes (qcommon/vm.c ops[]) */
static int OperandSize( int op ) {
	switch ( op ) {
	case OP_ENTER: case OP_LEAVE: case OP_CONST: case OP_LOCAL: case OP_BLOCK_COPY:
		return 4;
	case OP_ARG:
		return 1;
	default:
		if ( op >= OP_EQ && op <= OP_GEF ) {
			return 4;
		}
		return 0;
	}
}

typedef union {
	int32_t		i;
	uint32_t	u;
	float		f;
} floatint_t;

static uint32_t Pow2Pad( uint32_t v ) {
	uint32_t n = 1;

	while ( n < v ) {
		n <<= 1;
	}
	return n;
}

static int Rd32( const uint8_t *p ) {
	return (int32_t)( p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( (uint32_t)p[3] << 24 ) );
}

void PMVM_Free( pmvm_t *vm ) {
	if ( vm ) {
		free( vm->data );
		free( vm->code );
		free( vm );
	}
}

pmvm_t *PMVM_Load( const char *path, pmvmSyscall_t syscall, void *user, char *err, int errSize ) {
	FILE		*f;
	uint8_t		*file;
	long		length;
	pmvm_t		*vm;
	int			magic, instructionCount, codeOffset, codeLength, dataOffset, dataLength, litLength, bssLength;
	uint32_t	exact, total;
	const uint8_t	*cp, *cend;
	int			i;

	f = fopen( path, "rb" );
	if ( !f ) {
		snprintf( err, errSize, "cannot open %s", path );
		return NULL;
	}
	fseek( f, 0, SEEK_END );
	length = ftell( f );
	fseek( f, 0, SEEK_SET );
	file = (uint8_t *)malloc( length + 16 );
	if ( fread( file, 1, length, f ) != (size_t)length ) {
		fclose( f );
		free( file );
		snprintf( err, errSize, "cannot read %s", path );
		return NULL;
	}
	fclose( f );

	if ( length < 32 ) {
		free( file );
		snprintf( err, errSize, "%s: too short", path );
		return NULL;
	}

	magic = Rd32( file );
	instructionCount = Rd32( file + 4 );
	codeOffset = Rd32( file + 8 );
	codeLength = Rd32( file + 12 );
	dataOffset = Rd32( file + 16 );
	dataLength = Rd32( file + 20 );
	litLength = Rd32( file + 24 );
	bssLength = Rd32( file + 28 );

	if ( ( magic != VM_MAGIC && magic != VM_MAGIC_VER2 ) || instructionCount <= 0 || codeOffset < 0 ||
		codeLength < 0 || dataOffset < 0 || dataLength < 0 || litLength < 0 || bssLength < 0 ||
		(long)codeOffset + codeLength > length || (long)dataOffset + dataLength + litLength > length ) {
		free( file );
		snprintf( err, errSize, "%s: bad qvm header", path );
		return NULL;
	}

	vm = (pmvm_t *)calloc( 1, sizeof( *vm ) );
	vm->syscall = syscall;
	vm->user = user;
	vm->instructionCount = instructionCount;

	/* data segment: same sizing rules as VM_LoadQVM */
	exact = (uint32_t)dataLength + litLength + bssLength;
	total = exact < PROGRAM_STACK_SIZE ? PROGRAM_STACK_SIZE : exact;
	if ( Pow2Pad( total ) - total >= PROGRAM_STACK_EXTRA ) {
		total = Pow2Pad( total );
	} else {
		total += PROGRAM_STACK_EXTRA;
		total = Pow2Pad( total );
	}
	vm->exactDataLength = exact;
	vm->dataMask = total - 1;
	vm->dataAlloc = total + VM_DATA_GUARD_SIZE;
	vm->data = (uint8_t *)calloc( 1, vm->dataAlloc );
	memcpy( vm->data, file + dataOffset, (size_t)dataLength + litLength );
	vm->programStack = (int32_t)( vm->dataMask + 1 );

	/* instructions */
	vm->code = (int32_t *)calloc( (size_t)instructionCount + 8, 2 * sizeof( int32_t ) );
	cp = file + codeOffset;
	cend = cp + codeLength;
	for ( i = 0; i < instructionCount; i++ ) {
		int op, n;

		if ( cp >= cend ) {
			snprintf( err, errSize, "%s: code ends early", path );
			PMVM_Free( vm );
			free( file );
			return NULL;
		}
		op = *cp++;
		if ( op >= OP_MAX ) {
			snprintf( err, errSize, "%s: bad opcode %02X", path, op );
			PMVM_Free( vm );
			free( file );
			return NULL;
		}
		n = OperandSize( op );
		vm->code[i * 2] = op;
		if ( n == 4 ) {
			vm->code[i * 2 + 1] = Rd32( cp );
		} else if ( n == 1 ) {
			vm->code[i * 2 + 1] = *cp;
		}
		cp += n;
	}

	free( file );
	return vm;
}

int PMVM_Call( pmvm_t *vm, int nargs, const int32_t *args ) {
	int32_t		stack[OPSTACK_SIZE];
	int32_t		*opStack = &stack[1];
	int32_t		programStack = vm->programStack;
	int32_t		stackOnEntry = programStack;
	uint8_t		*image = vm->data;
	uint32_t	dataMask = vm->dataMask;
	const int32_t	*code = vm->code;
	const int32_t	*ci;
	int32_t		*img;
	floatint_t	r0, r1;
	int32_t		v0, v1;
	int			i;

	memset( stack, 0, sizeof( stack ) );

	programStack -= ( MAX_VMMAIN_CALL_ARGS + 2 ) * sizeof( int32_t );
	img = (int32_t *)&image[programStack];
	for ( i = 0; i < nargs; i++ ) {
		img[i + 2] = args[i];
	}
	img[1] = 0;
	img[0] = -1;

	ci = code;

	for ( ;; ) {
		int op;

		r0.i = opStack[0];
		r1.i = opStack[-1];

next:
		op = ci[0];
		v0 = ci[1];
		ci += 2;

		if ( opStack < &stack[1] || opStack >= &stack[OPSTACK_SIZE - 2] ) {
			snprintf( vm->error, sizeof( vm->error ), "opstack overflow" );
			return -1;
		}

		switch ( op ) {
		case OP_BREAK:
			goto next;

		case OP_ENTER:
			programStack -= v0;
			if ( programStack < (int32_t)( vm->dataMask + 1 ) - PROGRAM_STACK_SIZE - PROGRAM_STACK_EXTRA ) {
				snprintf( vm->error, sizeof( vm->error ), "programStack overflow" );
				return -1;
			}
			break;

		case OP_LEAVE:
			programStack += v0;
			v1 = *(int32_t *)&image[programStack];
			if ( v1 == -1 ) {
				goto done;
			} else if ( (uint32_t)v1 >= (uint32_t)vm->instructionCount ) {
				snprintf( vm->error, sizeof( vm->error ), "pc out of range in OP_LEAVE" );
				return -1;
			}
			ci = code + v1 * 2;
			break;

		case OP_CALL:
			*(int32_t *)&image[programStack] = (int32_t)( ( ci - code ) / 2 );
			if ( r0.i < 0 ) {
				/* system call; the callee sees the number at programStack + 4 */
				vm->programStack = programStack - 8;
				*(int32_t *)&image[programStack + 4] = ~r0.i;
				{
					int32_t argarr[16];
					int a;

					for ( a = 0; a < 16; a++ ) {
						argarr[a] = *(int32_t *)&image[programStack + 4 + 4 * a];
					}
					v0 = vm->syscall( vm, argarr );
				}
				if ( vm->error[0] ) {
					return -1;
				}
				ci = code + *(int32_t *)&image[programStack] * 2;
				*opStack = v0;
			} else if ( r0.u < (uint32_t)vm->instructionCount ) {
				ci = code + r0.i * 2;
				opStack--;
			} else {
				snprintf( vm->error, sizeof( vm->error ), "pc out of range in OP_CALL" );
				return -1;
			}
			break;

		case OP_PUSH:
			opStack++;
			break;

		case OP_POP:
			opStack--;
			break;

		case OP_CONST:
			opStack++;
			r1.i = r0.i;
			r0.i = *opStack = v0;
			goto next;

		case OP_LOCAL:
			opStack++;
			r1.i = r0.i;
			r0.i = *opStack = v0 + programStack;
			goto next;

		case OP_JUMP:
			if ( r0.u >= (uint32_t)vm->instructionCount ) {
				snprintf( vm->error, sizeof( vm->error ), "pc out of range in OP_JUMP" );
				return -1;
			}
			ci = code + r0.i * 2;
			opStack--;
			break;

		case OP_EQ:  opStack -= 2; if ( r1.i == r0.i ) ci = code + v0 * 2; break;
		case OP_NE:  opStack -= 2; if ( r1.i != r0.i ) ci = code + v0 * 2; break;
		case OP_LTI: opStack -= 2; if ( r1.i <  r0.i ) ci = code + v0 * 2; break;
		case OP_LEI: opStack -= 2; if ( r1.i <= r0.i ) ci = code + v0 * 2; break;
		case OP_GTI: opStack -= 2; if ( r1.i >  r0.i ) ci = code + v0 * 2; break;
		case OP_GEI: opStack -= 2; if ( r1.i >= r0.i ) ci = code + v0 * 2; break;
		case OP_LTU: opStack -= 2; if ( r1.u <  r0.u ) ci = code + v0 * 2; break;
		case OP_LEU: opStack -= 2; if ( r1.u <= r0.u ) ci = code + v0 * 2; break;
		case OP_GTU: opStack -= 2; if ( r1.u >  r0.u ) ci = code + v0 * 2; break;
		case OP_GEU: opStack -= 2; if ( r1.u >= r0.u ) ci = code + v0 * 2; break;
		case OP_EQF: opStack -= 2; if ( r1.f == r0.f ) ci = code + v0 * 2; break;
		case OP_NEF: opStack -= 2; if ( r1.f != r0.f ) ci = code + v0 * 2; break;
		case OP_LTF: opStack -= 2; if ( r1.f <  r0.f ) ci = code + v0 * 2; break;
		case OP_LEF: opStack -= 2; if ( r1.f <= r0.f ) ci = code + v0 * 2; break;
		case OP_GTF: opStack -= 2; if ( r1.f >  r0.f ) ci = code + v0 * 2; break;
		case OP_GEF: opStack -= 2; if ( r1.f >= r0.f ) ci = code + v0 * 2; break;

		case OP_LOAD1:
			r0.i = *opStack = image[r0.u & dataMask];
			goto next;

		case OP_LOAD2:
			r0.i = *opStack = *(uint16_t *)&image[r0.u & dataMask];
			goto next;

		case OP_LOAD4:
			r0.i = *opStack = *(int32_t *)&image[r0.u & dataMask];
			goto next;

		case OP_STORE1:
			image[r1.u & dataMask] = (uint8_t)r0.i;
			opStack -= 2;
			break;

		case OP_STORE2:
			*(int16_t *)&image[r1.u & dataMask] = (int16_t)r0.i;
			opStack -= 2;
			break;

		case OP_STORE4:
			*(int32_t *)&image[r1.u & dataMask] = r0.i;
			opStack -= 2;
			break;

		case OP_ARG:
			*(int32_t *)&image[v0 + programStack] = r0.i;
			opStack--;
			break;

		case OP_BLOCK_COPY:
			{
				int32_t count = v0, srci = (int32_t)( r0.u & dataMask ), desti = (int32_t)( r1.u & dataMask );

				count = ( ( srci + count ) & dataMask ) - srci;
				count = ( ( desti + count ) & dataMask ) - desti;
				memmove( &image[desti], &image[srci], count );
				opStack -= 2;
			}
			break;

		case OP_SEX8:  *opStack = (signed char)*opStack; break;
		case OP_SEX16: *opStack = (signed short)*opStack; break;
		case OP_NEGI:  *opStack = -r0.i; break;
		case OP_ADD:   *( --opStack ) = (int32_t)( r1.u + r0.u ); break;
		case OP_SUB:   *( --opStack ) = (int32_t)( r1.u - r0.u ); break;
		case OP_DIVI:
			if ( r0.i == 0 ) {
				snprintf( vm->error, sizeof( vm->error ), "integer divide by zero" );
				return -1;
			}
			*( --opStack ) = r1.i / r0.i;
			break;
		case OP_DIVU:
			if ( r0.u == 0 ) {
				snprintf( vm->error, sizeof( vm->error ), "integer divide by zero" );
				return -1;
			}
			*( --opStack ) = r1.u / r0.u;
			break;
		case OP_MODI:
			if ( r0.i == 0 ) {
				snprintf( vm->error, sizeof( vm->error ), "integer divide by zero" );
				return -1;
			}
			*( --opStack ) = r1.i % r0.i;
			break;
		case OP_MODU:
			if ( r0.u == 0 ) {
				snprintf( vm->error, sizeof( vm->error ), "integer divide by zero" );
				return -1;
			}
			*( --opStack ) = r1.u % r0.u;
			break;
		case OP_MULI:  *( --opStack ) = (int32_t)( r1.u * r0.u ); break;
		case OP_MULU:  *( --opStack ) = (int32_t)( r1.u * r0.u ); break;
		case OP_BAND:  *( --opStack ) = r1.u & r0.u; break;
		case OP_BOR:   *( --opStack ) = r1.u | r0.u; break;
		case OP_BXOR:  *( --opStack ) = r1.u ^ r0.u; break;
		case OP_BCOM:  *opStack = ~r0.u; break;
		case OP_LSH:   *( --opStack ) = (int32_t)( r1.u << ( r0.i & 31 ) ); break;
		case OP_RSHI:  *( --opStack ) = r1.i >> ( r0.i & 31 ); break;
		case OP_RSHU:  *( --opStack ) = r1.u >> ( r0.i & 31 ); break;

		case OP_NEGF:  *(float *)opStack = -r0.f; break;
		case OP_ADDF:  *(float *)( --opStack ) = r1.f + r0.f; break;
		case OP_SUBF:  *(float *)( --opStack ) = r1.f - r0.f; break;
		case OP_DIVF:  *(float *)( --opStack ) = r1.f / r0.f; break;
		case OP_MULF:  *(float *)( --opStack ) = r1.f * r0.f; break;
		case OP_CVIF:  *(float *)opStack = (float)r0.i; break;
		case OP_CVFI:  *opStack = (int32_t)r0.f; break;

		default:
			snprintf( vm->error, sizeof( vm->error ), "bad opcode %d", op );
			return -1;
		}
	}

done:
	if ( opStack != &stack[2] ) {
		snprintf( vm->error, sizeof( vm->error ), "interpreter error: opStack = %ld", (long)( opStack - stack ) );
		return -1;
	}
	vm->programStack = stackOnEntry;
	return *opStack;
}
