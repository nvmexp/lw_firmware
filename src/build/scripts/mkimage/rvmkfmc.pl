#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# Pragmas.
#
use strict;
use warnings 'all';


#
# Module imports.
#
use Fcntl qw(SEEK_SET);
use File::Basename;
use Getopt::Long;
use impl::ProfilesImpl;


#
# Global constants.
#
use constant MPU_SECTION_NAME       => '.LWPU.mpu_info';
use constant SIGGEN_ALIGNMENT       => 256;

#
# ELF fields.
#
use constant ELF_EHDR_ENTRY         => { offset => 24, size => 8, unpack => "Q<" };
use constant ELF_EHDR_PHOFF         => { offset => 32, size => 8, unpack => "Q<" };
use constant ELF_EHDR_SHOFF         => { offset => 40, size => 8, unpack => "Q<" };
use constant ELF_EHDR_PHENTSIZE     => { offset => 54, size => 2, unpack => "S<" };
use constant ELF_EHDR_PHNUM         => { offset => 56, size => 2, unpack => "S<" };
use constant ELF_EHDR_SHENTSIZE     => { offset => 58, size => 2, unpack => "S<" };
use constant ELF_EHDR_SHNUM         => { offset => 60, size => 2, unpack => "S<" };
use constant ELF_EHDR_SHSTRNDX      => { offset => 62, size => 2, unpack => "S<" };

use constant ELF_PHDR_VADDR         => { offset => 16, size => 8, unpack => "Q<" };
use constant ELF_PHDR_PADDR         => { offset => 24, size => 8, unpack => "Q<" };

use constant ELF_SHDR_NAME          => { offset =>  0, size => 4, unpack => "L<" };
use constant ELF_SHDR_FLAGS         => { offset =>  8, size => 8, unpack => "Q<" };
use constant ELF_SHDR_ADDR          => { offset => 16, size => 8, unpack => "Q<" };
use constant ELF_SHDR_OFFSET        => { offset => 24, size => 8, unpack => "Q<" };

#
# ELF flags.
#
use constant ELF_SFLAG_ALLOC        => 0x2;
use constant ELF_SFLAG_EXECINSTR    => 0x4;


#
# Global data.
#
my %g_options;                                      # Hash structure containing processed command-line inputs.
(my $g_programName = basename($0)) =~ s/\.\w+$//;   # File name of this script (without extension).


#
# Special blocks.
#
BEGIN {
    # Save the original command-line.
    our $g_orgCmdLine = '';
    $g_orgCmdLine = join ' ', $0, @ARGV;
}

END {
    # Original command-line cached by BEGIN block.
    our $g_orgCmdLine;

    # If exit code is not zero, dump the original command line to facilitate debugging.
    if ($?) {
        print "\nFailed command:\n  ";
        print 'perl '.$g_orgCmdLine."\n";
        usage();
    }
}


#
# @brief Prints usage information and exits with error.
#
sub usage {

    print <<__USAGE__;

Options:

  --help                            : Display this message
  --profile     STRING              : Target profile (e.g. 'gsp-ga10x').
  --monitorCode FILE                : Code image for separation-kernel (and secure partitions).
  --monitorData FILE                : Data image for separation-kernel (and secure partitions).
  --stubCode    FILE                : Bootstub image.
  --elfFile     FILE                : Application ELF.
  --retSection  STRING              : Name of section to return to after partition switch.
  --objCopy     FILE                : Path to the objcopy application to use.
  --appCode     STRING              : Output name for application code image.
  --appData     STRING              : Output name for application data image.
  --outCode     STRING              : Output name for final code image.
  --outData     STRING              : Output name for final data image.

__USAGE__

    exit 1;
}

#
# @brief Prints the script name and arguments, then exits with error.
#
# @param[in] Text to be printed.
#
sub fatal {
    die "\n[$g_programName] ERROR: @_\n\n";
}

#
# @brief Exits with error if the indicated option has an empty value.
#
# @param[in] The value of the option.
# @param[in] The name of the option.
#
sub checkRequiredOption {
    my $value = shift;
    my $option = shift;

    fatal("Missing required option: --$option") unless $value;
}

