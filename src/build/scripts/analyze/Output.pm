#
# Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

use strict;
use warnings 'all';

use Carp;
use Cwd;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT_OK = qw(outputAnalysis);

sub outputAnalysis {
    (@_ == 2) or croak "usage: outputAnalysis(ANALYZE, OUTPUT-DIR)";
    my ($analyze, $outputDir) = @_;

    # temporarily switch to output directory
    my $originalWd = getcwd();
    chdir $outputDir;

    outputFunctions($analyze, "Functions.pm");
    outputSummary($analyze, "summary.txt");
    outputRules($analyze, "Rules.pm");

    # gets appended to, so need to clear it beforehand
    truncate "calltree_compacted.txt", 0;

    foreach my $entryFn (sort keys %{$analyze->{INFO}}) {
        outputTree($analyze, $entryFn);
    }

    chdir $originalWd;
}

sub outputFunctions {
    my ($self, $filename, ) = @_;
    my $functions = $self->{FUNCTIONS};

    open(my $fh, ">", $filename) or croak "Error opening $filename: $!";
    select $fh;

    my @sorted = sort {
        my $sortKey = "STACK";
        $functions->{$b}{$sortKey} <=> $functions->{$a}{$sortKey}
    } keys %$functions;

    print "my \$functions = {\n\n";
    foreach my $name (@sorted) {
        my $fn = $functions->{$name};

        print "'$name' => {\n";
        foreach my $prop (qw(ADDR OVERLAY STACK FUNC_PTRS COMPACTED)) {
            next if not exists $fn->{$prop};
            printf "\t%-9s => '%s',\n", $prop, $fn->{$prop};
        }

        # print regular calls
        print "\tCALLS     => [";
        if (@{$fn->{CALLS}} > 0) {
            print "\n";
            print join(",\n", map("\t\t'$_'", @{$fn->{CALLS}}));
            print "\n\t";
        }
        print "],\n";

        # print function-pointer calls
        if ($fn->{FP_CALLS}) {
            print "\tFP_CALLS  => {\n";
            while (my ($addr, $targets) = each %{$fn->{FP_CALLS}}) {
                print "\t\t'$addr' => [\n";
                print join(",\n", map("\t\t\t'$_'", @$targets));
                print "\n\t\t],\n";
            }
            print "\t},\n";
        }

        print "},\n";
    }
    print "\n};\n";
    select STDOUT;
    close($fh);
}

sub outputRules {
    my ($self, $filename) = @_;

    open(my $fh, ">", $filename) or die "Error opening $filename: $!";
    select $fh;

    print "my \$rules = {\n";
    foreach my $task (sort keys %{$self->{TASKS}}) {
        my $rules = $self->{INFO}{$task}{OVL_RULES};
        print "\t'$task' => [\n";
        foreach my $rule (@$rules) {
            print "\t\t[", join(", ", map("'$_'", @$rule)), "],\n";
        }
        print "\t],\n";
    }
    print "}\n";

    close($fh);
    select STDOUT;
}

sub outputTree {
    my ($self, $entryFn) = @_;
    my $callTree = $self->{INFO}{$entryFn}{CALL_TREE};

    my $isCompactedTree = $self->{INFO}{$entryFn}{COMPACTED};

    my ($filename, $mode);
    if ($isCompactedTree) {
        $filename = "calltree_compacted.txt";
        $mode = ">>";
    } else {
        $filename = "calltree_$entryFn.txt";
        $mode = ">";
    }
    open(my $fh, $mode, $filename) or croak "Error opening $filename: $!";
    select $fh;

    foreach my $entry (@$callTree) {
        my ($name, $depth, $stack) = @$entry;

        print "|  " x $depth;
        if ($name eq "CYCLE") {
            $name = $stack;
            print "CYCLE: $name\n";
        } else {
            my $fn = $self->{FUNCTIONS}{$name};
            my $lwrrFnStack = $fn->{STACK};

            if ($self->{ARCH} eq 'RISCV' && exists $fn->{COMPACTED_USER_MODE_ONLY_STACK} &&
                !$self->isFunctionKernelMode($entryFn) && !$isCompactedTree) {
                # If we're in user-mode, use user-mode-only size versions for all compacted functions in the tree (unless
                # this is the compacted tree, where we want to see the full picture).
                $lwrrFnStack = $fn->{COMPACTED_USER_MODE_ONLY_STACK};
            }

            my $extraStack = $fn->{EXTRA_STACK};
            my $origStack = $fn->{ORIG_STACK};

            if ($fn->{COMPACTED} && $isCompactedTree && $name eq $entryFn) {
                # Don't print -COMPACTED for the entry function if we're printing the compacted tree
                print $name;

                # Use the stack size of the entry fn and not the whole stack of the compacted tree!
                $lwrrFnStack = $fn->{COMPACTED_ENTRY_STACK};

                $extraStack = $fn->{COMPACTED_ENTRY_EXTRA_STACK};
                $origStack = $fn->{COMPACTED_ENTRY_ORIG_STACK};
            } else {
                if ($fn->{COMPACTED}) {
                    # For non-entry compacted functions, do not print cycle info
                    $extraStack = undef;
                }

                print $fn->{NAME};
            }

            if (!$isCompactedTree)
            {
                my $addr = $self->{FUNCTIONS}->{$name}->{ADDR};
                print " [$addr]";
            }
            print "  [";
            if ($extraStack) {
                printf "%d=%d+%s", $lwrrFnStack, $origStack, $extraStack;
            } else {
                print $lwrrFnStack;
            }

            print ", $stack, $fn->{OVERLAY}]\n";
        }
    }

    print "=== End of $entryFn ===\n";

    my $si = $self->getStackInfo($entryFn);
    if ($isCompactedTree) {
        # don't print adjustment for compacted tree
        printf "Max stack usage: %d bytes\n", $si->{USED};
    } else {
        printf "Max stack usage: %d%+d=%d bytes\n",
            $si->{USED}, $si->{ADJUST}, $si->{TOTAL_USED};
    }
    map { print "\t$_\n" } @{$si->{MAX_STACK_PATHS}};

    if ($isCompactedTree) {
        # MMINTS-TODO: figure out a way to get MAX_STACK_PATHS to work for this too. For now size is enough
        if ($self->{ARCH} eq 'RISCV' && exists $self->{FUNCTIONS}{$entryFn}{COMPACTED_USER_MODE_ONLY_STACK}) {
            printf "Max stack usage (user-mode only): %d bytes\n", $self->{FUNCTIONS}{$entryFn}{COMPACTED_USER_MODE_ONLY_STACK};
        }
    }

    print "\n\n";

    select STDOUT;
    close($fh);
}

