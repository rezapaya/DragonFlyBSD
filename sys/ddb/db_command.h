/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/ddb/db_command.h,v 1.11 1999/08/28 00:41:06 peter Exp $
 * $DragonFly: src/sys/ddb/db_command.h,v 1.4 2006/05/20 02:42:01 dillon Exp $
 */

#ifndef _DDB_DB_COMMAND_H_
#define	_DDB_DB_COMMAND_H_

#ifndef _DDB_DDB_H_
#include <ddb/ddb.h>
#endif

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Command loop declarations.
 */

extern db_addr_t	db_dot;		/* current location */
extern db_addr_t	db_last_addr;	/* last explicit address typed */
extern db_addr_t	db_prev;	/* last address examined
					   or written */
extern db_addr_t	db_next;	/* next address to be examined
					   or written */

#ifdef _KERNEL

void	db_command_loop (void);

#endif

#endif /* !_DDB_DB_COMMAND_H_ */
