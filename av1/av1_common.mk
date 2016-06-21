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


AV1_COMMON_SRCS-yes += av1_common.mk
AV1_COMMON_SRCS-yes += av1_iface_common.h
AV1_COMMON_SRCS-yes += common/alloccommon.c
AV1_COMMON_SRCS-yes += common/blockd.c
AV1_COMMON_SRCS-yes += common/debugmodes.c
AV1_COMMON_SRCS-yes += common/entropy.c
AV1_COMMON_SRCS-yes += common/entropymode.c
AV1_COMMON_SRCS-yes += common/entropymv.c
AV1_COMMON_SRCS-yes += common/frame_buffers.c
AV1_COMMON_SRCS-yes += common/frame_buffers.h
AV1_COMMON_SRCS-yes += common/alloccommon.h
AV1_COMMON_SRCS-yes += common/blockd.h
AV1_COMMON_SRCS-yes += common/common.h
AV1_COMMON_SRCS-yes += common/entropy.h
AV1_COMMON_SRCS-yes += common/entropymode.h
AV1_COMMON_SRCS-yes += common/entropymv.h
AV1_COMMON_SRCS-yes += common/enums.h
AV1_COMMON_SRCS-yes += common/filter.h
AV1_COMMON_SRCS-yes += common/filter.c
AV1_COMMON_SRCS-yes += common/convolve.c
AV1_COMMON_SRCS-yes += common/convolve.h
AV1_COMMON_SRCS-yes += common/idct.h
AV1_COMMON_SRCS-yes += common/idct.c
AV1_COMMON_SRCS-yes += common/av1_inv_txfm.h
AV1_COMMON_SRCS-yes += common/av1_inv_txfm.c
AV1_COMMON_SRCS-yes += common/loopfilter.h
AV1_COMMON_SRCS-yes += common/thread_common.h
AV1_COMMON_SRCS-yes += common/mv.h
AV1_COMMON_SRCS-yes += common/onyxc_int.h
AV1_COMMON_SRCS-yes += common/pred_common.h
AV1_COMMON_SRCS-yes += common/pred_common.c
AV1_COMMON_SRCS-yes += common/quant_common.h
AV1_COMMON_SRCS-yes += common/reconinter.h
AV1_COMMON_SRCS-yes += common/reconintra.h
AV1_COMMON_SRCS-yes += common/av1_rtcd.c
AV1_COMMON_SRCS-yes += common/av1_rtcd_defs.pl
AV1_COMMON_SRCS-yes += common/scale.h
AV1_COMMON_SRCS-yes += common/scale.c
AV1_COMMON_SRCS-yes += common/seg_common.h
AV1_COMMON_SRCS-yes += common/seg_common.c
AV1_COMMON_SRCS-yes += common/tile_common.h
AV1_COMMON_SRCS-yes += common/tile_common.c
AV1_COMMON_SRCS-yes += common/loopfilter.c
AV1_COMMON_SRCS-yes += common/thread_common.c
AV1_COMMON_SRCS-yes += common/mvref_common.c
AV1_COMMON_SRCS-yes += common/mvref_common.h
AV1_COMMON_SRCS-yes += common/quant_common.c
AV1_COMMON_SRCS-yes += common/reconinter.c
AV1_COMMON_SRCS-yes += common/reconintra.c
AV1_COMMON_SRCS-yes += common/common_data.h
AV1_COMMON_SRCS-yes += common/scan.c
AV1_COMMON_SRCS-yes += common/scan.h
# TODO(angiebird) the forward transform belongs under encoder/
AV1_COMMON_SRCS-$(CONFIG_AV1_ENCODER) += common/av1_fwd_txfm.h
AV1_COMMON_SRCS-$(CONFIG_AV1_ENCODER) += common/av1_fwd_txfm.c
AV1_COMMON_SRCS-yes += common/clpf.c
AV1_COMMON_SRCS-yes += common/clpf.h
ifeq ($(CONFIG_DERING),yes)
AV1_COMMON_SRCS-yes += common/od_dering.c
AV1_COMMON_SRCS-yes += common/od_dering.h
AV1_COMMON_SRCS-yes += common/dering.c
AV1_COMMON_SRCS-yes += common/dering.h
endif
AV1_COMMON_SRCS-yes += common/odintrin.c
AV1_COMMON_SRCS-yes += common/odintrin.h
AV1_COMMON_SRCS-yes += analyzer/av1_analyzer.c
AV1_COMMON_SRCS-yes += analyzer/av1_analyzer.h

ifneq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans4_dspr2.c
AV1_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans8_dspr2.c
AV1_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans16_dspr2.c
endif

# common (msa)
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct4x4_msa.c
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct8x8_msa.c
AV1_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct16x16_msa.c

AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/idct_intrin_sse2.c
ifeq ($(CONFIG_AV1_ENCODER),yes)
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/av1_fwd_txfm_sse2.c
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/av1_fwd_dct32x32_impl_sse2.h
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/av1_fwd_txfm_impl_sse2.h
endif

ifneq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
AV1_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht4x4_add_neon.c
AV1_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht8x8_add_neon.c
endif

AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/av1_inv_txfm_sse2.c
AV1_COMMON_SRCS-$(HAVE_SSE2) += common/x86/av1_inv_txfm_sse2.h

$(eval $(call rtcd_h_template,av1_rtcd,av1/common/av1_rtcd_defs.pl))
