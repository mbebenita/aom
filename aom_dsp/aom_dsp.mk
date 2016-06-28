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


DSP_SRCS-yes += aom_dsp.mk
DSP_SRCS-yes += aom_dsp_common.h

DSP_SRCS-$(HAVE_MSA)    += mips/macros_msa.h

# bit reader
DSP_SRCS-yes += prob.h
DSP_SRCS-yes += prob.c
DSP_SRCS-$(CONFIG_ANS) += ans.h
DSP_SRCS-$(CONFIG_ANS) += ans.c

ifeq ($(CONFIG_ENCODERS),yes)
DSP_SRCS-$(CONFIG_ANS) += answriter.h
DSP_SRCS-yes += bitwriter.h
DSP_SRCS-yes += dkboolwriter.h
DSP_SRCS-yes += dkboolwriter.c
DSP_SRCS-yes += bitwriter_buffer.c
DSP_SRCS-yes += bitwriter_buffer.h
DSP_SRCS-yes += psnr.c
DSP_SRCS-yes += psnr.h
DSP_SRCS-$(CONFIG_ANS) += buf_ans.h
DSP_SRCS-$(CONFIG_ANS) += buf_ans.c
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += ssim.c
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += ssim.h
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += psnrhvs.c
DSP_SRCS-$(CONFIG_INTERNAL_STATS) += fastssim.c
endif

ifeq ($(CONFIG_DECODERS),yes)
DSP_SRCS-$(CONFIG_ANS) += ansreader.h
DSP_SRCS-yes += bitreader.h
DSP_SRCS-yes += dkboolreader.h
DSP_SRCS-yes += dkboolreader.c
DSP_SRCS-yes += bitreader_buffer.c
DSP_SRCS-yes += bitreader_buffer.h
endif

# intra predictions
DSP_SRCS-yes += intrapred.c

ifeq ($(CONFIG_DAALA_EC),yes)
DSP_SRCS-yes += entenc.c
DSP_SRCS-yes += entenc.h
DSP_SRCS-yes += entdec.c
DSP_SRCS-yes += entdec.h
DSP_SRCS-yes += entcode.c
DSP_SRCS-yes += entcode.h
DSP_SRCS-yes += daalaboolreader.c
DSP_SRCS-yes += daalaboolreader.h
DSP_SRCS-yes += daalaboolwriter.c
DSP_SRCS-yes += daalaboolwriter.h
endif

ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE) += x86/intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/intrapred_ssse3.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_ssse3.asm
endif  # CONFIG_USE_X86INC

ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE)  += x86/highbd_intrapred_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_intrapred_sse2.asm
endif  # CONFIG_USE_X86INC
endif  # CONFIG_AOM_HIGHBITDEPTH

DSP_SRCS-$(HAVE_NEON_ASM) += arm/intrapred_neon_asm$(ASM)
DSP_SRCS-$(HAVE_NEON) += arm/intrapred_neon.c
DSP_SRCS-$(HAVE_MSA) += mips/intrapred_msa.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred4_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/intrapred16_dspr2.c

DSP_SRCS-$(HAVE_DSPR2)  += mips/common_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/common_dspr2.c

# interpolation filters
DSP_SRCS-yes += aom_convolve.c
DSP_SRCS-yes += aom_convolve.h
DSP_SRCS-yes += aom_filter.h

DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64) += x86/convolve.h
DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64) += x86/aom_asm_stubs.c
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_subpixel_8t_sse2.asm
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_subpixel_bilinear_sse2.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_ssse3.asm
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_bilinear_ssse3.asm
DSP_SRCS-$(HAVE_AVX2)  += x86/aom_subpixel_8t_intrin_avx2.c
DSP_SRCS-$(HAVE_SSSE3) += x86/aom_subpixel_8t_intrin_ssse3.c
ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_high_subpixel_8t_sse2.asm
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_high_subpixel_bilinear_sse2.asm
endif
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE2)  += x86/aom_convolve_copy_sse2.asm
endif

ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes += arm/aom_convolve_copy_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve8_avg_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve8_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve_avg_neon_asm$(ASM)
DSP_SRCS-yes += arm/aom_convolve_neon.c
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes += arm/aom_convolve_copy_neon.c
DSP_SRCS-yes += arm/aom_convolve8_avg_neon.c
DSP_SRCS-yes += arm/aom_convolve8_neon.c
DSP_SRCS-yes += arm/aom_convolve_avg_neon.c
DSP_SRCS-yes += arm/aom_convolve_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM

# common (msa)
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_horiz_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_avg_vert_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_horiz_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve8_vert_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_avg_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_copy_msa.c
DSP_SRCS-$(HAVE_MSA) += mips/aom_convolve_msa.h

