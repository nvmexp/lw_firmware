objs-y += entry.o
objs-y += main.o
objs-y += cmdmgmt.o
objs-y += lockdown.o
objs-y += dispatcher_acr.o
objs-y += dispatcher_rmproxy.o
objs-y += dispatcher_spdm.o

ifeq ($(CC_IS_RESIDENT),false)
    objs-y += loader_entry.o
    objs-y += loader_main.o
    objs-y += linkages.o
endif
