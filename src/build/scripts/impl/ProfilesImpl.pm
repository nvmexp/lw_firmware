
package Profiles;

# < File orginization >
#
#   Profiles.pm
#       The raw data of Profiles definication
#       Located in 'build' dir of each modules (PMU/DPU/SEC)
#
#   ProfilesImpl.pm
#       The implementation and functions for Profiles.
#       Shared by RTOS-Flcn builds
#
#   ../../docs/Profiles.txt
#       Documentation on the format of Profiles.pm.
#

use strict;

use Carp;                              # for 'croak', 'cluck', etc.
use Utils;                             # import rmconfig utility functions
use Rmconfig::CfgHash;
use GenFile;
use Groups;

# This is a temporary module used for interpreting old scripts in the new formatting
use riscvMigration::ScriptUpdate;

our @ISA = qw( Groups Messages );

my $EMIT = undef;                      # ref to GenFile to emit g_xxx from template

#
# Header defines emitted by this script are organized here.
#
# COMMENT is a comment printed before this define. Do not use newlines.
# NAME is the symbol name that will be defined.
# FORMAT is the printf-compatible string use to format the return value of the getter.
# GETTER is a function reference that returns either `undef` for fields which should not emit, or a
#        value to be formatted. $_[0] is the ProfilesImpl instance, and $_[1] is the profile reference.
#
my @headerEntries = (
    {
        NAME    =>  'PROFILE_DMEM_VA_BOUND',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub {
            my $bound = $_[0]->dmemVaBound($_[1]);
            # Needs to be in blocks!
            $bound /= 256 if (defined $bound);
            return $bound;
        },
    },
    {
        NAME    =>  'PROFILE_RM_SHARE',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  \&rmShareSize,
    },
    {
        NAME    =>  'PROFILE_RM_MANAGED_HEAP_SIZE_BLOCKS',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub {
            my $size = $_[1]->{RM_MANAGED_HEAP};
            # Needs to be in blocks!
            $size /= 256 if (defined $size);
            return $size;
        },
    },
    {
        NAME    =>  'HS_UCODE_ENCRYPTION_LINKER_SYMBOLS',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&hsEncryptionSymbols,
    },
    {
        NAME    =>  'HS_UCODE_PKC_ENABLED',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&hsPkcEnabled,
    },
    {
        NAME    =>  'RM_PATCHING_HS_SIGNATURE_ENABLED',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&rmPatchingHsSignatureEnabled,
    },
    {
        NAME    =>  'HS_UCODE_PKC_SIG_SIZE_IN_BITS',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&hsPkcSigSize,
    },
    {
        NAME    =>  'NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&hsNumSigs,
    },
    {
        NAME    =>  'PROFILE_CMD_QUEUE_LENGTH',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{CMD_QUEUE_LENGTH}; },
    },
    {
        NAME    =>  'PROFILE_MSG_QUEUE_LENGTH',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{MSG_QUEUE_LENGTH}; },
    },
    {
        NAME    =>  'PROFILE_MPU_ENTRY_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{MPU_ENTRY_COUNT}; },
    },
    {
        NAME    =>  'BOOT_FROM_HS_ENABLED',
        FORMAT  =>  '(%dul)',
        GETTER  =>  \&bootFromHSEnabled,
    },

    # ODP configuration
    {
        NAME    =>  'PROFILE_ODP_CODE_PAGE_SIZE',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_CODE_PAGE_SIZE}; },
    },
    {
        NAME    =>  'PROFILE_ODP_CODE_PAGE_MAX_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_CODE_PAGE_MAX_COUNT}; },
    },
    {
        NAME    =>  'PROFILE_ODP_DATA_PAGE_SIZE',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_DATA_PAGE_SIZE}; },
    },
    {
        NAME    =>  'PROFILE_ODP_DATA_PAGE_MAX_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_DATA_PAGE_MAX_COUNT}; },
    },
    {
        NAME    =>  'PROFILE_ODP_MPU_FIRST',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_MPU_FIRST}; },
    },
    {
        NAME    =>  'PROFILE_ODP_MPU_SHARED_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_MPU_SHARED_COUNT}; },
    },
    {
        NAME    =>  'PROFILE_ODP_MPU_DTCM_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_MPU_DTCM_COUNT}; },
    },
    {
        NAME    =>  'PROFILE_ODP_MPU_ITCM_COUNT',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{ODP_MPU_DTCM_COUNT}; },
    },

    { # PMU only
        NAME    =>  'PROFILE_FRTS_MAX_SIZE',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{FRTS_MAX_SIZE}; },
    },

    # The below items are used for methods sent via the host channel interface (used only by SEC2 today)
    { # SEC2 only
        COMMENT =>  "Defines indicating the method array sizes and extents for channel interface.",
        NAME    =>  'APP_METHOD_ARRAY_SIZE',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{APP_METHOD_ARRAY_SIZE}; },
    },
    { # SEC2 only
        NAME    =>  'APP_METHOD_MIN',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{APP_METHOD_MIN}; },
    },
    { # SEC2 only
        NAME    =>  'APP_METHOD_MAX',
        FORMAT  =>  '(0x%xul)',
        GETTER  =>  sub { $_[1]->{APP_METHOD_MAX}; },
    },
);

