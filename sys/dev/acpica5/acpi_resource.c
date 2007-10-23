/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/dev/acpica/acpi_resource.c,v 1.25 2004/06/13 22:52:30 njl Exp $
 * $DragonFly: src/sys/dev/acpica5/acpi_resource.c,v 1.9 2007/10/23 03:04:48 y0netan1 Exp $
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include "acpi.h"
#include <dev/acpica5/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("RESOURCE")

/*
 * Fetch a device's resources and associate them with the device.
 *
 * Note that it might be nice to also locate ACPI-specific resource items, such
 * as GPE bits.
 *
 * We really need to split the resource-fetching code out from the
 * resource-parsing code, since we may want to use the parsing
 * code for _PRS someday.
 */
ACPI_STATUS
acpi_parse_resources(device_t dev, ACPI_HANDLE handle,
		     struct acpi_parse_resource_set *set, void *arg)
{
    ACPI_BUFFER		buf;
    ACPI_RESOURCE	*res;
    char		*curr, *last;
    ACPI_STATUS		status;
    void		*context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Special-case some devices that abuse _PRS/_CRS to mean
     * something other than "I consume this resource".
     *
     * XXX do we really need this?  It's only relevant once
     *     we start always-allocating these resources, and even
     *     then, the only special-cased device is likely to be
     *     the PCI interrupt link.
     */

