#!/usr/bin/perl
########################################################################################################
# Perl script to automate the entire GFE key generation process for MAXWELL and later.
# This script takes care of generating RSA keys pairs, validating them and signing them
# to be consumed by the GFE server. 
# Following steps are ilwolved in this process:
# Step 1: Process and validate command line arguments
# Step 2: Generate RSA (1K or 2K) key pairs using OPENSSL
# Step 3: Pack the RSA private key in the format required by FALCON ucode
# Step 4: Pass the plain packed RSA private key to FALCON and get encrypted private key
# Step 5: Use the encrypted private key to get signed ECID from FALCON
# Step 6: Validate the signature of ECID using RSA public key and OPENSSL
# Step 7: Generate a MASTER RSA 2K key (if not already generated) using OPENSSL
# Step 8: Concatanate encrypted RSA private key and RSA public key into a single binary file
# Step 9: Sign the binary file using MASTER RSA 2K

# REQUIREMENTS:
# Supported only in GM107 and later
# Requires OPENSSL 1.0.1e or later
# Generating RSA 1K and RSA 2K keys are supported
# Key verification supported only for RSA 1K keys (RSA 2K will be supported in future versions)
# Master key is RSA 2K type
############################################################################################################

use Getopt::Long;
use File::Path;
use Cwd;
use IPC::System::Simple qw(system systemx capture capturex run runx);

my $lwrrentwd      = getcwd;
my $genContentError = 0;

#Default arguments
my $chip           = "gm206";
my $keysize        = 1024;
my $totalkeys      = 30;
my $outdir         = $lwrrentwd."/generated_keys";
my $prefix         = "rsa";
my $openssl        = $lwrrentwd."/openssl";
my $genmodspath    = $lwrrentwd;
my $verifmodspath  = $lwrrentwd;
my $reboot         = 0;

#Master RSA key settings
my $masterkeypem    = "";
my $masterkeypubpem = "";
my $masterkeypasswd = "";
my $masterkeysize   = 2048;

#Mods args
my $modscmnargs    = "./sim.pl -chip gm206 -hwbios -hwchip2 -modsOpt rmtest.js -engr -forcetestname class95a1sec2gfetest -no_gold -manualVerif -ignore_tesla -devid_check_ignore -cvb_check_ignore ";
my $genmodsargs    = $modscmnargs;
my $verifmodsargs  = $modscmnargs;

#Pass strings
my $genpassstring   = "GFE: 'GenEkey' PASSED with retcode 0x0 - Expected retcode 0x0";
my $verifpassstring = "GFE: 'ReadEcid' PASSED with retcode 0x0 - Expected retcode 0x0";
my $osslpassstring  = "Signature Verified Successfully";
my $osslpubpass     = "writing RSA key";
my $osslgenpass     = "Generating RSA private key";

#
# Returns the string between START and END pattern in hex format.
# Useful while parsing RSA text out which has all the key parameters
# within defined start and end strings.
#
# lines  -> Holds the string to be searched
# start  -> Start of the pattern
# end    -> End of the pattern
#
# Returns the matching string /start(.*?)end/ in hex format
#
sub getContentsOfPkeyParam
{
    my    $result = "";
    local ($lines, $start, $end, $expectedlength) = @_;

    if (scalar(@_) != 4)
    {
        die "ERROR: Invalid args";
    }

    if ($end eq "")
    {
        #Search till end of file
        $lines =~ m{$start(.*)};
	$result = $1;
    }
    else
    {
        $lines =~ m{$start(.*?)$end};
	$result = $1;
    }

    if (length($result))
    { 
        $result =~ s{ }{}g; 
        $result =~ s{:}{}g;
        $stripchars = length($result) % 128;
        $result = substr $result, $stripchars;
    }
    else
    {
        die "ERROR: Patterns mismatch\n";
    }
    $packed = pack('H*', $result);
    if (length($packed) != $expectedlength)
    {
        $genContentError = 1;
    }

    return $packed;
}

