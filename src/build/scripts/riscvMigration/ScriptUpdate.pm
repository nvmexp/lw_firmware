#
# Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This package contains tables to allow updating legacy build scripts,
# including within scripts that utilize these build scripts.
#

package ScriptUpdate;
use strict;
use warnings;

# TODO: Add update adjustments to Tasks and other scripts

our %profilesAdjustments = (
    IMEM => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of physical IMEM", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    DMEM => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of physical DMEM", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    EMEM => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of physical EMEM", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    DESC => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of descriptor area", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    RM_SHARE => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) RM share size"); },
    },
    RM_MANAGED_HEAP => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) RM managed heap size"); },
        ADJUST          => sub { $_[0] * 256; },
    },
    FREEABLE_HEAP => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) Size of freeable heap in resident DMEM"); },
        ADJUST          => sub { $_[0] * 256; },
    },

    CMD_QUEUE_LENGTH => {
        PREFIX_COMMENT  => 'Queue length information',
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) Size of each command queue"); },
    },
    MSG_QUEUE_LENGTH => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(bytes) Size of each message queue"); },
    },

    APP_METHOD_ARRAY_SIZE => {
        PREFIX_COMMENT  => 'Channel interface method ID information',
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("Distinct app method count"); },
        FORMAT          => "%d",
    },
    APP_METHOD_MIN => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(0x%X >> 2)", $_[0] << 2); },
    },
    APP_METHOD_MAX => {
        NEWSECTION      => 'common',
        COMMENT         => sub { sprintf("(0x%X >> 2)", $_[0] << 2); },
    },

    HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("If non-zero, generate linker script symbols for HS ucode encryption"); },
        FORMAT          => "%d",
    },
    IMEM_VA => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of virtually addressed IMEM", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    DMEM_VA => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("(bytes) [%d kB] Size of virtually addressed DMEM", $_[0] / 1024); },
        ADJUST          => sub { $_[0] * 1024; },
    },
    DMEM_VA_BOUND => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("(bytes) Start address of virtually addressable DMEM"); },
        ADJUST          => sub { $_[0] * 256; },
    },
    ISR_STACK => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("(bytes) Interrupt stack size"); },
    },
    ESR_STACK => {
        NEWSECTION      => 'falcon',
        COMMENT         => sub { sprintf("(bytes) Exception stack size"); },
    },

    SAFERTOS_IMEM_RESERVED => { REMOVE => 1, },
    SAFERTOS_DMEM_RESERVED => { REMOVE => 1, },
);