    /* Fetch the device's current resources. */
    buf.Length = ACPI_ALLOCATE_BUFFER;
    if (ACPI_FAILURE((status = AcpiGetCurrentResources(handle, &buf)))) {
	if (status != AE_NOT_FOUND && status != AE_TYPE)
	    kprintf("can't fetch resources for %s - %s\n",
		   acpi_name(handle), AcpiFormatException(status));
	return_ACPI_STATUS (status);
    }
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "%s - got %ld bytes of resources\n",
		     acpi_name(handle), (long)buf.Length));
    set->set_init(dev, arg, &context);

    /* Iterate through the resources */
    curr = buf.Pointer;
    last = (char *)buf.Pointer + buf.Length;
    while (curr < last) {
	res = (ACPI_RESOURCE *)curr;
	curr += res->Length;

	/* Handle the individual resource types */
	switch(res->Type) {
	case ACPI_RESOURCE_TYPE_END_TAG:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "EndTag\n"));
	    curr = last;
	    break;
	case ACPI_RESOURCE_TYPE_FIXED_IO:
	    if (res->Data.FixedIo.AddressLength <= 0)
		break;
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "FixedIo 0x%x/%d\n",
			     res->Data.FixedIo.Address,
			     res->Data.FixedIo.AddressLength));
	    set->set_ioport(dev, context,
			    res->Data.FixedIo.Address,
			    res->Data.FixedIo.AddressLength);
	    break;
	case ACPI_RESOURCE_TYPE_IO:
	    if (res->Data.Io.AddressLength <= 0)
		break;
	    if (res->Data.Io.Minimum == res->Data.Io.Maximum) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Io 0x%x/%d\n",
				 res->Data.Io.Minimum,
				 res->Data.Io.AddressLength));
		set->set_ioport(dev, context,
				res->Data.Io.Minimum,
				res->Data.Io.AddressLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Io 0x%x-0x%x/%d\n",
				 res->Data.Io.Minimum,
				 res->Data.Io.Maximum, 
				 res->Data.Io.AddressLength));
		set->set_iorange(dev, context,
				 res->Data.Io.Minimum,
				 res->Data.Io.Maximum, 
				 res->Data.Io.AddressLength,
				 res->Data.Io.Alignment);
	    }
	    break;
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
	    if (res->Data.FixedMemory32.AddressLength <= 0)
		break;
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "FixedMemory32 0x%x/%d\n",
			      res->Data.FixedMemory32.Address, 
			      res->Data.FixedMemory32.AddressLength));
	    set->set_memory(dev, context,
			    res->Data.FixedMemory32.Address, 
			    res->Data.FixedMemory32.AddressLength);
	    break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
	    if (res->Data.Memory32.AddressLength <= 0)
		break;
	    if (res->Data.Memory32.Minimum ==
		res->Data.Memory32.Maximum) {

		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory32 0x%x/%d\n",
				  res->Data.Memory32.Minimum, 
				  res->Data.Memory32.AddressLength));
		set->set_memory(dev, context,
				res->Data.Memory32.Minimum,
				res->Data.Memory32.AddressLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory32 0x%x-0x%x/%d\n",
				 res->Data.Memory32.Minimum, 
				 res->Data.Memory32.Maximum,
				 res->Data.Memory32.AddressLength));
		set->set_memoryrange(dev, context,
				     res->Data.Memory32.Minimum,
				     res->Data.Memory32.Maximum,
				     res->Data.Memory32.AddressLength,
				     res->Data.Memory32.Alignment);
	    }
	    break;
	case ACPI_RESOURCE_TYPE_MEMORY24:
	    if (res->Data.Memory24.AddressLength <= 0)
		break;
	    if (res->Data.Memory24.Minimum ==
		res->Data.Memory24.Maximum) {

		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory24 0x%x/%d\n",
				 res->Data.Memory24.Minimum, 
				 res->Data.Memory24.AddressLength));
		set->set_memory(dev, context, res->Data.Memory24.Minimum,
				res->Data.Memory24.AddressLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory24 0x%x-0x%x/%d\n",
				 res->Data.Memory24.Minimum, 
				 res->Data.Memory24.Maximum,
				 res->Data.Memory24.AddressLength));
		set->set_memoryrange(dev, context,
				     res->Data.Memory24.Minimum,
				     res->Data.Memory24.Maximum,
				     res->Data.Memory24.AddressLength,
				     res->Data.Memory24.Alignment);
	    }
	    break;
	case ACPI_RESOURCE_TYPE_IRQ:
	    /*
	     * from 1.0b 6.4.2 
	     * "This structure is repeated for each separate interrupt
	     * required"
	     */
	    set->set_irq(dev, context, res->Data.Irq.Interrupts,
		res->Data.Irq.InterruptCount, res->Data.Irq.Triggering,
		res->Data.Irq.Polarity);
	    break;
	case ACPI_RESOURCE_TYPE_DMA:
	    /*
	     * from 1.0b 6.4.3 
	     * "This structure is repeated for each separate dma channel
	     * required"
	     */
	    set->set_drq(dev, context, res->Data.Dma.Channels,
			 res->Data.Dma.ChannelCount);
	    break;
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "start dependent functions\n"));
	    set->set_start_dependant(dev, context,
				     res->Data.StartDpf.CompatibilityPriority);
	    break;
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "end dependent functions\n"));
	    set->set_end_dependant(dev, context);
	    break;
	case ACPI_RESOURCE_TYPE_ADDRESS32:
	    if (res->Data.Address32.AddressLength <= 0)
		break;
	    if (res->Data.Address32.ProducerConsumer != ACPI_CONSUMER) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
		    "ignored Address32 %s producer\n",
		    res->Data.Address32.ResourceType == ACPI_IO_RANGE ?
		    "IO" : "Memory"));
		break;
	    }
	    if (res->Data.Address32.ResourceType != ACPI_MEMORY_RANGE &&
		res->Data.Address32.ResourceType != ACPI_IO_RANGE) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
		    "ignored Address32 for non-memory, non-I/O\n"));
		break;
	    }

	    if (res->Data.Address32.Minimum == ACPI_ADDRESS_FIXED &&
		res->Data.Address32.Maximum == ACPI_ADDRESS_FIXED) {

		if (res->Data.Address32.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address32/Memory 0x%x/%d\n",
				     res->Data.Address32.Minimum,
				     res->Data.Address32.AddressLength));
		    set->set_memory(dev, context,
				    res->Data.Address32.Minimum,
				    res->Data.Address32.AddressLength);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address32/IO 0x%x/%d\n",
				     res->Data.Address32.Minimum,
				     res->Data.Address32.AddressLength));
		    set->set_ioport(dev, context,
				    res->Data.Address32.Minimum,
				    res->Data.Address32.AddressLength);
		}
	    } else {
		if (res->Data.Address32.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address32/Memory 0x%x-0x%x/%d\n",
				     res->Data.Address32.Minimum,
				     res->Data.Address32.Maximum,
				     res->Data.Address32.AddressLength));
		    set->set_memoryrange(dev, context,
					  res->Data.Address32.Minimum,
					  res->Data.Address32.Maximum,
					  res->Data.Address32.AddressLength,
					  res->Data.Address32.Granularity);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address32/IO 0x%x-0x%x/%d\n",
				     res->Data.Address32.Minimum,
				     res->Data.Address32.Maximum,
				     res->Data.Address32.AddressLength));
		    set->set_iorange(dev, context,
				     res->Data.Address32.Minimum,
				     res->Data.Address32.Maximum,
				     res->Data.Address32.AddressLength,
				     res->Data.Address32.Granularity);
		}
	    }		    
	    break;
	case ACPI_RESOURCE_TYPE_ADDRESS16:
	    if (res->Data.Address16.AddressLength <= 0)
		break;
	    if (res->Data.Address16.ProducerConsumer != ACPI_CONSUMER) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
		    "ignored Address16 %s producer\n",
		    res->Data.Address16.ResourceType == ACPI_IO_RANGE ?
		    "IO" : "Memory"));
		break;
	    }
	    if (res->Data.Address16.ResourceType != ACPI_MEMORY_RANGE &&
		res->Data.Address16.ResourceType != ACPI_IO_RANGE) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
			"ignored Address16 for non-memory, non-I/O\n"));
		break;
	    }

	    if (res->Data.Address16.Minimum == ACPI_ADDRESS_FIXED &&
		res->Data.Address16.Maximum == ACPI_ADDRESS_FIXED) {

		if (res->Data.Address16.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address16/Memory 0x%x/%d\n",
				     res->Data.Address16.Minimum,
				     res->Data.Address16.AddressLength));
		    set->set_memory(dev, context,
				    res->Data.Address16.Minimum,
				    res->Data.Address16.AddressLength);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address16/IO 0x%x/%d\n",
				     res->Data.Address16.Minimum,
				     res->Data.Address16.AddressLength));
		    set->set_ioport(dev, context,
				    res->Data.Address16.Minimum,
				    res->Data.Address16.AddressLength);
		}
	    } else {
		if (res->Data.Address16.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address16/Memory 0x%x-0x%x/%d\n",
				     res->Data.Address16.Minimum,
				     res->Data.Address16.Maximum,
				     res->Data.Address16.AddressLength));
		    set->set_memoryrange(dev, context,
					  res->Data.Address16.Minimum,
					  res->Data.Address16.Maximum,
					  res->Data.Address16.AddressLength,
					  res->Data.Address16.Granularity);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address16/IO 0x%x-0x%x/%d\n",
				     res->Data.Address16.Minimum,
				     res->Data.Address16.Maximum,
				     res->Data.Address16.AddressLength));
		    set->set_iorange(dev, context,
				     res->Data.Address16.Minimum,
				     res->Data.Address16.Maximum,
				     res->Data.Address16.AddressLength,
				     res->Data.Address16.Granularity);
		}
	    }		    
	    break;
	case ACPI_RESOURCE_TYPE_ADDRESS64:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
			     "unimplemented Address64 resource\n"));
	    break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
	    /* XXX special handling? */
	    set->set_ext_irq(dev, context, res->Data.ExtendedIrq.Interrupts,
		res->Data.ExtendedIrq.InterruptCount,
		res->Data.ExtendedIrq.Triggering,
		res->Data.ExtendedIrq.Polarity);
	    break;
	case ACPI_RESOURCE_TYPE_VENDOR:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
			     "unimplemented VendorSpecific resource\n"));
	    break;
	default:
	    break;
	}
    }    

    AcpiOsFree(buf.Pointer);
    set->set_done(dev, context);
    return_ACPI_STATUS (AE_OK);
}