#
# @brief Validates the value of a file option.
#
# @param[in] The value of the file option.
# @param[in] The name of the file option.
#
# Exits with error if the indicated option has an empty value, or refers to a
# file that does not exist or cannot be read.
#
sub checkRequiredFile {
    my $value = shift;
    my $option = shift;

    checkRequiredOption($value, $option);
    fatal("Invalid file: --$option $value") unless -f $value;
    fatal("Unreadable file: --$option $value") unless -r $value;
}

#
# @brief Extracts and validates command-line arguments.
#
# Populates %g_options on success. Exits with error on failure.
#
sub parseArgs {
    # Require correct case for option names.
    Getopt::Long::Configure('no_ignore_case');

    # Populate hash table.
    GetOptions(\%g_options,
        'help',
        'profile=s',
        'monitorCode=s',
        'monitorData=s',
        'stubCode=s',
        'elfFile=s',
        'retSection=s',
        'objCopy=s',
        'appCode=s',
        'appData=s',
        'outCode=s',
        'outData=s',
    ) or fatal("Invalid arguments");

    # Handle explicit usage request.
    if ($g_options{help}) {
        usage();
    }

    # Validate arguments.
    checkRequiredOption($g_options{profile},     'profile');
    checkRequiredFile  ($g_options{monitorCode}, 'monitorCode');
    checkRequiredFile  ($g_options{monitorData}, 'monitorData');
    checkRequiredFile  ($g_options{stubCode},    'stubCode');
    checkRequiredFile  ($g_options{elfFile},     'elfFile');
    checkRequiredOption($g_options{retSection},  'retSection');
    checkRequiredFile  ($g_options{objCopy},     'objCopy');
    checkRequiredOption($g_options{appCode},     'appCode');
    checkRequiredOption($g_options{appData},     'appData');
    checkRequiredOption($g_options{outCode},     'outCode');
    checkRequiredOption($g_options{outData},     'outData');
}

#
# @brief Reads the contents of the passed binary file.
#
# @param[in] The path to the file to read.
#
# @return The file's contents and size.
#
sub readFileBytes {
  my $fileName = shift;
  my $bytes;
  my $size;

  # Open indicated file for reading.
  open BINFILE, '<:raw', $fileName or
      fatal("Unable to open input file: $fileName");

  # Set binary mode.
  binmode BINFILE or
      fatal("Unable to set binary mode: $fileName");

  # Obtain the size of the file.
  ($size = -s $fileName) or
      fatal("Unable to obtain file size: $fileName");

  # Read the file's full contents.
  read(BINFILE, $bytes, $size) == $size or
      fatal("Unable to read entirety of $fileName");

  # Close the file.
  close BINFILE;

  # Finished.
  return ($bytes, $size);
}

#
# @brief Writes data to the passed binary file.
#
# @param[in] The file handle to write to.
# @param[in] The file name (for error messages).
# @param[in] The data to write.
#
sub writeFileBytes {
    my $file = shift;
    my $name = shift;
    my $bytes = shift;

    print $file $bytes or fatal("Unable to write to file: $name");
}

#
# @brief Obtains a reference to the requested profile data.
#
# @param[in] The name of the desired target profile.
#
# @return A reference to the target profile's data.
#
sub getProfileData {
    my $profile = shift;

    # Extract the chip from the profile name.
    $profile =~ /^(\w+)-(\w+)/;
    my $chip = $2;

    # Obtain a reference to the corresponding profile data.
    my $profileRef = Profiles->new('MAKE_FMC', 'RISCV')->grpItemRef($chip);
    fatal("Unrecognized profile: $profile") unless $profileRef;
    
    # Done.
    return $profileRef;
}

#
# @brief Computes the amount of padding required to satisfy a certain alignment.
#
# @param[in] The current size/position the padding will be computed from.
# @param[in] The desired alignment. Must be nonzero.
#
# @return The number of bytes required to reach the desired alignment.
#
sub computePadding {
    my $current = shift;
    my $alignment = shift;

    return ($alignment - ($current % $alignment)) % $alignment;
}

