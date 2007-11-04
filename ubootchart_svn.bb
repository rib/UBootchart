# NOTE I haven't had this work for me yet, but it's a start.

DESCRIPTION = "A boot profiling tool"
HOMEPAGE = "http://code.google.com/p/ubootchart/"
LICENSE="GPLv3"
PV = "0.1.0+svnr${SRCREV}"
PR = "r0"

SRC_URI="svn://ubootchart.googlecode.com/svn/;proto=http;module=trunk"
S = "${WORKDIR}/trunk"

do_compile() {
        ${CC} ${CFLAGS} ${LDFLAGS} ${LIBS} ${INCLUDES} ${WORKDIR}/ubootchartd_bin.c -o ubootchartd_bin
}

do_install() {
        install -m 0755 -d ${D}/sbin ${D}/etc/ubootchart ${D}${docdir}/ubootchart
        install -m 0755 ${S}/ubootchartd_bin ${D}/sbin
        install -m 0755 ${WORKDIR}/ubootchartd ${D}/sbin
        install -m 0644 ${WORKDIR}/ubootchart.conf ${D}/etc/ubootchart
        install -m 0755 ${WORKDIR}/start.sh ${D}/etc/ubootchart
        install -m 0755 ${WORKDIR}/finish.sh ${D}/etc/ubootchart
        install -m 0644 ${WORKDIR}/README.txt ${D}${docdir}/ubootchart
}

