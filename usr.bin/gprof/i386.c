/*
 * $DragonFly: src/usr.bin/gprof/i386.c,v 1.2 2003/10/04 20:36:45 hmp Exp $
 */
#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
