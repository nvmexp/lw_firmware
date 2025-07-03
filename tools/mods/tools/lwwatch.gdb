define lwll
end

document lwll
    Used to load the lwwatch shared library in sim MODS.
    It's a noop in mfg MODS, since mfg MODS links with lwwatch statically.
end

define lwsafe
    echo You are at a safe point.\n\n
end

document lwsafe
    It's a noop in mfg MODS, because mfg MODS runs only on hardware.
end

define lw
    set $check = 0

    if $arg0 == diag
        set $check = 1
        call diag("")
    end

    if $arg0 == clocks
        set $check = 1
        call clocks("")
    end

    if $arg0 == cntrfreq
        set $check = 1
        call cntrfreq("$arg1")
    end

    if $arg0 == fifoinfo
        set $check = 1
        call fifoinfo("")
    end

    if $arg0 == grinfo
        set $check = 1
        call grinfo("")
    end

    if $arg0 == grstatus
        set $check = 1
        call grstatus("")
    end

    if $arg0 == grctx
        set $check = 1
        call grctx("$arg1")
    end

    if $arg0 == help
        set $check = 1
        call printHelpMenu()
    end

    if $arg0 == init
        set $check = 1
        call init("")
    end

    if $arg0 == pb
        set $check = 1
        call pb("")
    end

    if $arg0 == ptov
        set $check = 1
        call ptov("$arg1")
    end

    if $arg0 == vtop
        set $check = 1
        call vtop("$arg1")
    end

    if $arg0 == fbstate
        set $check = 1
        call fbstate("")
    end

    if $arg0 == ptevalidate
        set $check = 1
        call ptevalidate("")
    end

    if $arg0 == hoststate
        set $check = 1
        call hoststate("")
    end

    if $arg0 == grstate
        set $check = 1
        call grstate("")
    end

    if $arg0 == msdecstate
        set $check = 1
        call msdecstate("")
    end

    if $arg0 == elpgstate
        set $check = 1
        call elpgstate("")
    end

    if $arg0 == gpuanalyze
        set $check = 1
        call gpuanalyze("")
    end

    if $arg0 == zbc
        set $check = 1
        call zbc("$arg1")
    end

    if $arg0 == vic
        set $check = 1
        call vic("")
    end

    if $arg0 == msenc
        set $check = 1
        call msenc("")
    end

    if $arg0 == rd
        set $check = 1
        call rd("$arg1")
    end

    if $arg0 == wr
        set $check = 1
        call wr("$arg1 $arg2")
    end

    if $check == 0
        echo lwwatch: no lw helper, passing arg1 to function...\n
        # No helper call direct assuming a string...
        call $arg0($arg1)
    end
end

document lw
    Used to issue lwwatch commands. Works only for some commands.
    Usage: lw <command> <arguments>
end

define lwv
    if $arg1 == 0
        call $arg0("")
    end

    if $arg1 == 1
        call $arg0("$arg2")
    end

    if $arg1 == 2
        call $arg0("$arg2 $arg3")
    end

    if $arg1 == 3
        call $arg0("$arg2 $arg3 $arg4")
    end

    if $arg1 == 4
        call $arg0("$arg2 $arg3 $arg4 $arg5")
    end

    if $arg1 == 5
        call $arg0("$arg2 $arg3 $arg4 $arg5 $arg6")
    end
end

document lwv
    Used to issue lwwatch commands. Needs number of arguments and list of arguments.
    Usage: lwv <command> <number of arguments> <arguments>
end

define lws
    call $arg0($arg1)
end

document lws
    Used to issue lwwatch commands. Needs all the arguments to be put in quotes.
    Usage: lws <command> "<arguments>"
end