# common (dspr2)
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve_common_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_avg_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_avg_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve2_vert_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_avg_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_avg_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/convolve8_vert_dspr2.c

# loop filters
DSP_SRCS-yes += loopfilter.c

DSP_SRCS-$(ARCH_X86)$(ARCH_X86_64)   += x86/loopfilter_sse2.c
DSP_SRCS-$(HAVE_AVX2)                += x86/loopfilter_avx2.c
DSP_SRCS-$(HAVE_MMX)                 += x86/loopfilter_mmx.asm

DSP_SRCS-$(HAVE_NEON)   += arm/loopfilter_neon.c
ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes  += arm/loopfilter_mb_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_16_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_8_neon$(ASM)
DSP_SRCS-yes  += arm/loopfilter_4_neon$(ASM)
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes   += arm/loopfilter_16_neon.c
DSP_SRCS-yes   += arm/loopfilter_8_neon.c
DSP_SRCS-yes   += arm/loopfilter_4_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM

DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_msa.h
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_16_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_8_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/loopfilter_4_msa.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_filters_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_filters_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_macros_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_masks_dspr2.h
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_horiz_dspr2.c
DSP_SRCS-$(HAVE_DSPR2)  += mips/loopfilter_mb_vert_dspr2.c

ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_loopfilter_sse2.c
endif  # CONFIG_AOM_HIGHBITDEPTH

DSP_SRCS-yes            += txfm_common.h
DSP_SRCS-$(HAVE_SSE2)   += x86/txfm_common_sse2.h
DSP_SRCS-$(HAVE_MSA)    += mips/txfm_macros_msa.h
# forward transform
ifneq ($(filter yes,$(CONFIG_AV1_ENCODER)),)
DSP_SRCS-yes            += fwd_txfm.c
DSP_SRCS-yes            += fwd_txfm.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_txfm_impl_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/fwd_dct32x32_impl_sse2.h
ifeq ($(ARCH_X86_64),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/fwd_txfm_ssse3_x86_64.asm
endif
endif
DSP_SRCS-$(HAVE_AVX2)   += x86/fwd_txfm_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/fwd_dct32x32_impl_avx2.h
DSP_SRCS-$(HAVE_NEON)   += arm/fwd_txfm_neon.c
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_txfm_msa.h
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_txfm_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/fwd_dct32x32_msa.c
endif  # CONFIG_AV1_ENCODER

# inverse transform
ifneq ($(filter yes,$(CONFIG_AV1)),)
DSP_SRCS-yes            += inv_txfm.h
DSP_SRCS-yes            += inv_txfm.c
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_txfm_sse2.h
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_txfm_sse2.c
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/inv_wht_sse2.asm
ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/inv_txfm_ssse3_x86_64.asm
endif  # ARCH_X86_64
endif  # CONFIG_USE_X86INC

ifeq ($(HAVE_NEON_ASM),yes)
DSP_SRCS-yes  += arm/save_reg_neon$(ASM)
DSP_SRCS-yes  += arm/idct4x4_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct4x4_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct8x8_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct8x8_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct16x16_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct16x16_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct32x32_1_add_neon$(ASM)
DSP_SRCS-yes  += arm/idct32x32_add_neon$(ASM)
else
ifeq ($(HAVE_NEON),yes)
DSP_SRCS-yes  += arm/idct4x4_1_add_neon.c
DSP_SRCS-yes  += arm/idct4x4_add_neon.c
DSP_SRCS-yes  += arm/idct8x8_1_add_neon.c
DSP_SRCS-yes  += arm/idct8x8_add_neon.c
DSP_SRCS-yes  += arm/idct16x16_1_add_neon.c
DSP_SRCS-yes  += arm/idct16x16_add_neon.c
DSP_SRCS-yes  += arm/idct32x32_1_add_neon.c
DSP_SRCS-yes  += arm/idct32x32_add_neon.c
endif  # HAVE_NEON
endif  # HAVE_NEON_ASM
DSP_SRCS-$(HAVE_NEON)  += arm/idct16x16_neon.c

DSP_SRCS-$(HAVE_MSA)   += mips/inv_txfm_msa.h
DSP_SRCS-$(HAVE_MSA)   += mips/idct4x4_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct8x8_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct16x16_msa.c
DSP_SRCS-$(HAVE_MSA)   += mips/idct32x32_msa.c

ifneq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_DSPR2) += mips/inv_txfm_dspr2.h
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans4_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans8_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans16_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans32_dspr2.c
DSP_SRCS-$(HAVE_DSPR2) += mips/itrans32_cols_dspr2.c
endif  # CONFIG_AOM_HIGHBITDEPTH
endif  # CONFIG_AV1