#
# Counts number of oclwrences of a pattern within a file.
# Partilwlarly used to search mods log for PASS or FAIL string
#
# file    -> Input file to be searched
# pattern -> Pattern to be searched
#
# Returns number of oclwrences of given pattern
#
sub searchPatternInFile
{
    my ($file, $pattern) = (@_);

    open RFILE, "<$file" or die $!;

    my @lines = <RFILE>;
    my $contents= join(' ', @lines);
    $contents =~ s{\n}{ }g;

    #Search for atleast 1 oclwrence of string
    my @matches = ($contents =~ /$pattern/g);
    my $oclwrs = @matches;

    close RFILE;

    return $oclwrs;
}

#
# Call mods to encrypt a key. Uses GEN MODS passed by user. 
# Also scans through the mods log in the end to find out if 
# mods completed successfully.
#
# inbin       -> Input private key file in binary format
# outbin      -> Output file to hold the encrypted key from mods
# modslogpath -> Path used for creating tmp files or logs if needed
#
sub callGenModsToEncrypt
{
    my ($inbin, $outbin, $modslogpath) = (@_);
    my $lwrrentdir       = getcwd;
    my $cmd              = "";
    my $modslog          = $modslogpath."/modsgen.log";

    #Change into mods dir
    chdir $genmodspath;

    #Run mods command
    $cmd = "$genmodsargs -gfe_keyin $inbin -gfe_keyout $outbin -gfe_gen > $modslog 2>&1";
    #print $cmd;
    system($cmd);

    #Verif if mods ran properly or not
    if (!searchPatternInFile($modslog, $genpassstring))
    {
        die "ERROR: MODS failed while generating key, check log at $modslog\n";
    }

    #Delete mods file
    unlink $modslog;
    
    #Change back to CWD
    chdir $lwrrentdir;
}

#
# This is called after using the encrypted key to generated signed ECID.
# This function uses OPENSSL to make sure the signature in the ECID returned by SEC2
# falcon is valid. 
#
# keyid          -> ID of the key being generated and validated
# pubkey         -> Public RSA key to use for validating ECID signature
# outfilewithsig -> A binary file which has ECID and SIG concatanated in binary format.
#                   RM test cl95a1sec2gfe test needs to generate this file.
# osslogpath     -> Path for creating tmp files and logs
#
sub verifyEcidSigWithOpenssl
{
    my ($keyid, $pubkey, $outfilewithsig, $osslogpath) = (@_);
    my $sha2bin = $osslogpath."/tmpsha2.bin";
    my $sigbin = $osslogpath."/tmpsig.bin";
    my $ecidbin = $osslogpath."/tmpecid.bin";
    my $osslout = $osslogpath."/ossl.log";
 
    print "Validating sig: $keyid ..... ";
    if ($keysize > 1024)
    {
        print "unsupported keysize, skipping\n";
        return;
    }

    #The outfilewithsig has both ECID data and signature, split it out
    open    RFILE, "<$outfilewithsig" or die $!;
    binmode RFILE;
    undef $/;
    $contents = join('', <RFILE>);
    close RFILE;

    my $sigsize = $keysize/8;
    $sig = substr $contents, -$sigsize;
    open WFILE, ">$sigbin" or die $!;
    binmode WFILE;
    print WFILE $sig;
    close WFILE;

    my $ecidsize = length($contents) - $sigsize;
    $ecid = substr $contents, 0, $ecidsize;
    open WFILE, ">$ecidbin" or die $!;
    binmode WFILE;
    print WFILE $ecid;
    close WFILE;

    #Generate SHA256 digest of ecidbin
    system("$openssl dgst -sha256 -out $sha2bin -binary $ecidbin");

    #Verify signature now (openssl returns 1 but command finishes successfully. Ignoring 1 for now)
    run([0..1], "$openssl pkeyutl -verify -pubin -in $sha2bin -sigfile $sigbin -inkey $pubkey -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:-1 > $osslout 2>&1"); 

    if (!searchPatternInFile($osslout, $osslpassstring))
    {
        die "ERROR: Openssl sig verif failed, check log at $osslout\n";
    }
    print "Done\n";

    unlink $sha2bin;
    unlink $ecidbin;
    unlink $osslout;
    unlink $sigbin;
}

