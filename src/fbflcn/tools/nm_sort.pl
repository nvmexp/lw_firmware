#!/home/gnu/bin/perl

# little helper program to sort all functions in the nm file, showing size of each function.
# A diff of the object dump is sometimes insufficient to find differences between 2 binaries. This tool gives a bit better locality to those differences
# Author: Stefan Scotznivovsky (stefans@lwpu.com)
#
# use:  nm_sort.pl <nm file>
#

my ($filename) = @ARGV;

open(FH,'<', $filename) or die $!;
while(<FH>) {
  if (m/^([0-9A-Fa-f]+)\s([0-9A-Fa-f]+)\s[T]\s(.*)$/) {
    $field{${3}} = ${2};
  }
}

foreach my $name (sort keys %field) {
  printf "%-44s %s\n", $name, $field{$name};
}
