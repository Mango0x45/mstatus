.POSIX:

PREFIX  = /usr
DPREFIX = ${DESTDIR}${PREFIX}
MANDIR  = ${DPREFIX}/share/man

all: mast
mast: mast.go
	go build

install:
	mkdir -p ${DPREFIX}/bin ${MANDIR}/man1
	cp mast ${DPREFIX}/bin
	cp mast.1 ${MANDIR}/man1

clean:
	rm -f mast
