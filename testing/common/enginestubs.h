/*
 * enginestubs.h - the few engine services the vendored engine code (message, huffman,
 * collision) needs from its host program.
 */
#ifndef ENGINESTUBS_H
#define ENGINESTUBS_H

#include <setjmp.h>

/* Com_Error longjmps here when set, otherwise it prints and exits. The message is kept in
   Stub_ErrorMsg. */
extern jmp_buf	*Stub_ErrorJmp;
extern char		Stub_ErrorMsg[512];

/* the single "file" FS_ReadFile can return (used for the BSP loaded by CM_LoadMap) */
void	Stub_SetFile( const void *data, int length );

#endif