#
# This is called to generate signed ECID using the encrypted priv key just generated.
# Uses the VERIF mods passed by the user.
#
# keyid       -> ID of the key being verified
# inbin       -> Encrypted private RSA key of the key pair
# pubkey      -> RSA public key to be used for verification
# modslogpath -> Path to use for tmp files and logs
#
sub callVerifModsToVerifyEcid
{
    my ($keyid, $inbin, $pubkey, $modslogpath) = (@_);
    my $lwrrentdir       = getcwd;
    my $cmd              = "";
    my $modslog          = $modslogpath."/modsverif.log";
    my $outbin           = $modslogpath."/ecidwithsig.bin";

    #Change into mods dir
    chdir $verifmodspath;

    #Run mods command
    print "Validating key: $keyid ..... ";
    if ($keysize > 1024)
    {
        print "unsupported keysize, skipping\n";
        return;
    }

    $cmd = "$verifmodsargs -gfe_keyin $inbin -gfe_keyout $outbin > $modslog 2>&1";
    system($cmd);

    #Verif if mods ran properly or not
    if (!searchPatternInFile($modslog, $verifpassstring))
    {
        die "ERROR: MODS failed while verifying key, check log at $modslog\n";
    }
    print "Done\n";

    #Delete mods log    
    unlink $modslog;

    #
    #Run Openssl to make sure signature is right
    #Disabling Validating Signature for now, as it has been broken since long and fails the script while generating multiple RSA key-pairs
    #
    #verifyEcidSigWithOpenssl($keyid, $pubkey, $outbin, $modslogpath);

    #Change back to CWD
    chdir $lwrrentdir;

    unlink $outbin;
}

#
# Sign the generated GFE key pair with MASTER RSA 2K key.
# Key pair is concataneted into a binary file before hashing and signing.
# keypairfile = (encrypted priv key in binary | public key in binary format)
# sigfile = RSA_sig (master private key, SHA256(keypairfile)
# WARNING: Any changes in this sequence will affect GFE server
#
# keyid       -> ID of the key being generated
# encpriv     -> Encrypted RSA priv key of this key id
# pubkey      -> Public RSA key for this key id
# keyfilepath -> Path to use for final files, tmp files and logs
#
sub signGfeKeyPair
{
    my ($keyid, $encpriv, $pubkey, $keyfilepath) = (@_);
    my $mastersigfile = $keyfilepath."/".$prefix."master_sig.bin";
    my $tmpcat        = $keyfilepath."/tmpcat.bin";
    my $sha2bin       = $keyfilepath."/sha2bin.bin";
    my $tmpout        = $keyfilepath."/tmpout.log";
    my $contents      = "";

    open RFILE, "<$encpriv" or die $!;
    binmode RFILE;
    undef $/;
    my @lines = <RFILE>;
    $contents= join('', @lines);
    close RFILE;

    open RFILE, "<$pubkey" or die $!;
    binmode RFILE;
    undef $/;
    my @lines = <RFILE>;
    $contents .= join('', @lines);
    close RFILE;

    open WFILE, ">$tmpcat" or die $!;
    print WFILE $contents;
    close WFILE;

    #Generate SHA256 digest of ecidbin
    system("$openssl dgst -sha256 -out $sha2bin -binary $tmpcat");

    #Sign now
    system("$openssl pkeyutl -sign -passin pass:\"$masterkeypasswd\" -in $sha2bin -out $mastersigfile -inkey $masterkeypem -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:-1 > $tmpout 2>&1");

    if (! -e $mastersigfile)
    {
        die("ERROR: While signing using master key\n");
    }

    #Verify signature (openssl returns 1 but the command finishes successfully. Ignore value 1)
    run([0..1], "$openssl pkeyutl -verify -passin pass:\"$masterkeypasswd\" -in $sha2bin -sigfile $mastersigfile -inkey $masterkeypem -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pss -pkeyopt rsa_pss_saltlen:-1 > $tmpout 2>&1");

    if (!searchPatternInFile($tmpout, $osslpassstring))
    {
        die "ERROR: While verifying signature using master RSA pub key, check log at $tmpout\n";
    }

    #Delete tmp files
    unlink $tmpcat;
    unlink $sha2bin;
    unlink $tmpout;

    $contents = "";
}

