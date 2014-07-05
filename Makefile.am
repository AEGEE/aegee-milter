ACLOCAL_AMFLAGS=-I m4
AM_LDFLAGS= ${CFLAG_VISIBILITY}
EXTRA_DIST = contrib/sm8.14.3-prdr.diff
AM_CPPFLAGS =
AM_CFLAGS = -Wall -Wextra ${glib_CFLAGS} ${libiodbc_CFLAGS} -DSYSCONFDIR='"${sysconfdir}"' ${LTDLINCL} ${liblistserv_CFLAGS} ${libsieve_CFLAGS} ${libcyrus_sieve_CFLAGS} ${gmime_CFLAGS}
AM_CXXFLAGS = ${AM_CFLAGS} -std=gnu++11

pkglib_LTLIBRARIES = 
noinst_LTLIBRARIES=
EXTRA_LTLIBRARIES =
lib_LTLIBRARIES = 
MOSTLYCLEANFILES = 
CLEANFILES =
BUILT_SOURCES = 
include_HEADERS = 
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

#info_TEXINFOS = doc/prdr-milter.texi
#TEXINFO_TEX = doc/texinfo.tex
#EXTRA_DIST = doc/prdr-milter.texi.orig

sbin_PROGRAMS = aegee-milter

aegee_milter_SOURCES = src/prdr-milter.h src/prdr-list.h \
	src/core/aegee-milter.cpp src/core/smfi-operations.cpp \
	src/core/AegeeMilter.cpp src/core/AegeeMilter.hpp \
	src/core/Message.cpp src/core/Message.hpp \
	src/core/Module.hpp src/core/Module.cpp \
	src/core/Privdata.cpp src/core/Privdata.hpp \
	src/core/Recipient.cpp src/core/Recipient.hpp \
	src/core/SoList.cpp src/core/SoList.hpp \
	src/core/SoModule.cpp src/core/SoModule.hpp
aegee_milter_LDFLAGS = ${glib_LIBS} $(LIBLTDL) -lmilter ${gmime_LIBS}
aegee_milter_LDADD =
# LISTS

#... = list_simple.la
#aegee_prdr_LDADD += -dlpreopen src/lists/simple.la
#src_lists_simple_la_LIBADD = ${libconfig_LIBS}
#src_lists_simple_la_CFLAGS = ${libconfig_CFLAGS}
#src_lists_simple_la_SOURCES = src/lists/simple.cpp
#src_lists_simple_la_LFDLAGS = -module -avoid-version -no-undefined -static

if LIST_LISTSERV
aegee_milter_LDADD += -dlpreopen src/lists/list_listserv.la
pkglib_LTLIBRARIES += src/lists/list_listserv.la
endif

if LIST_LOG
aegee_milter_LDADD += -dlpreopen src/lists/list_log.la
pkglib_LTLIBRARIES += src/lists/list_log.la
endif

if LIST_MEMCACHED
aegee_milter_LDADD += -dlpreopen src/lists/list_memcached.la
pkglib_LTLIBRARIES += src/lists/list_memcached.la
endif

if LIST_ODBC
aegee_milter_LDADD += -dlpreopen src/lists/list_odbc.la
pkglib_LTLIBRARIES += src/lists/list_odbc.la
endif

if MOD_LIBSIEVE
aegee_milter_LDADD += -dlpreopen src/lists/list_timsieved_scripts.la
pkglib_LTLIBRARIES += src/lists/list_timsieved_scripts.la
endif

#MODULES 

if LIST_LISTSERV
aegee_milter_LDADD += -dlpreopen src/modules/mod_arf.la
pkglib_LTLIBRARIES += src/modules/mod_arf.la
endif
aegee_milter_LDADD += -dlpreopen src/modules/mod_block_sender.la -dlpreopen src/modules/mod_delay.la -dlpreopen src/modules/mod_equal.la  -dlpreopen src/modules/mod_relayed.la -dlpreopen src/modules/mod_sieve.la -dlpreopen src/modules/mod_simple.la -dlpreopen src/modules/mod_via.la -dlpreopen src/modules/mod_x_scc_headers.la
pkglib_LTLIBRARIES += src/modules/mod_block_sender.la src/modules/mod_delay.la  src/modules/mod_equal.la src/modules/mod_relayed.la src/modules/mod_sieve.la src/modules/mod_simple.la src/modules/mod_via.la src/modules/mod_x_scc_headers.la

