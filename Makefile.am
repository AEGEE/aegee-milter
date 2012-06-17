LIBTOOL_DEPS = @LIBTOOL_DEPS@
AM_LDFLAGS=
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = src/prdr-list.h contrib/sm8.14.3-prdr.diff
AM_CPPFLAGS = -I src/
AM_CFLAGS = -Wall -Wextra ${glib_CFLAGS} ${libiodbc_CFLAGS} -DSYSCONFDIR='"${sysconfdir}"' ${LTDLINCL} ${liblistserv_CFLAGS} ${libsieve_CFLAGS} ${gthread_CFLAGS}

pkglib_LTLIBRARIES = 
noinst_LTLIBRARIES=
EXTRA_LTLIBRARIES =
lib_LTLIBRARIES = 
MOSTLYCLEANFILES = 
CLEANFILES =
BUILT_SOURCES = 
include_HEADERS = 
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

#info_TEXINFOS = doc/prdr-milter.texi
#TEXINFO_TEX = doc/texinfo.tex
#EXTRA_DIST = doc/prdr-milter.texi.orig

#install-exec-hook: killall -SIGHUP aegee-prdr

sbin_PROGRAMS = src/aegee-milter

src_aegee_milter_SOURCES = src/prdr-milter.h src/aegee-milter.c src/helpers.c src/recipient_api.c src/prdr-list.h src/smfi-operations.c
#src_prdr_milter_DEPENDENCIES = src/LD_DYNAMIC_LIST
src_aegee_milter_LDFLAGS = ${glib_LIBS} ${gthread_LIBS} $(LIBLTDL)
#-Wl,--dynamic-list=LD_DYNAMIC_LIST
src_aegee_milter_LDADD = 
# LISTS

#... = list_simple.la
#src_aegee_prdr_LDADD += -dlpreopen src/lists/simple.la
#src_lists_simple_la_LIBADD = ${libconfig_LIBS}
#src_lists_simple_la_CFLAGS = ${libconfig_CFLAGS}
#src_lists_simple_la_LFDLAGS = -module -avoid-version -no-undefined -static

if LIST_LISTSERV
src_aegee_milter_LDADD += -dlpreopen src/lists/list_listserv.la
pkglib_LTLIBRARIES += src/lists/list_listserv.la
endif

if LIST_LOG
src_aegee_milter_LDADD += -dlpreopen src/lists/list_log.la
pkglib_LTLIBRARIES += src/lists/list_log.la
endif

if LIST_ODBC
src_aegee_milter_LDADD += -dlpreopen src/lists/list_odbc.la
pkglib_LTLIBRARIES += src/lists/list_odbc.la
endif

if MOD_SIEVE
src_aegee_milter_LDADD += -dlpreopen src/lists/list_timsieved_scripts.la
pkglib_LTLIBRARIES += src/lists/list_timsieved_scripts.la
endif

#MODULES 
#EXTRA_DIST += src/prdr-mod.h

#... = mod_simple.la
#mod_simple_la_LIBADD =
#mod_simple_la_CFLAGS =
if LIST_LISTSERV
src_aegee_milter_LDADD += -dlpreopen src/modules/mod_arf_microsoft.la
pkglib_LTLIBRARIES += src/modules/mod_arf_microsoft.la
endif
src_aegee_milter_LDADD += -dlpreopen src/modules/mod_block_sender.la -dlpreopen src/modules/mod_delay.la -dlpreopen src/modules/mod_equal.la  -dlpreopen src/modules/mod_relayed.la -dlpreopen src/modules/mod_simple.la -dlpreopen src/modules/mod_via.la -dlpreopen src/modules/mod_x_scc_headers.la
pkglib_LTLIBRARIES += src/modules/mod_block_sender.la src/modules/mod_delay.la  src/modules/mod_equal.la src/modules/mod_relayed.la

if MOD_SIEVE
src_aegee_milter_LDADD += -dlpreopen src/modules/mod_sieve.la 
pkglib_LTLIBRARIES += src/modules/mod_sieve.la
endif

pkglib_LTLIBRARIES += src/modules/mod_simple.la src/modules/mod_via.la src/modules/mod_x_scc_headers.la

src_lists_list_listserv_la_LIBADD = ${liblistserv_LIBS} ${glib_LIBS}
src_lists_list_listserv_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_lists_list_log_la_LIBADD = ${glib_LIBS} ${libiodbc_LIBS}
src_lists_list_log_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_lists_list_odbc_la_LIBADD = ${glib_LIBS} ${libiodbc_LIBS}
src_lists_list_odbc_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_lists_list_timsieved_scripts_la_LIBADD = ${glib_LIBS}
src_lists_list_timsieved_scripts_la_LDFLAGS = -module -avoid-version -no-undefined -static

src_modules_mod_arf_microsoft_la_LIBADD = ${liblistserv_LIBS} ${glib_LIBS}
src_modules_mod_arf_microsoft_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_block_sender_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_delay_la_LIBADD = ${glib_LIBS}
src_modules_mod_delay_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_equal_la_LIBADD = ${glib_LIBS}
src_modules_mod_equal_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_relayed_la_LIBADD = ${glib_LIBS}
src_modules_mod_relayed_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_sieve_la_LIBADD = ${libsieve_LIBS} ${glib_LIBS} ${gthread_LIBS}
src_modules_mod_sieve_la_LDFLAGS = -module -avoid-version -static
src_modules_mod_sieve_la_SOURCES = src/modules/mod_sieve/core.c src/modules/mod_sieve/libsieve.c src/modules/mod_sieve/message.h

src_modules_mod_simple_la_LIBADD = ${glib_LIBS}
src_modules_mod_simple_la_LDFLAGS = -module -avoid-version -static

#src/modules/mod_spamassassin.la 
src_modules_mod_via_la_LIBADD = ${glib_LIBS}
src_modules_mod_via_la_LDFLAGS = -module -avoid-version -static

src_modules_mod_x_scc_headers_la_LIBADD = ${glib_LIBS}
src_modules_mod_x_scc_headers_la_LDFLAGS = -module -avoid-version -static

include libltdl/Makefile.inc

install-exec-hook:
	rm -r ${pkglibdir}
