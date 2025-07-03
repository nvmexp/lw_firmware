use strict;
use File::Spec;

if (scalar(@ARGV) != 3) {
    print "Usage: cl <args> | perl getdeps.pl <srcfile> <objfile> <depfile>\n";
    exit(1);
}

my $srcfile = $ARGV[0];
$srcfile =~ s:\\:/:g;
my $objfile = $ARGV[1];
$objfile =~ s:\\:/:g;
my $depfile = $ARGV[2];

my $compilerErrorCount = 0;

# We use a list in addition to a hash to preserve the order of dependencies
my @deps = ();
my %deps_hash;

while (<STDIN>) {
    if (m/Note: including file:[ ]+(.*)$/) {
        my $dependency = File::Spec->rel2abs($1);
        $dependency =~ s:\\:/:g;
        $dependency =~ tr/[A-Z]/[a-z]/;
        $dependency =~ s/://;
        $dependency = "/cygdrive/$dependency";
        if ($deps_hash{$dependency} != 1) {
            $deps_hash{$dependency} = 1;
            push(@deps, $dependency);
        }
#        print $_;
    } elsif (m/ error /) {
		$compilerErrorCount++;
        print $_;
	} else {	
        print $_;
    }
}

open(DEPFILE, ">$depfile") || die "Couldn't open $depfile for writing";

# Don't use line continuations or very long lines, since make seems to have
# bugs when dealing with very long makefile lines.
foreach my $dep (@deps) {
    print DEPFILE "$objfile: $dep\n";
}
print DEPFILE "\n";

# Make a fake rule for each dependency so that deleting a header file
# doesn't break the build.
foreach my $dep (@deps) {
    print DEPFILE "$dep:\n";
}
close(DEPFILE);

# It is necessary for this to propage an error return value because under CMD.exe and mingw gmake we
# lose the status value of the call to CL when we pipe the output to perl:
#
# cl file.cpp | perl getdeps.pl 
#     If the compile fails, the return code for the entire command (including the pipe) is still success.
#     This may be a flaw in gmake.
#
# To work around this for now, we count " error " in the compiler stream and use that as the final status.
#
exit $compilerErrorCount;