#
# Makefile defines emitted by this script are organized here.
#
# COMMENT is a comment printed before this define. Do not use newlines.
# NAME is the symbol name that will be defined.
# FORMAT is the printf-compatible string use to format the return value of the getter.
# GETTER is a function reference that returns either `undef` for fields which should not emit, or a
#        value to be formatted. $_[0] is the ProfilesImpl instance, and $_[1] is the profile reference.
#
my @makefileEntries = (
    {
        NAME    =>  'DMEM_VA_SUPPORTED',
        FORMAT  =>  '%s',
        GETTER  =>  sub { return ($_[0]->dmemVaSize($_[1]) > 0) ? 'true' : 'false'; },
    },
);

# print general profile header
sub printProfileHeader {
    (@_ == 3) or croak 'usage: obj->printProfileHeader( PROFILE-NAME, OUTPUT-FILE )';

    my ($self, $profile, $outputFile) = @_;

    my ($opmode, $buildName);
    $profile =~ /^(\w+)-(\w+)$/;  # pmu-gf11x ->pmu, gf11x;   dpu-v0201 -> dpu, v0201
    ($opmode, $buildName) = ($1, $2);

    $EMIT = GenFile->new('C', $outputFile,
                         { update_only_if_changed => 1, } );  # PARAMS-HASH-REF

    croak "Error: Could not create output file '$outputFile' : $!"     if ! $EMIT;

    # Print header comments and preprocessing directives to avoid problems
    # caused by duplicate inclusion of header file
    $EMIT->print(<<__CODE__);
//
// This file is automatically generated by 'rtos-flcn-script.pl' - DO NOT EDIT!
//
// Profile defines
//

#ifndef _G_PROFILE_H_
#define _G_PROFILE_H_

__CODE__

    $EMIT->printf("// Defines indicating which profile is enabled on current ucode.\n");

    # loop the profiles
    foreach my $item (@{$self->grpItemsListRef()}) {
        my $enabled = (lc $buildName eq lc $item)? 1: 0;
        $EMIT->printf("#define %-40s %s\n", (uc $opmode)."_PROFILE_$item", $enabled);
    }
    $EMIT->print("\n");

    $EMIT->printf("// Defines indicating the options set for the profile.\n");

    my $profileRef = $self->grpItemRef($buildName);

    foreach my $entry (@headerEntries) {
        if (($entry->{NAME} eq 'HS_UCODE_ENCRYPTION_LINKER_SYMBOLS' or
                $entry->{NAME} eq 'HS_UCODE_PKC_ENABLED' or
                $entry->{NAME} eq 'HS_UCODE_PKC_SIG_SIZE_IN_BITS' or
                $entry->{NAME} eq 'BOOT_FROM_HS_ENABLED' or
                $entry->{NAME} eq 'NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL') and
            $self->_architecture($profileRef) ne 'FALCON') {
            next;
        }

        my $value = $entry->{GETTER}->($self, $profileRef);
        next if !defined $value;

        if (defined $entry->{COMMENT}) {
            $EMIT->printf("// %s\n", $entry->{COMMENT});
        }
        $EMIT->printf("#define %-40s $entry->{FORMAT}\n", $entry->{NAME}, $value);
    }

    $EMIT->printf("\n#endif //_G_PROFILE_H_\n");

    $EMIT->closefile();
    $EMIT = undef;

}

