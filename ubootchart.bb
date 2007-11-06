DESCRIPTION = "A boot profiling tool"
HOMEPAGE = "http://code.google.com/p/ubootchart/"
LICENSE="GPLv3"
PR = "r19"

SRC_URI = "file://ubootchartd_bin.c \
            file://ubootchartd \
	    file://ubootchart.conf \
            file://start.sh \
            file://finish.sh"

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
}