src_lists_list_listserv_la_LDFLAGS = -module -avoid-version -no-undefined -static
src_lists_list_listserv_la_LIBADD = ${liblistserv_LIBS} ${glib_LIBS}
src_lists_list_listserv_la_SOURCES = src/lists/list_listserv.cpp

src_lists_list_log_la_LDFLAGS = -module -avoid-version -no-undefined -static
src_lists_list_log_la_LIBADD = ${glib_LIBS} ${libiodbc_LIBS}
src_lists_list_log_la_SOURCES = src/lists/list_log.cpp

src_lists_list_memcached_la_LDFLAGS = -module -avoid-version -no-undefined -static
src_lists_list_memcached_la_LIBADD = ${libmemcached_LIBS}
src_lists_list_memcached_la_SOURCES = src/lists/list_memcached.cpp

src_lists_list_odbc_la_LIBADD = ${glib_LIBS} ${libiodbc_LIBS}
src_lists_list_odbc_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_lists_list_timsieved_scripts_la_LDFLAGS = -module -avoid-version -no-undefined -static
src_lists_list_timsieved_scripts_la_LIBADD = ${glib_LIBS}
src_lists_list_timsieved_scripts_la_SOURCES = src/lists/list_timsieved_scripts.cpp

src_modules_mod_arf_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_arf_la_LIBADD = ${liblistserv_LIBS} ${glib_LIBS} ${gmime_LIBS}
src_modules_mod_arf_la_SOURCES = src/modules/mod_arf.cpp

src_modules_mod_block_sender_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_block_sender_la_SOURCES = src/modules/mod_block_sender.cpp

src_modules_mod_delay_la_SOURCES = src/modules/mod_delay.cpp
src_modules_mod_delay_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_delay_la_LIBADD = ${glib_LIBS}

src_modules_mod_equal_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_equal_la_LIBADD = ${glib_LIBS}
src_modules_mod_equal_la_SOURCES = src/modules/mod_equal.cpp

src_modules_mod_relayed_la_LIBADD = ${glib_LIBS}
src_modules_mod_relayed_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_relayed_la_SOURCES = src/modules/mod_relayed.cpp

src_modules_mod_sieve_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_sieve_la_LIBADD = ${glib_LIBS}
src_modules_mod_sieve_la_SOURCES = src/modules/mod_sieve/core.cpp src/modules/mod_sieve/message.hpp

if MOD_CYRUS_SIEVE
src_modules_mod_sieve_la_LIBADD += ${libcyrus_sieve_LIBS}
src_modules_mod_sieve_la_SOURCES += src/modules/mod_sieve/libcyrus_sieve.cpp
endif

if MOD_LIBSIEVE
src_modules_mod_sieve_la_LIBADD += ${libsieve_LIBS}
src_modules_mod_sieve_la_SOURCES += src/modules/mod_sieve/libsieve.cpp
endif

src_modules_mod_simple_la_LIBADD = ${glib_LIBS}
src_modules_mod_simple_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_simple_la_CFLAGS =
src_modules_mod_simple_la_SOURCES = src/modules/mod_simple.cpp


#src/modules/mod_spamassassin.la 
src_modules_mod_via_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_via_la_LIBADD = ${glib_LIBS}
src_modules_mod_via_la_SOURCES = src/modules/mod_via.cpp

src_modules_mod_x_scc_headers_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_x_scc_headers_la_LIBADD = ${glib_LIBS}
src_modules_mod_x_scc_headers_la_SOURCES = src/modules/mod_x_scc_headers.cpp

include libltdl/ltdl.mk

install-exec-hook:
	rm -r ${pkglibdir}
# killall -SIGHUP aegee-prdr