#
# @brief Reads a specific section of the indicated file.
#
# @param[in] The file handle to read from.
# @param[in] The file name (for error messages).
# @param[in] The offset to start reading from.
# @param[in] The amount of data to read.
#
# @return The data read from the file.
#
sub readFileField {
    my $file = shift;
    my $name = shift;
    my $offset = shift;
    my $size = shift;

    my $data;

    # Seek the file to the requested start position.
    seek($file, $offset, SEEK_SET) or
        fatal("Unable to seek to offset $offset in $name");

    # Read the requested amount of data.
    read($file, $data, $size) == $size or
        fatal("Unable to read $size bytes from $name");

    # Finished.
    return $data;
}

#
# @brief Reads a null-terminated string from the indicated file.
#
# @param[in] The file handle to read from.
# @param[in] The file name (for error messages).
# @param[in] The offset to start reading from.
#
# @return The resulting string.
#
sub readFileString {
    my $file = shift;
    my $name = shift;
    my $offset = shift;

    # Seek the file to the requested start position.
    seek($file, $offset, SEEK_SET) or
        fatal("Unable to seek to offset $offset in $name");

    # Temporarily override the terminator character.
    local $/ = "\0";

    # Read from the file until the terminator is reached.
    defined(my $string = <$file>) or
        fatal("Unable to read string from file: $name");

    # Remove the terminator from the result.
    chomp $string;

    # Finished.
    return $string;
}

#
# @brief Reads a specific field from a file's ELF header.
#
# @param[in] The handle for the ELF file to read from.
# @param[in] The ELF file's name (for error messages).
# @param[in] The ELF_EHDR_* constant for the field to read.
#
# @return The contents of the requested field.
#
sub readElfHeader {
    my $file = shift;
    my $name = shift;
    my $field = shift;

    # Read the specified field.
    my $bytes = readFileField($file, $name, $field->{offset}, $field->{size});

    # Deserialize and return.
    return unpack($field->{unpack}, $bytes);
}

#
# @brief Reads a specific field from an ELF program header.
#
# @param[in] The handle for the ELF file to read from.
# @param[in] The ELF file's name (for error messages).
# @param[in] The index of the desired program header.
# @param[in] The ELF_PHDR_* constant for the field to read.
#
# @return The contents of the requested field.
#
sub readProgramHeader {
    my $file = shift;
    my $name = shift;
    my $index = shift;
    my $field = shift;

    # First obtain the necessary parameters from the ELF header.
    my $phoff = readElfHeader($file, $name, ELF_EHDR_PHOFF);
    my $phentsize = readElfHeader($file, $name, ELF_EHDR_PHENTSIZE);

    # Compute the offset to the desired program header.
    my $base = $phoff + $phentsize * $index;

    # Read the requested field.
    my $bytes = readFileField($file, $name, $base + $field->{offset},
        $field->{size});

    # Deserialize and return.
    return unpack($field->{unpack}, $bytes);
}

#
# @brief Reads a specific field from an ELF section header.
#
# @param[in] The handle for the ELF file to read from.
# @param[in] The ELF file's name (for error messages).
# @param[in] The index of the desired section header.
# @param[in] The ELF_SHDR_* constant for the field to read.
#
# @return The contents of the requested field.
#
sub readSectionHeader {
    my $file = shift;
    my $name = shift;
    my $index = shift;
    my $field = shift;

    # First obtain the necessary parameters from the ELF header.
    my $shoff = readElfHeader($file, $name, ELF_EHDR_SHOFF);
    my $shentsize = readElfHeader($file, $name, ELF_EHDR_SHENTSIZE);

    # Compute the offset to the desired section header.
    my $base = $shoff + $shentsize * $index;

    # Read the reqested field.
    my $bytes = readFileField($file, $name, $base + $field->{offset},
        $field->{size});

    # Deserialize and return.
    return unpack($field->{unpack}, $bytes);
}

