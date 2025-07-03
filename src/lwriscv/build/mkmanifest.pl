# placeholder for manifest script. For now, generates dummy fata for testing purpose

use strict;
use warnings "all";

use IO::File;
use Getopt::Long;
use File::Basename;
use File::Spec;

my %opt;
my $g_usageExample = 'perl '.basename($0).' --itcmSize <size of ITCM> --dtcmSize <size of DTCM> --pdi <PDI info> ';


sub usage {

    print <<__USAGE__;

Usage:

   $g_usageExample

Options:

  --help                : Help message
  --itcmSize            : ITCM size
  --dtcmSize            : DTCM size
  --pdi                 : PDI info

__USAGE__

    exit 1;
}

sub parseArgs {
    my %opt_raw;

    Getopt::Long::Configure("bundling", "no_ignore_case");

    GetOptions (\%opt_raw,
        "h|help"                    => \$opt{help},
        "itcmSize|i=s"              => \$opt{imemSize},
        "dtcmSize|d=s"              => \$opt{dmemSize},
        "pdi=s"                     => \$opt{pdiInfo},
        "outDir=s"                  => \$opt{outDir},
        "outFilePrefix=s"           => \$opt{outFilePrefix}
    )
    or usage();
}

sub generateManifest {

    my $imem    = shift;
    my $dmem    = shift;
    my $pdi     = shift;
    my $outDir  = shift;

    my $outImagePath = File::Spec->catfile($opt{outDir},
                                           $opt{outFilePrefix}.'_data.bin');

    open IMAGE_FILE, ">$outImagePath"   or die("Can't open output file: $outImagePath");
    binmode IMAGE_FILE;

    print IMAGE_FILE pack("C", 0x41 ) x 0x2000;

    close IMAGE_FILE;

    signManifest();
}

sub signManifest {
    my $outImagePath = File::Spec->catfile($opt{outDir},
                                           $opt{outFilePrefix}.'.out');
    my $inputImagePath = File::Spec->catfile($opt{outDir},
                                           $opt{outFilePrefix}.'_data.bin');


    open IMAGE_FILE, ">$outImagePath"   or die("Can't open output file: $outImagePath");

    binmode IMAGE_FILE;

    print IMAGE_FILE pack("C", 0x43 ) x 0x2000;

    close IMAGE_FILE;

}
parseArgs();
generateManifest($opt{imemSize}, $opt{dmemSize}, $opt{pdiInfo});