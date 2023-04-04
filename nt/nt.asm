;****************************************************************************
;
;    A test app to debug the NHACP library
;
;    Copyright (C) 2023 John Winans
;
;    This library is free software; you can redistribute it and/or
;    modify it under the terms of the GNU Lesser General Public
;    License as published by the Free Software Foundation; either
;    version 2.1 of the License, or (at your option) any later version.
;
;    This library is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;    Lesser General Public License for more details.
;
;    You should have received a copy of the GNU Lesser General Public
;    License along with this library; if not, write to the Free Software
;    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
;    USA
;
;
;****************************************************************************

	org	0x100

	ld	sp,.stack

	call	nhacp_init

	call	iputs
	db	"ready...\r\n\0"

.loop:
	call	nhacp_start
	call	delay1
	jp	.loop

delay1:
	ld	b,0x08
.dloop1:
	ld	hl,0
.dloop:
	dec	hl
	ld	a,h
	or	l
	jp	nz,.dloop
	djnz	.dloop1
	ret



include 'io.asm'
include 'puts.asm'
include 'sio.asm'
include 'nhacp.asm'
include 'hexdump.asm'

	ds	256
	ds      0x100-(($+0x100)&0x0ff)		; align to multiple of 0x100 for EZ dumping
.stack:

.buf:
	ds	256
