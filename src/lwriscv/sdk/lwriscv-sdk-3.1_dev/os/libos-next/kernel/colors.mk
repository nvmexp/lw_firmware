COLOR?=1

ifeq ($(COLOR),1)
COLOR_DEFAULT=\e[39m
COLOR_RED_BRIGHT=\e[91m
COLOR_GREEN_BRIGHT=\e[92m
COLOR_RED=\e[31m
COLOR_GREEN=\e[32m
COLOR_BOLD=\e[1m
COLOR_RESET=\e[0m
else
COLOR_DEFAULT=
COLOR_RED_BRIGHT=
COLOR_GREEN_BRIGHT=
COLOR_RED=
COLOR_GREEN=
COLOR_BOLD=
COLOR_RESET=
endif

define ANNOUNCE_FAIL
$(COLOR_RED)                       
    #######    #    ### #		
    #         # #    #  #		
    #        #   #   #  #		
    #####   #     #  #  #		
    #       #######  #  #		
    #       #     #  #  #		
    #       #     # ### #######	

Consider running ./regolden.sh to regolden the test results

endef
export ANNOUNCE_FAIL

define ANNOUNCE_PASS
$(COLOR_GREEN)
    ######     #     #####   #####
    #     #   # #   #     # #     #
    #     #  #   #  #       #
    ######  #     #  #####   #####
    #       #######       #       #
    #       #     # #     # #     #
    #       #     #  #####   #####

endef
export ANNOUNCE_PASS