# print general profile makefile fragment
sub printProfileMakefile {
    (@_ == 3) or croak 'usage: obj->printProfileMakefile( PROFILE-NAME, OUTPUT-FILE )';

    my ($self, $profile, $outputFile) = @_;

    my ($opmode, $buildName);
    $profile =~ /^(\w+)-(\w+)$/;  # pmu-gf11x ->pmu, gf11x;   dpu-v0201 -> dpu, v0201
    ($opmode, $buildName) = (uc $1, $2);

    my $profileRef = $self->grpItemRef($buildName);

    $EMIT = GenFile->new('make', $outputFile);

    croak "Error: Could not create output file '$outputFile' : $!"     if ! $EMIT;

    $EMIT->print(<<__CODE__);
#
# This file is automatically generated by 'rtos-flcn-script.pl' - DO NOT EDIT!
#
# Profile configuration
#

ifdef ${opmode}CFG_PROFILE

__CODE__

    foreach my $entry (@makefileEntries) {
        my $value = $entry->{GETTER}->($self, $profileRef);
        next if !defined $value;

        if (defined $entry->{COMMENT}) {
            $EMIT->printf("# %s\n", $entry->{COMMENT});
        }
        $EMIT->printf("%-40s = $entry->{FORMAT}\n", $entry->{NAME}, $value);
    }

    $EMIT->print(<<__CODE__);

endif # ${opmode}CFG_PROFILE

#
# makefiles can test \$(${opmode}_PROFILE_MK_INITIALIZED) to ensure g_profile.mk
# has been included
#
${opmode}_PROFILE_MK_INITIALIZED := 1

__CODE__

    $EMIT->closefile();
    $EMIT = undef;

}

#
# \brief Check if the parameter is a power of two.
#
sub _isPowerOfTwo {
    my $val = shift;
    return ($val & ($val - 1)) == 0;
}

#
# \brief Validate the contents of the script (after resolving).
#
sub _validate {
    (@_ == 1) or croak 'usage: obj->_validate( )';

    my ($self) = @_;

    my $profileRef;
    foreach my $profileName (@{$self->grpItemsListRef()}) {

        $profileRef = $self->grpItemRef($profileName);

        if ($self->dmemVaSize($profileRef)) {
            # Ensure that the profile specifies a DMEM_VA_BOUND
            if (! $self->dmemVaBound($profileRef)) {
                $self->error("DMEM_VA_BOUND is required for profiles with DMEM_VA");
            }

            # Ensure that the VA_BOUND is smaller than physical DMEM
            if ($self->dmemVaBound($profileRef) >= $self->dmemPhysicalSize($profileRef)) {
                $self->error("DMEM_VA_BOUND must be less than physical DMEM size");
            }
        }

        if (defined $profileRef->{ODP_DATA_PAGE_SIZE}) {
            if ($profileRef->{ODP_DATA_PAGE_SIZE} < $profileRef->{MPU_GRANULARITY}) {
                $self->error("ODP_DATA_PAGE_SIZE must be the same size or larger than MPU_GRANULARITY");
            }
            if (!_isPowerOfTwo($profileRef->{ODP_DATA_PAGE_SIZE})) {
                $self->error("ODP_DATA_PAGE_SIZE must be a power of two");
            }
        }

        if (defined $profileRef->{ODP_CODE_PAGE_SIZE}) {
            if ($profileRef->{ODP_CODE_PAGE_SIZE} < $profileRef->{MPU_GRANULARITY}) {
                $self->error("ODP_CODE_PAGE_SIZE must be the same size or larger than MPU_GRANULARITY");
            }
            if (!_isPowerOfTwo($profileRef->{ODP_CODE_PAGE_SIZE})) {
                $self->error("ODP_CODE_PAGE_SIZE must be a power of two");
            }
        }

        if ((defined $profileRef->{ODP_MPU_SHARED_COUNT}) and
            (defined $profileRef->{ODP_MPU_DTCM_COUNT} or defined $profileRef->{ODP_MPU_ITCM_COUNT})) {
            $self->error("If ODP_MPU_SHARED_COUNT is set, ODP_MPU_*TCM_COUNT may not be set");
        }

        if ($self->odpEnabled($profileRef) and
            (!defined $profileRef->{ODP_MPU_SHARED_COUNT}) and
            (!defined $profileRef->{ODP_MPU_DTCM_COUNT}) and
            (!defined $profileRef->{ODP_MPU_ITCM_COUNT})) {
            $self->error("If ODP is enabled, MPU counts must be set");
        }

        # TODO - give error for missing required Fields
    }
}

