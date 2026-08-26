/* $FreeBSD$ */
/* $NetBSD: citrus_mmap.h,v 1.1 2003/06/25 09:51:38 tshiozak Exp $ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c)2003 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _CITRUS_MMAP_H_
#define _CITRUS_MMAP_H_

/*
 * There is no mmap, so a region is a heap block holding the whole file, and
 * reading one is a syscall — which a coroutine must await.
 *
 * The split is what keeps most of the library synchronous. The index files are
 * read whatever the conversion, so they are loaded once at startup and
 * _citrus_map_file answers out of that, keeping upstream's signature: every
 * caller that only ever names an index stays an ordinary function. A .esdb,
 * .mps or .646 depends on the encodings asked for and cannot be preloaded; the
 * three sites that read one call _citrus_load_file and are coroutines.
 *
 * A region from the cache is borrowed, and _citrus_unmap_file frees only a
 * block that was read for the occasion.
 */
__BEGIN_DECLS
int _citrus_map_file(struct _citrus_region *__restrict, const char *__restrict);
void _citrus_unmap_file(struct _citrus_region *);
__END_DECLS

#endif