/*
 * Resource-set vectors used to attach _CRS-derived resources 
 * to an ACPI device.
 */
static void	acpi_res_set_init(device_t dev, void *arg, void **context);
static void	acpi_res_set_done(device_t dev, void *context);
static void	acpi_res_set_ioport(device_t dev, void *context,
				    u_int32_t base, u_int32_t length);
static void	acpi_res_set_iorange(device_t dev, void *context,
				     u_int32_t low, u_int32_t high, 
				     u_int32_t length, u_int32_t align);
static void	acpi_res_set_memory(device_t dev, void *context,
				    u_int32_t base, u_int32_t length);
static void	acpi_res_set_memoryrange(device_t dev, void *context,
					 u_int32_t low, u_int32_t high, 
					 u_int32_t length, u_int32_t align);
static void	acpi_res_set_irq(device_t dev, void *context, u_int8_t *irq,
				 int count, int trig, int pol);
static void	acpi_res_set_ext_irq(device_t dev, void *context,
				     u_int32_t *irq, int count, int trig,
				     int pol);
static void	acpi_res_set_drq(device_t dev, void *context, u_int8_t *drq,
				 int count);
static void	acpi_res_set_start_dependant(device_t dev, void *context,
					     int preference);
static void	acpi_res_set_end_dependant(device_t dev, void *context);

struct acpi_parse_resource_set acpi_res_parse_set = {
    acpi_res_set_init,
    acpi_res_set_done,
    acpi_res_set_ioport,
    acpi_res_set_iorange,
    acpi_res_set_memory,
    acpi_res_set_memoryrange,
    acpi_res_set_irq,
    acpi_res_set_ext_irq,
    acpi_res_set_drq,
    acpi_res_set_start_dependant,
    acpi_res_set_end_dependant
};

struct acpi_res_context {
    int		ar_nio;
    int		ar_nmem;
    int		ar_nirq;
    int		ar_ndrq;
    void	*ar_parent;
};

static void
acpi_res_set_init(device_t dev, void *arg, void **context)
{
    struct acpi_res_context	*cp;

    if ((cp = AcpiOsAllocate(sizeof(*cp))) != NULL) {
	bzero(cp, sizeof(*cp));
	cp->ar_parent = arg;
	*context = cp;
    }
}

static void
acpi_res_set_done(device_t dev, void *context)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    AcpiOsFree(cp);
}

static void
acpi_res_set_ioport(device_t dev, void *context, u_int32_t base,
		    u_int32_t length)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    bus_set_resource(dev, SYS_RES_IOPORT, cp->ar_nio++, base, length);
}

static void
acpi_res_set_iorange(device_t dev, void *context, u_int32_t low,
		     u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "I/O range not supported\n");
}

static void
acpi_res_set_memory(device_t dev, void *context, u_int32_t base,
		    u_int32_t length)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;

    bus_set_resource(dev, SYS_RES_MEMORY, cp->ar_nmem++, base, length);
}

static void
acpi_res_set_memoryrange(device_t dev, void *context, u_int32_t low,
			 u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "memory range not supported\n");
}

static void
acpi_res_set_irq(device_t dev, void *context, u_int8_t *irq, int count,
    int trig, int pol)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL || irq == NULL)
	return;

    /* This implements no resource relocation. */
    if (count != 1)
	return;

    bus_set_resource(dev, SYS_RES_IRQ, cp->ar_nirq++, *irq, 1);
#if 0	/* From FreeBSD-5 XXX */
    BUS_CONFIG_INTR(dev, *irq, (trig == ACPI_EDGE_SENSITIVE) ?
	INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL, (pol == ACPI_ACTIVE_HIGH) ?
	INTR_POLARITY_HIGH : INTR_POLARITY_LOW);
#endif
}

static void
acpi_res_set_ext_irq(device_t dev, void *context, u_int32_t *irq, int count,
    int trig, int pol)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL || irq == NULL)
	return;

    /* This implements no resource relocation. */
    if (count != 1)
	return;

    bus_set_resource(dev, SYS_RES_IRQ, cp->ar_nirq++, *irq, 1);
#if 0	/* From FreeBSD-5 XXX */
    BUS_CONFIG_INTR(dev, *irq, (trig == ACPI_EDGE_SENSITIVE) ?
	INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL, (pol == ACPI_ACTIVE_HIGH) ?
	INTR_POLARITY_HIGH : INTR_POLARITY_LOW);
#endif
}

