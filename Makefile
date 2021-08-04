.POSIX:

target  = mstatus

CC	= cc
CFLAGS	= -O3 -std=c11 -pedantic -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes
LDFLAGS	= -lX11

PREFIX	= /usr/local

all: ${target}
${target}: mstatus.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $<

.PHONY: clean install uninstall
clean:
	rm -f ${target} ${objects}

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin ${DESTDIR}${PREFIX}/share/man/man1
	cp -f ${target} ${DESTDIR}${PREFIX}/bin
	cp -f ${target}.1 ${DESTDIR}${PREFIX}/share/man/man1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${target} ${DESTDIR}${PREFIX}/share/man/man1/${target}.1
