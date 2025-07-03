use strict;
use warnings;
use String::CRC32;

open my $fh, '<', $ARGV[0] or die $!;
binmode $fh;
my $crc = crc32($fh);
my $hex = sprintf ("0x%X\n", $crc);
print $hex;
close $fh;