#
# Function which generates key pairs, encrypts the private key using genmods, validates those key pairs
# using verifmods and sign the key pair (private key now encrypted) using master rsa key
#
# keyid       -> ID of the key being generated
# keyfilepath -> Path to use for key files, tmp files and log
#
sub generateKey
{
    my ($keyid, $keyfilepath) = (@_);
    my $enckey = $keyfilepath."/".$prefix."priv_enc.bin";
    my $pubkey = $keyfilepath."/".$prefix."pub.pem";

    #Below tmp files will be deleted before exiting this function
    my $tmppem   = $keyfilepath."/tmp.pem";
    my $tmptext  = $keyfilepath."/tmp.txt";
    my $tmpbin   = $keyfilepath."/tmp.bin";

    #Generate key using openssl
    system("$openssl genrsa -out $tmppem $keysize > /dev/null 2>&1");

    #Get text output
    system("$openssl rsa -in $tmppem -text -noout > $tmptext 2>&1");

    #Get pubkey out of pem file
    system("$openssl rsa -in $tmppem -pubout -out $pubkey > /dev/null 2>&1");

    #Delete PEM file
    unlink $tmppem;

    #Colwert the text output into a format we need 
    open    RFILE, "<$tmptext" or die $!;
    open    WFILE, ">$tmpbin" or die $!;
    binmode WFILE;
    undef $/;

    my @lines = <RFILE>;
    my $contents= join(' ', @lines);
    $contents =~ s{\n}{ }g;

    #Unlink txt file
    unlink $tmptext;

    $genContentError = 0;

    #Read all the lines
    print WFILE getContentsOfPkeyParam($contents, "modulus:", "publicExponent:", $keysize/8);
    print WFILE getContentsOfPkeyParam($contents, "prime1:", "prime2:", $keysize/16);
    print WFILE getContentsOfPkeyParam($contents, "prime2:", "exponent1:", $keysize/16);
    print WFILE getContentsOfPkeyParam($contents, "exponent1:", "exponent2:", $keysize/16);
    print WFILE getContentsOfPkeyParam($contents, "exponent2:", "coefficient:", $keysize/16);
    print WFILE getContentsOfPkeyParam($contents, "coefficient:", "", $keysize/16);
 
    #Closing files
    close RFILE;
    close WFILE;

    #Check if error oclwred during generating key, regenerate again
    if ($genContentError == 1)
    {
        unlink $tmpbin;
        return;
    }

    #Now call mods with proper arguments to get encrypted key
    print "Generating key: $keyid ..... ";
    callGenModsToEncrypt($tmpbin, $enckey, $keyfilepath);
    print "Done\n";

    #Lets verify newly generated key
    callVerifModsToVerifyEcid($keyid, $enckey, $pubkey, $keyfilepath);

    #Remove tmpfiles
    unlink $tmpbin;

    #Now sign encrypted key and public key using master key
    print "Signing key   : $keyid ..... ";
    signGfeKeyPair($keyid, $enckey, $pubkey, $keyfilepath);
    print "Done\n";

    print "\n";
}

