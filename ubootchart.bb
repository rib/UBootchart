DESCRIPTION = "uBootChart a boot profiling tool"
PR = "r14"

SRC_URI = "file://ubootchartd_bin.c \
            file://ubootchartd \
	    file://ubootchart.conf \
            file://start.sh \
            file://finish.sh \
            file://README.txt"

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