#
# \brief Colwert old field formatting into the new formatting.
#
sub _scriptUpdate {
    (@_ == 2) or croak 'usage: obj->_scriptUpdate( PROFILE-REF )';

    my ($self, $profileRef) = @_;

    foreach my $fieldName (keys %$profileRef) {
        # Update the value to act like new script, if an update is necessary
        my $value = $profileRef->{$fieldName};
        if (defined $ScriptUpdate::profilesAdjustments{$fieldName} and
            defined $ScriptUpdate::profilesAdjustments{$fieldName}->{ADJUST}) {
            $profileRef->{$fieldName} = $ScriptUpdate::profilesAdjustments{$fieldName}->{ADJUST}->($value);
        }
    }
    $profileRef->{ARCH} = 'FALCON';
}

#
# \brief Resolve the contents of the profile into a flat format.
#
sub _scriptResolve {
    (@_ == 2) or croak 'usage: obj->_scriptResolve( PROFILE-REF )';

    my ($self, $profileRef) = @_;

    my $profileArch = $self->_architecture($profileRef);
    my %newHash;

    # Get common settings first, so that they may be overridden
    if (defined $profileRef->{COMMON}) {
        my $commonRef = cfgHashRef(delete $profileRef->{COMMON}, "$self->{FULL_NAME} COMMON");
        @newHash{keys %$commonRef} = values %$commonRef;
    }
    # Load architecture-specific settings.
    if (defined $profileRef->{$profileArch}) {
        my $archRef = cfgHashRef(delete $profileRef->{$profileArch}, "$self->{FULL_NAME} ARCH");
        @newHash{keys %$archRef} = values %$archRef;
    }
    # Delete both architectures.
    if (defined $profileRef->{FALCON}) {
        delete $profileRef->{FALCON};
    }
    if (defined $profileRef->{RISCV}) {
        delete $profileRef->{RISCV};
    }

    # Place new keys into the root.
    @$profileRef{keys %newHash} = values %newHash;
}

#
# \brief Colwert each profile's configuration to a simpler form.
#
sub _scriptParse
{
    (@_ == 1) or croak 'usage: obj->_scriptParse( )';

    my ($self) = @_;

    my $profileRef;
    foreach my $profileName (@{$self->grpItemsListRef()}) {
        $profileRef = $self->grpItemRef($profileName);

        # New section fields added to the new scripts can be used to determine script version.
        if (defined $profileRef->{COMMON} or
            defined $profileRef->{FALCON} or
            defined $profileRef->{RISCV} or
            defined $profileRef->{ARCH}) {
            $self->_scriptResolve($profileRef);
        }
        else {
            $self->_scriptUpdate($profileRef);
        }
    }
}

#
# \brief Determine the architecture of this profile reference.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Name of architecture for this profile.
#
sub _architecture
{
    (@_ == 2) or croak 'usage: obj->_architecture( PROFILE-REF )';

    my ($self, $profileRef) = @_;
    my $arch = $self->{PARAMETER_ARCH};

    if (! defined $arch) {
        $arch = $profileRef->{ARCH};
    }

    $self->error('Architecture must be specified when ambiguous') if (! defined $arch);

    $arch = uc $arch;
    if ($arch ne 'FALCON' and $arch ne 'RISCV') {
        $self->error("Unknown architecture '$arch'");
    }

    return $arch;
}

###################################################################################################
# Public API begins here!
###################################################################################################
# NOTE: Although we lwrrently keep compatibility with the previous script format, this is likely to
#       not live too long; once the new format is in place, we will want to remove the old format's
#       support.

