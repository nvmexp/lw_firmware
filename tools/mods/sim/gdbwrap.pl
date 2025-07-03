#!/usr/bin/elw perl

my $ddd = 0 ;
my $gdbserver = 0;

# This would be much easier if our GDB supported --args
if ($ARGV[0] eq '-tstStream') {
    $modsArgs = $ARGV[0] . ' ' . $ARGV[1] . ' ';
    shift(@ARGV); shift(@ARGV);
}
if ($ARGV[0] eq '-ddd') { 
    $ddd = 1;
    shift(@ARGV);
}
if ($ARGV[0] eq '-gdbserver') {
    $gdbserver = 1;
    $gdbserverHostPort = $ARGV[1];
    shift(@ARGV); shift(@ARGV);
}
$modsProg = $ARGV[0];
shift(@ARGV);
$modsArgs .= '"' . join('" "', @ARGV) . '"';
open(DISPFILE, "> dbcommands") || die "can't open dbcommands file\n";
print DISPFILE "set args $modsArgs";
close DISPFILE;

my $real_LD_LIBRARY_PATH = `$ELW{SHELL} -c \'echo \$LD_LIBRARY_PATH\'`;
chomp $real_LD_LIBRARY_PATH;
if ($ELW{LD_LIBRARY_PATH} ne $real_LD_LIBRARY_PATH) {
    print "-------------------------------------------------------------------\n";
    print "GDB may fail to find your shared libraries\n";
    print "Your LD_LIBRARY_PATH=$ELW{LD_LIBRARY_PATH}\nGDB will search .so in $real_LD_LIBRARY_PATH\n";
    print "\nFrom From http://sources.redhat.com/gdb/current/onlinedocs/gdb.html#Environment:\n";
    print "Warning: On Unix systems, GDB runs your program using the shell indicated by your SHELL environment variable if it exists (or /bin/sh if not). If your SHELL variable names a shell that runs an initialization file such as .cshrc for C-shell, or .bashrc for BASH any variables you set in that file affect your program. You may wish to move setting of environment variables to files that are only run when you sign on, such as .login or .profile.\n\n";
    print "Consider re-arranging your initialization scripts or running your command with: \n";
    print "  elw SHELL=/bin/sh <your command>\n";
    print "-------------------------------------------------------------------\n";
}

my $gdbProg = "gdb";
my $gdbserverProg = "gdbserver";
my $gdbPath = $ELW{MODS_GDB_PATH};
if (! -e "$gdbPath/gdb") {
    $gdbPath = "/home/utils/gdb-10.1/bin";
}
if (-e "$gdbPath/gdb") {
    $gdbProg = "$gdbPath/$gdbProg";
    $gdbserverProg = "$gdbPath/$gdbserverProg";
}

if ($ddd) {
    system("ddd --debugger gdb --gdb $modsProg --command=dbcommands");
} elsif ($gdbserver){
    system("$gdbserverProg $gdbserverHostPort $modsProg $modsArgs");
} else {
    system("$gdbProg $modsProg --command=dbcommands");
}