sub outputSummary {
    my ($self, $filename) = @_;

    open(my $fh, ">", $filename) or die "Error opening $filename: $!";
    select $fh;

    print "Ucode analysis summary for $self->{OPMODE}-$self->{BUILD}\n";
    print "(Full output in _out/$self->{BUILD}/_analysis/)\n\n";

    $self->printStackTable();
    $self->printOverflows();
    $self->printCycles();
    $self->printUnreached();
    $self->printStackProtectionTable();
    $self->printStackProtectionDetails();

    print "\n";
    select STDOUT;
    close($fh);
}

sub printStackProtectionDetails {
    my $self = shift;
    printHeading("Stack Smashing Protection Details");
    my $functions = $self->{FUNCTIONS};

    foreach my $entryFn ($self->getRtosRoots(), sort keys %{$self->{TASKS}}) {
        my $si = $self->getStackInfo($entryFn);

        foreach my $name (sort @{$si->{PROTECTED_FUNCS}}) {
            printf "%-10s    ", $entryFn;
            print $name . ",";
            print "\n";
        }

        print "\n";
    }
}

sub printStackProtectionTable {
    my $self = shift;
    my $functions = $self->{FUNCTIONS};

    printHeading("Stack Smashing Protection Summary");
    printf "# Func Protected    Task";
    print "\n\n";

    foreach my $entryFn ($self->getRtosRoots(), sort keys %{$self->{TASKS}}) {
        my $si = $self->getStackInfo($entryFn);
        my $numProtectedFuncs = 0;
        if (defined $si->{PROTECTED_FUNCS}) {
            $numProtectedFuncs = scalar @{$si->{PROTECTED_FUNCS}};
        }
        printf "       %4d          "x1, $numProtectedFuncs;
        print "$entryFn";
        print "\n";
    }
}

sub printStackTable {
    my $self = shift;
    printHeading("Stack info");
    printf "%-8s"x4, qw(Decl Used Remain Task);
    print "\n\n";

    my $haveCycles;
    foreach my $fn ($self->getRtosRoots(), sort keys %{$self->{TASKS}}) {
        my $si = $self->getStackInfo($fn);
        printf "%4d    "x3, $si->{DECL}, $si->{TOTAL_USED}, $si->{DIFF};
        print "$fn";
        if (@{$self->{INFO}{$fn}{CYCLES}} > 0) {
            print " (!)";
            $haveCycles = 1;
        }
        print "\n";
    }

    my $intOverhead = $self->{CONF}{RTOS_PARAMS}{$self->{ARCH}}{INTERRUPT_OVERHEAD};
    print "\n", <<EOF;
Stack usage adjustments:
  +$intOverhead to all tasks for interrupt/exception overhead
  -$intOverhead from interrupt/exception handlers
  no adjustment to start
EOF

    if ($haveCycles) {
        print "\n(!) task contains a cycle, so usage may be underestimated\n";
    }
}

sub printOverflows {
    my $self = shift;
    printHeading("Stack overflows");
    my $count = 0;
    foreach my $task (sort keys %{$self->{TASKS}}) {
        my $overflows = $self->{INFO}{$task}{OVERFLOWS};
        if (@$overflows > 0) {
            print "$task:\n";
            foreach my $oflow (@$overflows) {
                my ($path, $stack) = @$oflow;
                printf "%8d    %s\n", $stack, $path;
            }
            $count++;
        }
    }
    print "none\n" if $count == 0;
}

sub printCycles {
    my $self = shift;
    printHeading("Cycles");
    my $count = 0;
    foreach my $task (sort keys %{$self->{TASKS}}) {
        my $cycles = $self->{INFO}{$task}{CYCLES};
        next if 0 == @$cycles;
        print "$task:\n";
        foreach my $cycle (sort @$cycles) {
            my $stack = $self->{CYCLE_STACKS}{$cycle};
            printf "%8d    %s\n", $stack, $cycle;
            $count++;
        }
    }
    print "none\n" if $count == 0;
}

sub printUnreached {
    my $self = shift;
    printHeading("Unreached functions");
    foreach my $fn (sort @{$self->{UNREACHED}}) {
        print $fn;
        if (0 == grep($_ eq $fn, @{$self->{CONF}{UNREACHED}})) {
            print "    ( UNEXPECTED !!! )";
        }
        print "\n";
    }
    print "none\n" if @{$self->{UNREACHED}} == 0;
}

sub printHeading {
    my ($label, ) = @_;
    my $nEquals = 3;
    print "\n", "="x$nEquals, " $label ", "="x$nEquals, "\n";
}
