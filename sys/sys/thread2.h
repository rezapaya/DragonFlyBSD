/*
 * SYS/THREAD2.H
 *
 *	Implements inline procedure support for the LWKT subsystem. 
 *
 *	Generally speaking these routines only operate on threads associated
 *	with the current cpu.  For example, a higher priority thread pending
 *	on a different cpu will not be immediately scheduled by a yield() on
 *	this cpu.
 *
 * $DragonFly: src/sys/sys/thread2.h,v 1.11 2004/01/01 00:31:46 dillon Exp $
 */

#ifndef _SYS_THREAD2_H_
#define _SYS_THREAD2_H_

/*
 * Userland will have its own globaldata which it includes prior to this.
 */
#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)
#ifndef _SYS_GLOBALDATA_H_
#include <sys/globaldata.h>
#endif
#endif

/*
 * Critical sections prevent preemption by raising a thread's priority
 * above the highest possible interrupting priority.  Additionally, the
 * current cpu will not be able to schedule a new thread but will instead
 * place it on a pending list (with interrupts physically disabled) and
 * set mycpu->gd_reqflags to indicate that work needs to be done, which
 * lwkt_yield_quick() takes care of.
 *
 * Some of these routines take a struct thread pointer as an argument.  This
 * pointer MUST be curthread and is only passed as an optimization.
 *
 * Synchronous switching and blocking is allowed while in a critical section.
 */

static __inline void
crit_enter(void)
{
    struct thread *td = curthread;

    td->td_pri += TDPRI_CRIT;
#ifdef INVARIANTS
    if (td->td_pri < 0)
	crit_panic();
#endif
}

static __inline void
crit_enter_quick(struct thread *curtd)
{
    curtd->td_pri += TDPRI_CRIT;
}

static __inline void
crit_exit_noyield(struct thread *curtd)
{
    curtd->td_pri -= TDPRI_CRIT;
#ifdef INVARIANTS
    if (curtd->td_pri < 0)
	crit_panic();
#endif
}

static __inline void
crit_exit(void)
{
    thread_t td = curthread;

    td->td_pri -= TDPRI_CRIT;
#ifdef INVARIANTS
    if (td->td_pri < 0)
	crit_panic();
#endif
    if (td->td_pri < TDPRI_CRIT && td->td_gd->gd_reqflags)
	lwkt_yield_quick();
}

static __inline void
crit_exit_quick(struct thread *curtd)
{
    curtd->td_pri -= TDPRI_CRIT;
    if (curtd->td_pri < TDPRI_CRIT && curtd->td_gd->gd_reqflags)
	lwkt_yield_quick();
}

static __inline int
crit_panic_save(void)
{
    thread_t td = curthread;
    int pri = td->td_pri;
    td->td_pri = td->td_pri & TDPRI_MASK;
    return(pri);
}

static __inline void
crit_panic_restore(int cpri)
{
    curthread->td_pri = cpri;
}

static __inline int
lwkt_havetoken(lwkt_token_t tok)
{
    return (tok->t_cpu == mycpu->gd_cpuid);
}

/*
 * Return whether any threads are runnable, whether they meet mp_lock
 * requirements or not.
 */
static __inline int
lwkt_runnable(void)
{
    return (mycpu->gd_runqmask != 0);
}

#endif

