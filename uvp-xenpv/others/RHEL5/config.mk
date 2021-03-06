######################### TOP config.mk begin #################################
# Hack: we need to use the config which was used to build the kernel,
# except that that won't have the right headers etc., so duplicate
# some of the mach-xen infrastructure in here.
#
# (i.e. we need the native config for things like -mregparm, but
# a Xen kernel to find the right headers)
_XEN_CPPFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
_XEN_CPPFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
_XEN_CPPFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H
KERNELRELEASE := $(shell echo $(KERNDIR) | grep "2.6.18-194.el5\|2.6.18-8\|2.6.18-SKL\|2.6.18-1.2798")
KERNELRELEASE_DISK := $(shell echo $(KERNDIR) | grep 2.6.18-128)
AUTOCONF := $(shell find $(CROSS_COMPILE_KERNELSOURCE) -name autoconf.h)

ifneq ($(KERNELRELEASE), )
  _XEN_CPPFLAGS += -DCUSTOMIZED
endif
ifeq ($(ARCH),ia64)
_XEN_CPPFLAGS += -DCONFIG_VMX_GUEST
endif
ifneq ($(KERNELRELEASE_DISK), )
_XEN_CPPFLAGS += -DDISK_EX_FEATURE
endif

_XEN_CPPFLAGS += -include $(AUTOCONF)

EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
######################### TOP config.mk end ###################################