static void
acpi_res_set_drq(device_t dev, void *context, u_int8_t *drq, int count)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL || drq == NULL)
	return;
    
    /* This implements no resource relocation. */
    if (count != 1)
	return;

    bus_set_resource(dev, SYS_RES_DRQ, cp->ar_ndrq++, *drq, 1);
}

static void
acpi_res_set_start_dependant(device_t dev, void *context, int preference)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "dependent functions not supported\n");
}

static void
acpi_res_set_end_dependant(device_t dev, void *context)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "dependent functions not supported\n");
}

/*
 * Resource-owning placeholders for IO and memory pseudo-devices.
 *
 * This code allocates system resource objects that will be owned by ACPI
 * child devices.  Really, the acpi parent device should have the resources
 * but this would significantly affect the device probe code.
 */

static int	acpi_sysres_probe(device_t dev);
static int	acpi_sysres_attach(device_t dev);

static device_method_t acpi_sysres_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_sysres_probe),
    DEVMETHOD(device_attach,	acpi_sysres_attach),

    {0, 0}
};

static driver_t acpi_sysres_driver = {
    "acpi_sysresource",
    acpi_sysres_methods,
    0,
};

static devclass_t acpi_sysres_devclass;
DRIVER_MODULE(acpi_sysresource, acpi, acpi_sysres_driver, acpi_sysres_devclass,
    0, 0);
MODULE_DEPEND(acpi_sysresource, acpi, 1, 1, 1);

static int
acpi_sysres_probe(device_t dev)
{
    ACPI_HANDLE h;

    h = acpi_get_handle(dev);
    if (acpi_disabled("sysresource") ||
	(!acpi_MatchHid(h, "PNP0C01") && !acpi_MatchHid(h, "PNP0C02")))
	return (ENXIO);

    device_set_desc(dev, "System Resource");
    device_quiet(dev);
    return (-100);
}

static int
acpi_sysres_attach(device_t dev)
{
    device_t gparent;
    struct resource *res;
    struct rman *rm;
    struct resource_list_entry *rle;
    struct resource_list *rl;

    /*
     * Pre-allocate/manage all memory and IO resources.  We detect duplicates
     * by setting rle->res to the resource we got from the parent.  We can't
     * ignore them since rman can't handle duplicates.
     */
    rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
    SLIST_FOREACH(rle, rl, link) {
	if (rle->res != NULL) {
	    device_printf(dev, "duplicate resource for %lx\n", rle->start);
	    continue;
	}

	/* Only memory and IO resources are valid here. */
	switch (rle->type) {
	case SYS_RES_IOPORT:
	    rm = &acpi_rman_io;
	    break;
	case SYS_RES_MEMORY:
	    rm = &acpi_rman_mem;
	    break;
	default:
	    continue;
	}

	/* Pre-allocate resource and add to our rman pool. */
	gparent = device_get_parent(device_get_parent(dev));
	res = BUS_ALLOC_RESOURCE(gparent, dev, rle->type, &rle->rid,
	    rle->start, rle->start + rle->count - 1, rle->count, 0);
	if (res != NULL) {
	    rman_manage_region(rm, rman_get_start(res), rman_get_end(res));
	    rle->res = res;
	}
    }

    return (0);
}

struct resource_list_entry *
acpi_sysres_find(int type, u_long addr)
{
    device_t *devs;
    int i, numdevs;
    struct resource_list *rl;
    struct resource_list_entry *rle;

    /* We only consider IO and memory resources for our pool. */
    rle = NULL;
    if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY)
        return (rle);

    /* Find all the sysresource devices. */
    if (devclass_get_devices(acpi_sysres_devclass, &devs, &numdevs) != 0)
	return (rle);

    /* Check each device for a resource that contains "addr". */
    for (i = 0; i < numdevs && rle == NULL; i++) {
	rl = BUS_GET_RESOURCE_LIST(device_get_parent(devs[i]), devs[i]);
	if (rl == NULL)
	    continue;
	SLIST_FOREACH(rle, rl, link) {
	    if (type == rle->type && addr >= rle->start &&
		addr < rle->start + rle->count)
		break;
	}
    }

    kfree(devs, M_TEMP);
    return (rle);
}