#
# @brief Obtains the textual name of an ELF section.
#
# @param[in] The handle for the ELF file to read from.
# @param[in] The ELF file's name (for error messages).
# @param[in] The index of the desired section.
#
# @return The name of the requested section as a string.
#
sub readSectionName {
    my $file = shift;
    my $name = shift;
    my $index = shift;

    # Locate the string table.
    my $shstrndx = readElfHeader($file, $name, ELF_EHDR_SHSTRNDX);
    my $strtabOffset = readSectionHeader($file, $name, $shstrndx, ELF_SHDR_OFFSET);

    # Obtain the location of the target section's name within the string table.
    my $nameOffset = readSectionHeader($file, $name, $index, ELF_SHDR_NAME);

    # Read the section's name out as a null-terminated string.
    return readFileString($file, $name, $strtabOffset + $nameOffset);
}

#
# @brief Obtains the index of an ELF section from its textual name.
#
# @param[in] The handle for the ELF file to search through.
# @param[in] The ELF file's name (for error messages).
# @param[in] The name of the desired section.
#
# @return The index of the requested section.
#
sub getSectionByName {
    my $file = shift;
    my $name = shift;
    my $target = shift;

    # Get the total number of sections.
    my $shnum = readElfHeader($file, $name, ELF_EHDR_SHNUM);

    # Search for the target section.
    for(my $i = 0; $i < $shnum; $i++) {
        if(readSectionName($file, $name, $i) eq $target) {
            return $i;
        }
    }

    # Not found.
    fatal("Unable to find section $target in $name");
}

#
# @brief Obtains the base physical address of an ELF section.
#
# @param[in] The handle for the ELF file to read from.
# @param[in] The ELF file's name (for error messages).
# @param[in] The name of the desired section.
#
# @return The physical address of the requested section.
#
sub getSectionAddrPhys {
    my $file = shift;
    my $name = shift;
    my $target = shift;

    # Find the target section.
    my $index = getSectionByName($file, $name, $target);

    # Sanity check that the section is loadable.
    if ((readSectionHeader($file, $name, $index, ELF_SHDR_FLAGS) &
        ELF_SFLAG_ALLOC) == 0) {
        fatal("Section $target in $name is not loadable");
    }

    # Get the section's virtual address.
    my $vaddrTarget = readSectionHeader($file, $name, $index, ELF_SHDR_ADDR);

    # Get the total number of segments in the ELF file.
    my $phnum = readElfHeader($file, $name, ELF_EHDR_PHNUM);

    # Find the corresponding segment for the target section.
    for(my $i = 0; $i < $phnum; $i++) {
        my $vaddrLwrrent = readProgramHeader($file, $name, $i, ELF_PHDR_VADDR);

        if ($vaddrLwrrent == $vaddrTarget) {
            # Return the segment's physical address.
            return readProgramHeader($file, $name, $i, ELF_PHDR_PADDR);
        }
    }

    # No such segment found.
    fatal("Unable to find matching segment for section $target in $name");
}

#
# @brief Computes various parameters required by the bootstub.
#
# @param[in] The size of the bootstub code.
#
# @return The computed parameters and their size.
#
# Assumes that %g_options has already been populated and validated.
#
sub computeParameters {
    my $stubSize = shift;

    # Open application ELF for reading.
    open my $ELF_FILE, '<:raw', $g_options{elfFile} or
        fatal("Unable to open ELF file: $g_options{elfFile}");

    # Set binary mode.
    binmode $ELF_FILE or
        fatal("Unable to set binary mode: $g_options{elfFile}");

    # Compute total size of parameters.
    my $paramSize = (
        + 8     # sizeof(LW_RISCV_MPU_INFO*)
        + 8     # sizeof(void*)
        + 8     # sizeof(void*)
        + 8     # sizeof(LwU64)
    );

    # Construct parameters themselves as a byte string.
    my $paramString = (
        # Pointer to the MPU descriptor (PA).
        pack("Q<", getSectionAddrPhys($ELF_FILE, $g_options{elfFile}, MPU_SECTION_NAME))       .

        # Pointer to the application entry-point (VA).
        pack("Q<", readElfHeader($ELF_FILE, $g_options{elfFile}, ELF_EHDR_ENTRY))              .

        # Pointer to the partition-switch return section (PA).
        pack("Q<", getSectionAddrPhys($ELF_FILE, $g_options{elfFile}, $g_options{retSection})) .

        # Total bootstub footprint.
        pack("Q<", $stubSize + $paramSize)
    );

    # Sanity check.
    my $actualSize = length($paramString);
    fatal("Actual size of boot parameters does not match expected size " .
        "($actualSize vs. $paramSize)") unless ($actualSize == $paramSize);

    # Done with ELF file.
    close $ELF_FILE;

    # Finished.
    return ($paramString, $paramSize);
}