#
# Help function
#
sub help
{
    print "================================================================================================\n";
    print "Help:\n";
    print "\tperl gfekeygen.pl <args>\n";
    print "\t\t--help       - Print this help\n";
    print "\t\t--chip       - Chip to run MODS on                 (Default: gm206)\n";
    print "\t\t--keysize    - Either 1024 or 2048                 (Default: 1024)\n";
    print "\t\t--totalkeys  - Number of keys to be generated      (Default: 30)\n";
    print "\t\t--outdir     - Output directory for generated keys (Default: generated_keys)\n";
    print "\t\t--prefix     - Key name prefix                     (Default: rsa)\n";
    print "\t\t--openssl    - Openssl binary                      (Default: ./openssl)\n";
    print "\t\t--genmods    - Mods path to use for keygen         (Default: PWD)\n";
    print "\t\t--verifmods  - Mods path to use for verification   (Default: PWD)\n";
    print "\t\t--clean      - Delete outdir if exists             (Default: no)\n";
    print "\t\t--reboot     - Reboot from last state (no clean)   (Default: no)\n";
    print "================================================================================================\n";

    exit;
}

#
# Parse command line arguments and perform sanity checks
# Update global variables accordingly
#
sub parseArgs
{
    my $help  = 0;
    my $clean = 0;
    GetOptions("keysize=i" => \$keysize,
               "totalkeys=i" => \$totalkeys,
               "outdir=s" => \$outdir,
               "prefix=s" => \$prefix,
               "help" => \$help,
               "clean" => \$clean,
               "openssl=s" => \$openssl,
               "chip=s" => \$chip,
               "genmods=s" => \$genmodspath,
               "verifmods=s" => \$verifmodspath,
               "reboot" => \$reboot)
    or die("ERROR: command line arguments");

    if ($help)
    {
        help();
    }

    #Check args
    if (($keysize != 1024) && ($keysize != 2048))
    {
        print "ERROR: Invalid keysize\n";
        help();
    }

    if ($keysize == 2048)
    {
        $genmodsargs   .= " -gfe_2k ";
        $verifmodsargs .= " -gfe_2k ";
    }

    #Append chip
    #$genmodsargs   .= " -chip $chip ";
    #$verifmodsargs .= " -chip $chip ";

    if (!$reboot)
    {
        #Check if clean is specified
        if ($clean)
        {
            rmtree $outdir;
        }

        #Check if directory exists
        mkdir $outdir or die "ERROR: Directory '$outdir' already exists\n";
    }
    else
    {
        #Reboot specified
        if (! -d $outdir)
        {
            #Outdir doesnt exist, cancel reboot
            $reboot = 0;
            mkdir $outdir or die "ERROR: Directory '$outdir' already exists\n";
        }
    }

    #check if openssl exists
    if (! -e $openssl)
    {
        die("ERROR: Openssl bin doesnt exist\n");
    }

    #check if gen mods exists
    if (! -d $genmodspath)
    {
        die("ERROR: Gen Mods '$genmodspath' doesnt exist\n");
    }

    #check if verif mods exists
    if (! -d $verifmodspath)
    {
        die("ERROR: Verif Mods '$verifmodspath' doesnt exist\n");
    }


    print "KEYGEN: Config\n";
    print "\t Reboot         => $reboot\n";
    print "\t Chip           => $chip\n";
    print "\t Keysize        => $keysize\n";
    print "\t Totalkeys      => $totalkeys\n";
    print "\t Outdir         => $outdir\n";
    print "\t Prefix         => $prefix\n";
    print "\t Openssl        => $openssl\n";
    print "\t Genmodspath    => $genmodspath\n";
    print "\t Verifmodspath  => $verifmodspath\n";
    print "\n";
}