# quantization
ifneq ($(filter yes, $(CONFIG_AV1_ENCODER)),)
DSP_SRCS-yes            += quantize.c
DSP_SRCS-yes            += quantize.h

DSP_SRCS-$(HAVE_SSE2)   += x86/quantize_sse2.c
ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_quantize_intrin_sse2.c
endif
ifeq ($(ARCH_X86_64),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSSE3)  += x86/quantize_ssse3_x86_64.asm
DSP_SRCS-$(HAVE_AVX)    += x86/quantize_avx_x86_64.asm
endif
endif

# avg
DSP_SRCS-yes           += avg.c
DSP_SRCS-$(HAVE_SSE2)  += x86/avg_intrin_sse2.c
DSP_SRCS-$(HAVE_NEON)  += arm/avg_neon.c
DSP_SRCS-$(HAVE_MSA)   += mips/avg_msa.c
ifeq ($(ARCH_X86_64),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSSE3) += x86/avg_ssse3_x86_64.asm
endif
endif

endif  # CONFIG_AV1_ENCODER

ifeq ($(CONFIG_ENCODERS),yes)
DSP_SRCS-yes            += sad.c
DSP_SRCS-yes            += subtract.c

DSP_SRCS-$(HAVE_MEDIA)  += arm/sad_media$(ASM)
DSP_SRCS-$(HAVE_NEON)   += arm/sad4d_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/sad_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/subtract_neon.c

DSP_SRCS-$(HAVE_MSA)    += mips/sad_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/subtract_msa.c

DSP_SRCS-$(HAVE_SSE3)   += x86/sad_sse3.asm
DSP_SRCS-$(HAVE_SSSE3)  += x86/sad_ssse3.asm
DSP_SRCS-$(HAVE_SSE4_1) += x86/sad_sse4.asm
DSP_SRCS-$(HAVE_AVX2)   += x86/sad4d_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/sad_avx2.c

ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE)    += x86/sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE)    += x86/sad_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/subtract_sse2.asm

ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad_sse2.asm
endif  # CONFIG_AOM_HIGHBITDEPTH
endif  # CONFIG_USE_X86INC

endif  # CONFIG_ENCODERS

ifneq ($(filter yes,$(CONFIG_ENCODERS)),)
DSP_SRCS-yes            += variance.c
DSP_SRCS-yes            += variance.h

DSP_SRCS-$(HAVE_MEDIA)  += arm/bilinear_filter_media$(ASM)
DSP_SRCS-$(HAVE_MEDIA)  += arm/subpel_variance_media.c
DSP_SRCS-$(HAVE_MEDIA)  += arm/variance_halfpixvar16x16_h_media$(ASM)
DSP_SRCS-$(HAVE_MEDIA)  += arm/variance_halfpixvar16x16_hv_media$(ASM)
DSP_SRCS-$(HAVE_MEDIA)  += arm/variance_halfpixvar16x16_v_media$(ASM)
DSP_SRCS-$(HAVE_MEDIA)  += arm/variance_media$(ASM)
DSP_SRCS-$(HAVE_NEON)   += arm/subpel_variance_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/variance_neon.c

DSP_SRCS-$(HAVE_MSA)    += mips/variance_msa.c
DSP_SRCS-$(HAVE_MSA)    += mips/sub_pixel_variance_msa.c

DSP_SRCS-$(HAVE_MMX)    += x86/variance_mmx.c
DSP_SRCS-$(HAVE_MMX)    += x86/variance_impl_mmx.asm
DSP_SRCS-$(HAVE_SSE)    += x86/variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/variance_sse2.c  # Contains SSE2 and SSSE3
DSP_SRCS-$(HAVE_SSE2)   += x86/halfpix_variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/halfpix_variance_impl_sse2.asm
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_impl_avx2.c

ifeq ($(ARCH_X86_64),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/ssim_opt_x86_64.asm
endif  # ARCH_X86_64

ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE)    += x86/subpel_variance_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/subpel_variance_sse2.asm  # Contains SSE2 and SSSE3
endif  # CONFIG_USE_X86INC

ifeq ($(CONFIG_AOM_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_impl_sse2.asm
ifeq ($(CONFIG_USE_X86INC),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_subpel_variance_impl_sse2.asm
endif  # CONFIG_USE_X86INC
endif  # CONFIG_AOM_HIGHBITDEPTH
endif  # CONFIG_ENCODERS

DSP_SRCS-no += $(DSP_SRCS_REMOVE-yes)

DSP_SRCS-yes += aom_dsp_rtcd.c
DSP_SRCS-yes += aom_dsp_rtcd_defs.pl

$(eval $(call rtcd_h_template,aom_dsp_rtcd,aom_dsp/aom_dsp_rtcd_defs.pl))