#
# @brief Creates the intermediate application images.
#
# Dumps the relevant sections of the application ELF to produce intermediate
# code and data images used to create the final output images.
#
# Assumes that %g_options has already been populated and validated.
#
sub dumpElfSections {
    my $codeSections = "";
    my $dataSections = "";

    # Open application ELF for reading.
    open my $ELF_FILE, '<:raw', $g_options{elfFile} or
        fatal("Unable to open ELF file: $g_options{elfFile}");

    # Set binary mode.
    binmode $ELF_FILE or
        fatal("Unable to set binary mode: $g_options{elfFile}");

    # Get the total number of sections.
    my $shnum = readElfHeader($ELF_FILE, $g_options{elfFile}, ELF_EHDR_SHNUM);

    # Sort the sections into code and data.
    for (my $i = 0; $i < $shnum; $i++) {
        # Get the flags for the current section.
        my $lwrrentFlags = readSectionHeader($ELF_FILE, $g_options{elfFile},
            $i, ELF_SHDR_FLAGS);

        # Skip if not loadable.
        if (($lwrrentFlags & ELF_SFLAG_ALLOC) == 0) {
            next;
        }

        # Append to the appropriate list.
        my $lwrrentName = readSectionName($ELF_FILE, $g_options{elfFile}, $i);
        if (($lwrrentFlags & ELF_SFLAG_EXECINSTR) == 0) {
            $dataSections .= " -j $lwrrentName";
        } else {
            $codeSections .= " -j $lwrrentName";
        }
    }

    # Done with ELF file.
    close $ELF_FILE;

    # Dump the code sections to create the application code image.
    if (system("$g_options{objCopy} -O binary $codeSections $g_options{elfFile} $g_options{appCode}") != 0) {
        fatal("Unable to dump code sections: $codeSections");
    }

    # Dump the data sections to create the application data image.
    if (system("$g_options{objCopy} -O binary $dataSections $g_options{elfFile} $g_options{appData}") != 0) {
        fatal("Unable to dump data sections: $dataSections");
    }
}

