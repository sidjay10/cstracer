include $(top_srcdir)/Makefile.tool.am

EXTRA_DIST = docs/ct-manual.xml
#----------------------------------------------------------------------------
# Headers, etc
#----------------------------------------------------------------------------

noinst_HEADERS = \
		 arm64regs.h \
		 x86-64regs.h

#----------------------------------------------------------------------------
# cstracer-<platform>
#----------------------------------------------------------------------------

noinst_PROGRAMS  = cstracer-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += cstracer-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

CSTRACER_SOURCES_COMMON = \
	ct_main.c 

cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(CSTRACER_SOURCES_COMMON)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(cstracer_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(CSTRACER_SOURCES_COMMON)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(cstracer_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif

