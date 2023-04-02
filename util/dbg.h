//****************************************************************************
//
//    Copyright (C) 2023 John Winans
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
//    USA
//
//****************************************************************************

#ifndef dbg_H
#define dbg_H

#include <stdio.h>

#define ENABLE_DBG

#ifdef ENABLE_DBG
#define DBG(...) do { printf("*** %s %d %s(): ", __FILE__, __LINE__, __FUNCTION__); printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
#define DBG(...) do { } while(0)
#endif

#endif
