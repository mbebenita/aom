##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##


AV1_DX_EXPORTS += exports_dec

AV1_DX_SRCS-yes += $(AV1_COMMON_SRCS-yes)
AV1_DX_SRCS-no  += $(AV1_COMMON_SRCS-no)
AV1_DX_SRCS_REMOVE-yes += $(AV1_COMMON_SRCS_REMOVE-yes)
AV1_DX_SRCS_REMOVE-no  += $(AV1_COMMON_SRCS_REMOVE-no)

AV1_DX_SRCS-yes += av1_dx_iface.c

AV1_DX_SRCS-yes += decoder/decodemv.c
AV1_DX_SRCS-yes += decoder/decodeframe.c
AV1_DX_SRCS-yes += decoder/decodeframe.h
AV1_DX_SRCS-yes += decoder/detokenize.c
AV1_DX_SRCS-yes += decoder/decodemv.h
AV1_DX_SRCS-yes += decoder/detokenize.h
AV1_DX_SRCS-yes += decoder/dthread.c
AV1_DX_SRCS-yes += decoder/dthread.h
AV1_DX_SRCS-yes += decoder/decoder.c
AV1_DX_SRCS-yes += decoder/decoder.h
AV1_DX_SRCS-yes += decoder/dsubexp.c
AV1_DX_SRCS-yes += decoder/dsubexp.h

ifeq ($(CONFIG_PVQ),yes)
# PVQ fro daala
AV1_DX_SRCS-yes += decoder/pvq_decoder.c
AV1_DX_SRCS-yes += decoder/pvq_decoder.h
AV1_DX_SRCS-yes += decoder/decint.h
AV1_DX_SRCS-yes += decoder/generic_decoder.c
AV1_DX_SRCS-yes += decoder/laplace_decoder.c
endif

AV1_DX_SRCS-yes := $(filter-out $(AV1_DX_SRCS_REMOVE-yes),$(AV1_DX_SRCS-yes))
