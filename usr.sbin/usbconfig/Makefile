#
# $FreeBSD: src/usr.sbin/usbconfig/Makefile,v 1.3 2010/01/09 18:59:03 antoine Exp $
#
PROG=	usbconfig
MAN=	usbconfig.8
SRCS=	usbconfig.c dump.c
DPADD+=	${LIBUSB}
LDADD+= -lusb

.include <bsd.prog.mk>