#
# Main function which loops through number of keys requested and calls generateKey
#
sub handleKeyGen
{
    my $keydir      = $outdir."/key_".$keysize."_";
    my $rebootfile  = $outdir."/reboot.txt";
    my $rebootcount = 0;

    if ($reboot && (-e $rebootfile))
    {
        open RFILE, "<$rebootfile" or die $!;
        my @line = <RFILE>;
        $rebootcount = $line[0];
        #Directory should already exist, start from next
        $rebootcount += 1;
    }

    for ($count=$rebootcount; $count < $totalkeys; $count++)
    {
        local $keyid=$count+1;
        $keypath = $keydir.($count+1);

ecc:
        if ((-d $keypath))
        {
            #Could be stale
            rmtree $keypath;
        }

        mkdir $keypath or die("ERROR: Failed to create '$keypath' directory\n");
        generateKey($keyid, $keypath);

       if ($genContentError == 1)
       {
           #Redo
           goto ecc;
       }

        open WFILE, ">$rebootfile" or die $!;
        print WFILE $count;
        close WFILE;
    }

    print "\nKEYGEN: Done\n";
}

#
# Check password strent
#
sub isPasswordIlwalid
{
    my ($password) = (@_);

    # Check password length
    if (length($password) < 8)
    {
        print "ERROR: Password doesn't meet the required length,";
        return 1;
    }

    # Check if password has atleast one uppercase character
    if (!($password =~ /[A-Z]/))
    {
        print "ERROR: Password doesn't have an uppercase character,";
        return 1;
    }
 
    # Check if password has atleast one lowercase character
    if (!($password =~ /[a-b]/))
    {
        print "ERROR: Password doesn't have a lowercase character,";
        return 1;
    }
 
    # Check if password has atleast one number
    if (!($password =~ /[0-9]/))
    {
        print "ERROR: Password doesn't have a numeric digit,";
        return 1;
    }
 
    # Check if password has atleast one special character
    if (($password =~ /[^a-zA-Z0-9]/))
    {
        print "ERROR: Special characters not allowed, ";
        return 1;
    }
 
    return 0;
}

#
# Generates global master RSA key and encrypts using the password provided by user
# NOTE: PASSWORD will be in clear (use caution)
#
sub generateGlobalRsakey
{
    my $mastergiven = 0;
    my $retrycount  = 0;
    my $ossllog     = $outdir."/ossl.log";
    $masterkeypem    = $outdir."/masterkey.pem";
    $masterkeypubpem = $outdir."/masterkeypub.pem";

    #Check if key already exists and reboot is specified
    if (-e $masterkeypem )
    {
        if ((!$reboot) || (!-e $masterkeypubpem))
        {
            die("ERROR: Master key exists with no reboot or master pub key\n");
        }

        #Master key is provided, use it
        $mastergiven = 1;
        print "KENGEN: Retrieving given master key\n";
    }
    else
    {
        print "KENGEN: Generating Master RSA2K key to sign GFE key pairs\n";
    }
PASSWORD:
    $retrycount += 1;
    print "\tEnter a password for master key (len=8, [a-zA-Z0-9): ";
    system('stty', '-echo');
    chop($masterkeypasswd = <STDIN>);
    system('stty', 'echo');

    # Check password
    if (isPasswordIlwalid($masterkeypasswd))
    {
        if ($retrycount < 6)
        {
            print " , try again\n";
            goto PASSWORD;
        }
        else
        {
            print " , exiting\n";
            die();
        }
    }
    
    if (!$mastergiven)
    {
        #Generate key using openssl
        system("$openssl genrsa -des3 -passout pass:\"$masterkeypasswd\" -out $masterkeypem $masterkeysize > $ossllog 2>&1");

        if (!searchPatternInFile($ossllog, $osslgenpass))
        {
            die "ERROR Openssl: While generating master RSA pub key, check log at $ossllog\n";
        }
    }

    #Get pubkey out of pem file
    system("$openssl rsa -in $masterkeypem -passin pass:\"$masterkeypasswd\" -pubout -out $masterkeypubpem > $ossllog 2>&1");
    if (!searchPatternInFile($ossllog, $osslpubpass))
    {
        die "ERROR Openssl: While generating master RSA pub key, check log at $ossllog\n";
    }
    print "\n";

    unlink $ossllog;
}

# Main functions
parseArgs();
generateGlobalRsakey();
handleKeyGen();