sub new
{
    (@_ == 2 or @_ == 3) or croak 'usage: obj->new( NAME[, UPROC-ARCH] )';

    my ($class, $name, $arch) = @_;

    if (defined $arch) {
        $arch = uc $arch;
    }

    my $buildsRaw = utilReadConfigFile($name, 'Profiles.pm');

    my $self = Groups->new($name, $buildsRaw);

    bless $self, $class;

    $self->{PARAMETER_ARCH} = $arch;
    $self->_scriptParse();

    $self->_validate();

    return $self;
}

#
# \brief Retrieve the contents of a field from a profile.
#
# For old style scripts, the values will be 'updated' to be formatted like the new style scripts.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \param    FIELD-NAME          Name of the field to get.
#
# \return Contents of the field, or undef if it does not exist.
#
sub getField {
    (@_ == 3) or croak 'usage: obj->getField( PROFILE-ITEM-REF, FIELD-NAME )';

    my ($self, $profileRef, $fieldName) = @_;

    return $profileRef->{$fieldName};
}

#
# \brief Get size (and check enablement) of DMEM VA.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return DMEM VA size in bytes if the profile has DMEM VA, zero if it does not.
#
sub dmemVaSize {
    (@_ == 2) or croak 'usage: obj->dmemVaSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    # Only falcon has DMEM VA
    return undef unless ($self->_architecture($profileRef) eq 'FALCON');

    return $profileRef->{DMEM_VA};
}

#
# \brief Get DMEM-VA start address.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return DMEM-VA start address in bytes.
#
sub dmemVaBound {
    (@_ == 2) or croak 'usage: obj->dmemVaBound( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    # TODO: remove this once DMEM_VA_BOUND is not used on RISCV builds anymore
    if ($self->_architecture($profileRef) eq 'RISCV') {
        return 0;
    }

    if (! $self->dmemVaSize($profileRef)) {
        # No DMEM-VA
        return undef;
    }

    return $profileRef->{DMEM_VA_BOUND};
}

#
# \brief Get size of physical IMEM.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Physical IMEM size in bytes.
#
sub imemPhysicalSize {
    (@_ == 2) or croak 'usage: obj->imemPhysicalSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    my $value = $profileRef->{IMEM};
    $self->error('IMEM size must be specified') unless (defined $value);

    return $value;
}

#
# \brief Get size of physical DMEM.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Physical DMEM size in bytes.
#
sub dmemPhysicalSize {
    (@_ == 2) or croak 'usage: obj->dmemPhysicalSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    my $value = $profileRef->{DMEM};
    $self->error('DMEM size must be specified') unless (defined $value);

    return $value;
}

#
# \brief Get size of physical EMEM.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Physical EMEM size in bytes.
#
sub ememPhysicalSize {
    (@_ == 2) or croak 'usage: obj->ememPhysicalSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    # EMEM must be defined even if it's not specified
    my $value = $profileRef->{EMEM};
    if (! defined $value) {
        $value = 0;
    }
    return $value;
}

#
# \brief Get maximum size of IMEM region.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Maximum IMEM size in bytes.
#
sub imemMaxSize {
    (@_ == 2) or croak 'usage: obj->imemMaxSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    # Only falcon has IMEM VA
    if ($self->_architecture($profileRef) eq 'FALCON') {
        return $profileRef->{IMEM_VA};
    }
    else {
        return $self->imemPhysicalSize($profileRef);
    }
}

#
# \brief Get maximum size of DMEM region.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Maximum DMEM size in bytes.
#
sub dmemMaxSize {
    (@_ == 2) or croak 'usage: obj->dmemMaxSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    my $dmvaSize = $self->dmemVaSize($profileRef);

    if (! $dmvaSize) {
        # No DMEM-VA, use physical DMEM size
        return $self->dmemPhysicalSize($profileRef);
    }
    else {
        # DMEM-VA factors in EMEM size.
        return $dmvaSize + $self->ememPhysicalSize($profileRef);
    }
}

#
# \brief Get maximum size of DESC region.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Maximum DMEM size in bytes.
#
sub descMaxSize {
    (@_ == 2) or croak 'usage: obj->descMaxSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    return $profileRef->{DESC};
}

#
# \brief Get size of RM shared memory
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Size of RM shared memory in bytes.
#
sub rmShareSize {
    (@_ == 2) or croak 'usage: obj->rmShareSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    return $profileRef->{RM_SHARE};
}

