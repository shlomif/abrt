# ABRTUtils has small set of deps. This reduces deps of smaller abrt binaries
# ABRTdUtils has much more. It is used by daemon and plugins only
lib_LTLIBRARIES = libABRTUtils.la libABRTdUtils.la

# Not used just yet:
# time.cpp
# xconnect.cpp

NULL =

# removed: CrashTypesSocket.cpp
libABRTUtils_la_SOURCES = \
    xfuncs.cpp \
    encbase64.cpp \
    read_write.cpp \
    logging.cpp \
    copyfd.cpp \
    daemon.cpp \
    skip_whitespace.cpp \
    xatonum.cpp numtoa.cpp \
    spawn.cpp \
    stringops.cpp \
    dirsize.cpp \
    DebugDump.h DebugDump.cpp \
    CrashTypes.cpp \
    ABRTException.cpp \
	$(NULL)

libABRTUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
	$(NULL)

libABRTUtils_la_LDFLAGS = \
    -version-info 0:1:0 \
    $(DL_LIBS) \
	$(NULL)

libABRTUtils_la_LIBADD =


libABRTdUtils_la_SOURCES = \
    parse_release.cpp \
    make_descr.cpp \
    CommLayerInner.h CommLayerInner.cpp \
    abrt_dbus.h abrt_dbus.cpp \
    abrt_xmlrpc.h abrt_xmlrpc.cpp \
    Plugin.h Plugin.cpp \
    Polkit.h Polkit.cpp \
	abrt-debuginfo-install-pk.h abrt-debuginfo-install-pk.cpp \
    Action.h Database.h Reporter.h Analyzer.h \
    Observer.h \
    DBusCommon.h \
	$(NULL)

libABRTdUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE \
	$(PACKAGEKIT_GLIB2_CFLAGS) -DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE \
	$(NULL)

libABRTdUtils_la_LDFLAGS = \
	$(PACKAGEKIT_GLIB2_LIBS) \
    -version-info 0:1:0 \
    $(DL_LIBS) \
    $(DBUS_LIBS) \
	$(NULL)

libABRTdUtils_la_LIBADD = \
	$(PACKAGEKIT_GLIB2_LIBS) \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS) \
    $(POLKIT_LIBS) \
	$(NULL)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