#
# @brief Creates the final code image.
#
# @param[in] A reference to the target profile's parameters.
#
# Combines the separation-kernel/secure-partitions code image, boot-stub code
# image, boot parameters, and application code image into a single loadable
# text blob.
#
# Assumes that %g_options has already been populated and validated.
#
sub createFmcText {
    my $profileRef = shift;
    my $totalSize = 0;
    my $padSize = 0;

    # Open output file for writing.
    open my $OUTPUT_FILE, ">$g_options{outCode}" or
        fatal("Unable to open output file: $g_options{outCode}");

    # Set binary mode.
    binmode $OUTPUT_FILE or
        fatal("Unable to set binary mode: $g_options{outCode}");

    # Write separation-kernel/secure-partitions image.
    my ($skCodeBytes, $skCodeSize) = readFileBytes($g_options{monitorCode});
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, $skCodeBytes);
    $totalSize += $skCodeSize;

    # Pad to MPU boundary.
    $padSize = computePadding($totalSize, $profileRef->{MPU_GRANULARITY});
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Write bootstub image.
    my ($bsCodeBytes, $bsCodeSize) = readFileBytes($g_options{stubCode});
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, $bsCodeBytes);
    $totalSize += $bsCodeSize;

    # Write boot parameters.
    my ($paramBytes, $paramSize) = computeParameters($bsCodeSize);
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, $paramBytes);
    $totalSize += $paramSize;

    # Pad to MPU boundary.
    $padSize = computePadding($totalSize, $profileRef->{MPU_GRANULARITY});
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Pad to carveout boundary (check for overflow).
    $padSize = $profileRef->{IMEM_START_CARVEOUT_SIZE} - $totalSize;
    fatal('IMEM carveout overflow by ' . -$padSize . ' bytes') unless ($padSize >= 0);
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Write application image.
    my ($appCodeBytes, $appCodeSize) = readFileBytes($g_options{appCode});
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, $appCodeBytes);
    $totalSize += $appCodeSize;

    # Pad to signing alignment.
    $padSize = computePadding($totalSize, SIGGEN_ALIGNMENT);
    writeFileBytes($OUTPUT_FILE, $g_options{outCode}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Check for IMEM overflow.
    $padSize = $profileRef->{IMEM} - $totalSize;
    fatal('Final code image overflows IMEM by ' . -$padSize . ' bytes')
        unless ($padSize >= 0);

    # Finished.
    close $OUTPUT_FILE;
}

#
# @brief Creates the final data image.
#
# @param[in] A reference to the target profile's parameters.
#
# Combines the separation-kernel/secure-partitions data image and
# application data image into a single loadable data blob.
#
# Assumes that %g_options has already been populated and validated.
#
sub createFmcData {
    my $profileRef = shift;
    my $totalSize = 0;
    my $padSize = 0;

    # Open output file for writing.
    open my $OUTPUT_FILE, ">$g_options{outData}" or
        fatal("Unable to open output file: $g_options{outData}");

    # Set binary mode.
    binmode $OUTPUT_FILE or
        fatal("Unable to set binary mode: $g_options{outData}");

    # Write separation-kernel/secure-partitions image.
    my ($skDataBytes, $skDataSize) = readFileBytes($g_options{monitorData});
    writeFileBytes($OUTPUT_FILE, $g_options{outData}, $skDataBytes);
    $totalSize += $skDataSize;

    # Pad to carveout boundary (check for overflow).
    $padSize = $profileRef->{DMEM_START_CARVEOUT_SIZE} - $totalSize;
    fatal('DMEM carveout overflow by ' . -$padSize . ' bytes') unless ($padSize >= 0);
    writeFileBytes($OUTPUT_FILE, $g_options{outData}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Write application image.
    my ($appDataBytes, $appDataSize) = readFileBytes($g_options{appData});
    writeFileBytes($OUTPUT_FILE, $g_options{outData}, $appDataBytes);
    $totalSize += $appDataSize;

    # Pad to signing alignment.
    $padSize = computePadding($totalSize, SIGGEN_ALIGNMENT);
    writeFileBytes($OUTPUT_FILE, $g_options{outData}, (pack("C", 0) x $padSize));
    $totalSize += $padSize;

    # Check for DMEM overflow.
    $padSize = $profileRef->{DMEM} - $totalSize;
    fatal('Final data image overflows DMEM by ' . -$padSize . ' bytes')
        unless ($padSize >= 0);
    
    # Finished.
    close $OUTPUT_FILE;
}

#
# @brief Creates the output code and data images.
#
# Combines the separation-kernel/secure-partitions code image, boot-stub code
# image, boot parameters, and application code image into a single loadable
# text blob.
#
# Also combines the separation-kernel/secure-partitions data image and
# application data image into a loadable data blob.
#
# Assumes that %g_options has already been populated and validated.
#
sub createFmcImages {
    # Obtain a reference to the parameters for the target profile.
    my $profileRef = getProfileData($g_options{profile});

    # Extract the relevant ELF sections.
    dumpElfSections();

    # Create the text image.
    createFmcText($profileRef);

    # Create the data image.
    createFmcData($profileRef);
}


#
# Script entry-point.
#
parseArgs();
createFmcImages();