#
# \brief Get whether to generate linker script symbols for HS ucode encryption.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return 1 if the symbols should be generated, otherwise 0.
#
sub hsEncryptionSymbols {
    (@_ == 2) or croak 'usage: obj->hsEncryptionSymbols( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('hsEncryptionSymbols only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{HS_UCODE_ENCRYPTION_LINKER_SYMBOLS};
}

#
# \brief Get whether to generate linker script symbols for PKC HS.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return 1 if the symbols should be generated, otherwise 0.
#
sub hsPkcEnabled {
    (@_ == 2) or croak 'usage: obj->hsPkcEnabled( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('hsPkcEnabled only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{HS_UCODE_PKC_ENABLED};
}

#
# \brief Get whether RM is patching HS sigantures to WPR.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return 1 if the symbols should be generated, otherwise 0.
#
sub rmPatchingHsSignatureEnabled {
    (@_ == 2) or croak 'usage: obj->rmPatchingHsSignatureEnabled( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    return $profileRef->{RM_PATCHING_HS_SIGNATURE_ENABLED};
}
#
# \brief Get number of signatures per HS overlay.
#
# \param     PROFILE-ITEM-REF    Reference to profile to check.
# \return %d
#
sub hsNumSigs {
    (@_ == 2) or croak 'usage: obj->hsPkcEnabled( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;
    my $numSigs = 1;

    $self->error('hsNumSigs only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');
    
    if ($profileRef->{NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL})
    {
        $numSigs = $profileRef->{NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL};
    }
    
    return $numSigs;
}

#
# \brief Get PKC HS signature size in bits.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return %d
#
sub hsPkcSigSize {
    (@_ == 2) or croak 'usage: obj->hsPkcSigSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('hsPkcSigSize only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{HS_UCODE_PKC_SIG_SIZE_IN_BITS};
}

#
# \brief Get Falcon ESR stack size.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Falcon ESR stack size in bytes.
#
sub flcnEsrStackSize {
    (@_ == 2) or croak 'usage: obj->flcnEsrStackSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('flcnEsrStackSize only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{ESR_STACK};
}

#
# \brief Get Falcon ISR stack size.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Falcon ISR stack size in bytes.
#
sub flcnIsrStackSize {
    (@_ == 2) or croak 'usage: obj->flcnIsrStackSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('flcnIsrStackSize only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{ISR_STACK};
}

#
# \brief Check to see if BOOT_FROM_HS is enabled
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return 1 if the symbols should be generated, otherwise 0.
#
sub bootFromHSEnabled {
    (@_ == 2) or croak 'usage: obj->bootFromHSEnabled( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('bootFromHSEnabled only valid for Falcon builds') if ($self->_architecture($profileRef) ne 'FALCON');

    return $profileRef->{BOOT_FROM_HS_ENABLED};
}


#
# \brief Get RISCV system stack size.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return RISCV system stack size in bytes.
#
sub riscvSysStackSize {
    (@_ == 2) or croak 'usage: obj->riscvSysStackSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    $self->error('riscvSysStackSize only valid for RISCV builds') if ($self->_architecture($profileRef) ne 'RISCV');

    return $profileRef->{SYS_STACK};
}

#
# \brief Get size of the freeable heap.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return Freeable heap size in bytes.
#
sub freeableHeapSize {
    (@_ == 2) or croak 'usage: obj->freeableHeapSize( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    return $profileRef->{FREEABLE_HEAP};
}

#
# \brief Get whether ODP is enabled or not.
#
# \param    PROFILE-ITEM-REF    Reference to profile to check.
# \return false if ODP is disabled, true if it is enabled.
#
sub odpEnabled {
    (@_ == 2) or croak 'usage: obj->odpEnabled( PROFILE-ITEM-REF )';

    my ($self, $profileRef) = @_;

    my $codePageSize = 0;
    my $dataPageSize = 0;
    if (defined $profileRef->{ODP_CODE_PAGE_SIZE}) {
        $codePageSize = $profileRef->{ODP_CODE_PAGE_SIZE};
    }
    if (defined $profileRef->{ODP_DATA_PAGE_SIZE}) {
        $dataPageSize = $profileRef->{ODP_DATA_PAGE_SIZE};
    }

    return ($codePageSize > 0) || ($dataPageSize > 0);
}

1;
