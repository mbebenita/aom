/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AOM_AOMCX_H_
#define AOM_AOMCX_H_

/*!\defgroup aom_encoder AOMedia AOM/AV1 Encoder
 * \ingroup aom
 *
 * @{
 */
#include "./aom.h"
#include "./aom_encoder.h"

/*!\file
 * \brief Provides definitions for using AOM or AV1 encoder algorithm within the
 *        aom Codec Interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*!\name Algorithm interface for AV1
 *
 * This interface provides the capability to encode raw AV1 streams.
 * @{
 */
extern aom_codec_iface_t aom_codec_av1_cx_algo;
extern aom_codec_iface_t *aom_codec_av1_cx(void);
/*!@} - end algorithm interface member group*/

/*
 * Algorithm Flags
 */

/*!\brief Don't reference the last frame
 *
 * When this flag is set, the encoder will not use the last frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * last frame or not automatically.
 */
#define AOM_EFLAG_NO_REF_LAST (1 << 16)

/*!\brief Don't reference the golden frame
 *
 * When this flag is set, the encoder will not use the golden frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * golden frame or not automatically.
 */
#define AOM_EFLAG_NO_REF_GF (1 << 17)

#if CONFIG_EXT_REFS
/*!\brief Don't reference the backward reference frame
 *
 * When this flag is set, the encoder will not use the bwd frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * bwd frame or not automatically.
 */
#define AOM_EFLAG_NO_REF_BRF (1 << 18)

#endif  // CONFIG_EXT_REFS

/*!\brief Don't reference the alternate reference frame
 *
 * When this flag is set, the encoder will not use the alt ref frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * alt ref frame or not automatically.
 */
#define AOM_EFLAG_NO_REF_ARF (1 << 19)

/*!\brief Don't update the last frame
 *
 * When this flag is set, the encoder will not update the last frame with
 * the contents of the current frame.
 */
#define AOM_EFLAG_NO_UPD_LAST (1 << 20)

/*!\brief Don't update the golden frame
 *
 * When this flag is set, the encoder will not update the golden frame with
 * the contents of the current frame.
 */
#define AOM_EFLAG_NO_UPD_GF (1 << 21)

#if CONFIG_EXT_REFS
/*!\brief Don't update the backward reference frame
 *
 * When this flag is set, the encoder will not update the bwd frame with
 * the contents of the current frame.
 */
#define AOM_EFLAG_NO_UPD_BRF (1 << 22)

#endif  // CONFIG_EXT_REFS

/*!\brief Don't update the alternate reference frame
 *
 * When this flag is set, the encoder will not update the alt ref frame with
 * the contents of the current frame.
 */
#define AOM_EFLAG_NO_UPD_ARF (1 << 23)

/*!\brief Force golden frame update
 *
 * When this flag is set, the encoder copy the contents of the current frame
 * to the golden frame buffer.
 */
#define AOM_EFLAG_FORCE_GF (1 << 24)

#if CONFIG_EXT_REFS
/*!\brief Force backward reference frame update
 *
 * When this flag is set, the encoder copy the contents of the current frame
 * to the bwd frame buffer.
 */
#define AOM_EFLAG_FORCE_BRF (1 << 25)
#endif  // CONFIG_EXT_REFS

/*!\brief Force alternate reference frame update
 *
 * When this flag is set, the encoder copy the contents of the current frame
 * to the alternate reference frame buffer.
 */
#define AOM_EFLAG_FORCE_ARF (1 << 26)

/*!\brief Disable entropy update
 *
 * When this flag is set, the encoder will not update its internal entropy
 * model based on the entropy of this frame.
 */
#define AOM_EFLAG_NO_UPD_ENTROPY (1 << 27)

/*!\brief AVx encoder control functions
 *
 * This set of macros define the control functions available for AVx
 * encoder interface.
 *
 * \sa #aom_codec_control
 */
enum aome_enc_control_id {
  /*!\brief Codec control function to pass an ROI map to encoder.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_ROI_MAP = 8,

  /*!\brief Codec control function to pass an Active map to encoder.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_ACTIVEMAP,

  /*!\brief Codec control function to set encoder scaling mode.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_SCALEMODE = 11,

  /*!\brief Codec control function to set encoder internal speed settings.
   *
   * Changes in this value influences, among others, the encoder's selection
   * of motion estimation methods. Values greater than 0 will increase encoder
   * speed at the expense of quality.
   *
   * \note Valid range for AOM: -16..16
   * \note Valid range for AV1: -8..8
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_CPUUSED = 13,

  /*!\brief Codec control function to enable automatic set and use alf frames.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_ENABLEAUTOALTREF,

  /*!\brief control function to set noise sensitivity
   *
   * 0: off, 1: OnYOnly, 2: OnYUV,
   * 3: OnYUVAggressive, 4: Adaptive
   *
   * Supported in codecs: AOM
   */
  AOME_SET_NOISE_SENSITIVITY,

  /*!\brief Codec control function to set sharpness.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_SHARPNESS,

  /*!\brief Codec control function to set the threshold for MBs treated static.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_STATIC_THRESHOLD,

  /*!\brief Codec control function to set the number of token partitions.
   *
   * Supported in codecs: AOM
   */
  AOME_SET_TOKEN_PARTITIONS,

  /*!\brief Codec control function to get last quantizer chosen by the encoder.
   *
   * Return value uses internal quantizer scale defined by the codec.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_GET_LAST_QUANTIZER,

  /*!\brief Codec control function to get last quantizer chosen by the encoder.
   *
   * Return value uses the 0..63 scale as used by the rc_*_quantizer config
   * parameters.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_GET_LAST_QUANTIZER_64,

  /*!\brief Codec control function to set the max no of frames to create arf.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_ARNR_MAXFRAMES,

  /*!\brief Codec control function to set the filter strength for the arf.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_ARNR_STRENGTH,

  /*!\deprecated control function to set the filter type to use for the arf. */
  AOME_SET_ARNR_TYPE,

  /*!\brief Codec control function to set visual tuning.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_TUNING,

  /*!\brief Codec control function to set constrained quality level.
   *
   * \attention For this value to be used aom_codec_enc_cfg_t::g_usage must be
   *            set to #AOM_CQ.
   * \note Valid range: 0..63
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_CQ_LEVEL,

  /*!\brief Codec control function to set Max data rate for Intra frames.
   *
   * This value controls additional clamping on the maximum size of a
   * keyframe. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allocate no more than 4.5 frames worth of bitrate
   * to a keyframe, set this to 450.
   *
   * Supported in codecs: AOM, AV1
   */
  AOME_SET_MAX_INTRA_BITRATE_PCT,

  /*!\brief Codec control function to set reference and update frame flags.
   *
   *  Supported in codecs: AOM
   */
  AOME_SET_FRAME_FLAGS,

  /*!\brief Codec control function to set max data rate for Inter frames.
   *
   * This value controls additional clamping on the maximum size of an
   * inter frame. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allow no more than 4.5 frames worth of bitrate
   * to an inter frame, set this to 450.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_MAX_INTER_BITRATE_PCT,

  /*!\brief Boost percentage for Golden Frame in CBR mode.
   *
   * This value controls the amount of boost given to Golden Frame in
   * CBR mode. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * the feature is off, i.e., no golden frame boost in CBR mode and
   * average bitrate target is used.
   *
   * For example, to allow 100% more bits, i.e, 2X, in a golden frame
   * than average frame, set this to 100.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_GF_CBR_BOOST_PCT,

  /*!\brief Codec control function to set the temporal layer id.
   *
   * For temporal scalability: this control allows the application to set the
   * layer id for each frame to be encoded. Note that this control must be set
   * for every frame prior to encoding. The usage of this control function
   * supersedes the internal temporal pattern counter, which is now deprecated.
   *
   * Supported in codecs: AOM
   */
  AOME_SET_TEMPORAL_LAYER_ID,

  /*!\brief Codec control function to set encoder screen content mode.
   *
   * 0: off, 1: On, 2: On with more aggressive rate control.
   *
   * Supported in codecs: AOM
   */
  AOME_SET_SCREEN_CONTENT_MODE,

  /*!\brief Codec control function to set lossless encoding mode.
   *
   * AV1 can operate in lossless encoding mode, in which the bitstream
   * produced will be able to decode and reconstruct a perfect copy of
   * input source. This control function provides a mean to switch encoder
   * into lossless coding mode(1) or normal coding mode(0) that may be lossy.
   *                          0 = lossy coding mode
   *                          1 = lossless coding mode
   *
   *  By default, encoder operates in normal coding mode (maybe lossy).
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_LOSSLESS,
#if CONFIG_AOM_QM
  /*!\brief Codec control function to encode with quantisation matrices.
   *
   * AOM can operate with default quantisation matrices dependent on
   * quantisation level and block type.
   *                          0 = do not use quantisation matrices
   *                          1 = use quantisation matrices
   *
   *  By default, the encoder operates without quantisation matrices.
   *
   * Supported in codecs: AOM
   */

  AV1E_SET_ENABLE_QM,

  /*!\brief Codec control function to set the min quant matrix flatness.
   *
   * AOM can operate with different ranges of quantisation matrices.
   * As quantisation levels increase, the matrices get flatter. This
   * control sets the minimum level of flatness from which the matrices
   * are determined.
   *
   *  By default, the encoder sets this minimum at half the available
   *  range.
   *
   * Supported in codecs: AOM
   */
  AV1E_SET_QM_MIN,

  /*!\brief Codec control function to set the max quant matrix flatness.
   *
   * AOM can operate with different ranges of quantisation matrices.
   * As quantisation levels increase, the matrices get flatter. This
   * control sets the maximum level of flatness possible.
   *
   * By default, the encoder sets this maximum at the top of the
   * available range.
   *
   * Supported in codecs: AOM
   */
  AV1E_SET_QM_MAX,
#endif

  /*!\brief Codec control function to set number of tile columns.
   *
   * In encoding and decoding, AV1 allows an input image frame be partitioned
   * into separated vertical tile columns, which can be encoded or decoded
   * independently. This enables easy implementation of parallel encoding and
   * decoding. This control requests the encoder to use column tiles in
   * encoding an input frame, with number of tile columns (in Log2 unit) as
   * the parameter:
   *             0 = 1 tile column
   *             1 = 2 tile columns
   *             2 = 4 tile columns
   *             .....
   *             n = 2**n tile columns
   * The requested tile columns will be capped by encoder based on image size
   * limitation (The minimum width of a tile column is 256 pixel, the maximum
   * is 4096).
   *
   * By default, the value is 0, i.e. one single column tile for entire image.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_TILE_COLUMNS,

  /*!\brief Codec control function to set number of tile rows.
   *
   * In encoding and decoding, AV1 allows an input image frame be partitioned
   * into separated horizontal tile rows. Tile rows are encoded or decoded
   * sequentially. Even though encoding/decoding of later tile rows depends on
   * earlier ones, this allows the encoder to output data packets for tile rows
   * prior to completely processing all tile rows in a frame, thereby reducing
   * the latency in processing between input and output. The parameter
   * for this control describes the number of tile rows, which has a valid
   * range [0, 2]:
   *            0 = 1 tile row
   *            1 = 2 tile rows
   *            2 = 4 tile rows
   *
   * By default, the value is 0, i.e. one single row tile for entire image.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_TILE_ROWS,

  /*!\brief Codec control function to enable frame parallel decoding feature.
   *
   * AV1 has a bitstream feature to reduce decoding dependency between frames
   * by turning off backward update of probability context used in encoding
   * and decoding. This allows staged parallel processing of more than one
   * video frames in the decoder. This control function provides a mean to
   * turn this feature on or off for bitstreams produced by encoder.
   *
   * By default, this feature is off.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_FRAME_PARALLEL_DECODING,

  /*!\brief Codec control function to set adaptive quantization mode.
   *
   * AV1 has a segment based feature that allows encoder to adaptively change
   * quantization parameter for each segment within a frame to improve the
   * subjective quality. This control makes encoder operate in one of the
   * several AQ_modes supported.
   *
   * By default, encoder operates with AQ_Mode 0(adaptive quantization off).
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_AQ_MODE,

  /*!\brief Codec control function to enable/disable periodic Q boost.
   *
   * One AV1 encoder speed feature is to enable quality boost by lowering
   * frame level Q periodically. This control function provides a mean to
   * turn on/off this feature.
   *               0 = off
   *               1 = on
   *
   * By default, the encoder is allowed to use this feature for appropriate
   * encoding modes.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_FRAME_PERIODIC_BOOST,

  /*!\brief Codec control function to set noise sensitivity.
   *
   *  0: off, 1: On(YOnly)
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_NOISE_SENSITIVITY,

  /*!\brief Codec control function to set content type.
   * \note Valid parameter range:
   *              AOM_CONTENT_DEFAULT = Regular video content (Default)
   *              AOM_CONTENT_SCREEN  = Screen capture content
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_TUNE_CONTENT,

  /*!\brief Codec control function to register callback to get per layer packet.
   * \note Parameter for this control function is a structure with a callback
   *       function and a pointer to private data used by the callback.
   *
   * Supported in codecs: AV1
   */
  AV1E_REGISTER_CX_CALLBACK,

  /*!\brief Codec control function to set color space info.
   * \note Valid ranges: 0..7, default is "UNKNOWN".
   *                     0 = UNKNOWN,
   *                     1 = BT_601
   *                     2 = BT_709
   *                     3 = SMPTE_170
   *                     4 = SMPTE_240
   *                     5 = BT_2020
   *                     6 = RESERVED
   *                     7 = SRGB
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_COLOR_SPACE,

  /*!\brief Codec control function to set temporal layering mode.
   * \note Valid ranges: 0..3, default is "0"
   * (AV1E_TEMPORAL_LAYERING_MODE_NOLAYERING).
   *                     0 = AV1E_TEMPORAL_LAYERING_MODE_NOLAYERING
   *                     1 = AV1E_TEMPORAL_LAYERING_MODE_BYPASS
   *                     2 = AV1E_TEMPORAL_LAYERING_MODE_0101
   *                     3 = AV1E_TEMPORAL_LAYERING_MODE_0212
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_TEMPORAL_LAYERING_MODE,

  /*!\brief Codec control function to set minimum interval between GF/ARF frames
   *
   * By default the value is set as 4.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_MIN_GF_INTERVAL,

  /*!\brief Codec control function to set minimum interval between GF/ARF frames
   *
   * By default the value is set as 16.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_MAX_GF_INTERVAL,

  /*!\brief Codec control function to get an Active map back from the encoder.
   *
   * Supported in codecs: AV1
   */
  AV1E_GET_ACTIVEMAP,

  /*!\brief Codec control function to set color range bit.
   * \note Valid ranges: 0..1, default is 0
   *                     0 = Limited range (16..235 or HBD equivalent)
   *                     1 = Full range (0..255 or HBD equivalent)
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_COLOR_RANGE,

  /*!\brief Codec control function to set intended rendering image size.
   *
   * By default, this is identical to the image size in pixels.
   *
   * Supported in codecs: AV1
   */
  AV1E_SET_RENDER_SIZE,
};

/*!\brief aom 1-D scaling mode
 *
 * This set of constants define 1-D aom scaling modes
 */
typedef enum aom_scaling_mode_1d {
  AOME_NORMAL = 0,
  AOME_FOURFIVE = 1,
  AOME_THREEFIVE = 2,
  AOME_ONETWO = 3
} AOM_SCALING_MODE;

/*!\brief Temporal layering mode enum for AV1 SVC.
 *
 * This set of macros define the different temporal layering modes.
 * Supported codecs: AV1 (in SVC mode)
 *
 */
typedef enum av1e_temporal_layering_mode {
  /*!\brief No temporal layering.
   * Used when only spatial layering is used.
   */
  AV1E_TEMPORAL_LAYERING_MODE_NOLAYERING = 0,

  /*!\brief Bypass mode.
   * Used when application needs to control temporal layering.
   * This will only work when the number of spatial layers equals 1.
   */
  AV1E_TEMPORAL_LAYERING_MODE_BYPASS = 1,

  /*!\brief 0-1-0-1... temporal layering scheme with two temporal layers.
   */
  AV1E_TEMPORAL_LAYERING_MODE_0101 = 2,

  /*!\brief 0-2-1-2... temporal layering scheme with three temporal layers.
   */
  AV1E_TEMPORAL_LAYERING_MODE_0212 = 3
} AV1E_TEMPORAL_LAYERING_MODE;

/*!\brief  aom region of interest map
 *
 * These defines the data structures for the region of interest map
 *
 */

typedef struct aom_roi_map {
  /*! An id between 0 and 3 for each 16x16 region within a frame. */
  unsigned char *roi_map;
  unsigned int rows; /**< Number of rows. */
  unsigned int cols; /**< Number of columns. */
  // TODO(paulwilkins): broken for AV1 which has 8 segments
  // q and loop filter deltas for each segment
  // (see MAX_MB_SEGMENTS)
  int delta_q[4];  /**< Quantizer deltas. */
  int delta_lf[4]; /**< Loop filter deltas. */
  /*! Static breakout threshold for each segment. */
  unsigned int static_threshold[4];
} aom_roi_map_t;

/*!\brief  aom active region map
 *
 * These defines the data structures for active region map
 *
 */

typedef struct aom_active_map {
  unsigned char
      *active_map; /**< specify an on (1) or off (0) each 16x16 region within a
                      frame */
  unsigned int rows; /**< number of rows */
  unsigned int cols; /**< number of cols */
} aom_active_map_t;

/*!\brief  aom image scaling mode
 *
 * This defines the data structure for image scaling mode
 *
 */
typedef struct aom_scaling_mode {
  AOM_SCALING_MODE h_scaling_mode; /**< horizontal scaling mode */
  AOM_SCALING_MODE v_scaling_mode; /**< vertical scaling mode   */
} aom_scaling_mode_t;

/*!\brief AOM token partition mode
 *
 * This defines AOM partitioning mode for compressed data, i.e., the number of
 * sub-streams in the bitstream. Used for parallelized decoding.
 *
 */

typedef enum {
  AOM_ONE_TOKENPARTITION = 0,
  AOM_TWO_TOKENPARTITION = 1,
  AOM_FOUR_TOKENPARTITION = 2,
  AOM_EIGHT_TOKENPARTITION = 3
} aome_token_partitions;

/*!brief AV1 encoder content type */
typedef enum {
  AOM_CONTENT_DEFAULT,
  AOM_CONTENT_SCREEN,
  AOM_CONTENT_INVALID
} aom_tune_content;

/*!\brief AOM model tuning parameters
 *
 * Changes the encoder to tune for certain types of input material.
 *
 */
typedef enum { AOM_TUNE_PSNR, AOM_TUNE_SSIM } aom_tune_metric;

/*!\cond */
/*!\brief AOM encoder control function parameter type
 *
 * Defines the data types that AOME control functions take. Note that
 * additional common controls are defined in aom.h
 *
 */

AOM_CTRL_USE_TYPE(AOME_SET_FRAME_FLAGS, int)
#define AOM_CTRL_AOME_SET_FRAME_FLAGS
AOM_CTRL_USE_TYPE(AOME_SET_TEMPORAL_LAYER_ID, int)
#define AOM_CTRL_AOME_SET_TEMPORAL_LAYER_ID
AOM_CTRL_USE_TYPE(AOME_SET_ROI_MAP, aom_roi_map_t *)
#define AOM_CTRL_AOME_SET_ROI_MAP
AOM_CTRL_USE_TYPE(AOME_SET_ACTIVEMAP, aom_active_map_t *)
#define AOM_CTRL_AOME_SET_ACTIVEMAP
AOM_CTRL_USE_TYPE(AOME_SET_SCALEMODE, aom_scaling_mode_t *)
#define AOM_CTRL_AOME_SET_SCALEMODE

AOM_CTRL_USE_TYPE(AV1E_SET_SVC, int)
#define AOM_CTRL_AV1E_SET_SVC
AOM_CTRL_USE_TYPE(AV1E_SET_SVC_PARAMETERS, void *)
#define AOM_CTRL_AV1E_SET_SVC_PARAMETERS
AOM_CTRL_USE_TYPE(AV1E_REGISTER_CX_CALLBACK, void *)
#define AOM_CTRL_AV1E_REGISTER_CX_CALLBACK

AOM_CTRL_USE_TYPE(AOME_SET_CPUUSED, int)
#define AOM_CTRL_AOME_SET_CPUUSED
AOM_CTRL_USE_TYPE(AOME_SET_ENABLEAUTOALTREF, unsigned int)
#define AOM_CTRL_AOME_SET_ENABLEAUTOALTREF
AOM_CTRL_USE_TYPE(AOME_SET_NOISE_SENSITIVITY, unsigned int)
#define AOM_CTRL_AOME_SET_NOISE_SENSITIVITY
AOM_CTRL_USE_TYPE(AOME_SET_SHARPNESS, unsigned int)
#define AOM_CTRL_AOME_SET_SHARPNESS
AOM_CTRL_USE_TYPE(AOME_SET_STATIC_THRESHOLD, unsigned int)
#define AOM_CTRL_AOME_SET_STATIC_THRESHOLD
AOM_CTRL_USE_TYPE(AOME_SET_TOKEN_PARTITIONS, int) /* aome_token_partitions */
#define AOM_CTRL_AOME_SET_TOKEN_PARTITIONS

AOM_CTRL_USE_TYPE(AOME_SET_ARNR_MAXFRAMES, unsigned int)
#define AOM_CTRL_AOME_SET_ARNR_MAXFRAMES
AOM_CTRL_USE_TYPE(AOME_SET_ARNR_STRENGTH, unsigned int)
#define AOM_CTRL_AOME_SET_ARNR_STRENGTH
AOM_CTRL_USE_TYPE_DEPRECATED(AOME_SET_ARNR_TYPE, unsigned int)
#define AOM_CTRL_AOME_SET_ARNR_TYPE
AOM_CTRL_USE_TYPE(AOME_SET_TUNING, int) /* aom_tune_metric */
#define AOM_CTRL_AOME_SET_TUNING
AOM_CTRL_USE_TYPE(AOME_SET_CQ_LEVEL, unsigned int)
#define AOM_CTRL_AOME_SET_CQ_LEVEL

AOM_CTRL_USE_TYPE(AV1E_SET_TILE_COLUMNS, int)
#define AOM_CTRL_AV1E_SET_TILE_COLUMNS
AOM_CTRL_USE_TYPE(AV1E_SET_TILE_ROWS, int)
#define AOM_CTRL_AV1E_SET_TILE_ROWS

AOM_CTRL_USE_TYPE(AOME_GET_LAST_QUANTIZER, int *)
#define AOM_CTRL_AOME_GET_LAST_QUANTIZER
AOM_CTRL_USE_TYPE(AOME_GET_LAST_QUANTIZER_64, int *)
#define AOM_CTRL_AOME_GET_LAST_QUANTIZER_64

AOM_CTRL_USE_TYPE(AOME_SET_MAX_INTRA_BITRATE_PCT, unsigned int)
#define AOM_CTRL_AOME_SET_MAX_INTRA_BITRATE_PCT
AOM_CTRL_USE_TYPE(AOME_SET_MAX_INTER_BITRATE_PCT, unsigned int)
#define AOM_CTRL_AOME_SET_MAX_INTER_BITRATE_PCT

AOM_CTRL_USE_TYPE(AOME_SET_SCREEN_CONTENT_MODE, unsigned int)
#define AOM_CTRL_AOME_SET_SCREEN_CONTENT_MODE

AOM_CTRL_USE_TYPE(AV1E_SET_GF_CBR_BOOST_PCT, unsigned int)
#define AOM_CTRL_AV1E_SET_GF_CBR_BOOST_PCT

AOM_CTRL_USE_TYPE(AV1E_SET_LOSSLESS, unsigned int)
#define AOM_CTRL_AV1E_SET_LOSSLESS

#if CONFIG_AOM_QM
AOM_CTRL_USE_TYPE(AV1E_SET_ENABLE_QM, unsigned int)
#define AOM_CTRL_AV1E_SET_ENABLE_QM

AOM_CTRL_USE_TYPE(AV1E_SET_QM_MIN, unsigned int)
#define AOM_CTRL_AV1E_SET_QM_MIN

AOM_CTRL_USE_TYPE(AV1E_SET_QM_MAX, unsigned int)
#define AOM_CTRL_AV1E_SET_QM_MAX
#endif

AOM_CTRL_USE_TYPE(AV1E_SET_FRAME_PARALLEL_DECODING, unsigned int)
#define AOM_CTRL_AV1E_SET_FRAME_PARALLEL_DECODING

AOM_CTRL_USE_TYPE(AV1E_SET_AQ_MODE, unsigned int)
#define AOM_CTRL_AV1E_SET_AQ_MODE

AOM_CTRL_USE_TYPE(AV1E_SET_FRAME_PERIODIC_BOOST, unsigned int)
#define AOM_CTRL_AV1E_SET_FRAME_PERIODIC_BOOST

AOM_CTRL_USE_TYPE(AV1E_SET_NOISE_SENSITIVITY, unsigned int)
#define AOM_CTRL_AV1E_SET_NOISE_SENSITIVITY

AOM_CTRL_USE_TYPE(AV1E_SET_TUNE_CONTENT, int) /* aom_tune_content */
#define AOM_CTRL_AV1E_SET_TUNE_CONTENT

AOM_CTRL_USE_TYPE(AV1E_SET_COLOR_SPACE, int)
#define AOM_CTRL_AV1E_SET_COLOR_SPACE

AOM_CTRL_USE_TYPE(AV1E_SET_MIN_GF_INTERVAL, unsigned int)
#define AOM_CTRL_AV1E_SET_MIN_GF_INTERVAL

AOM_CTRL_USE_TYPE(AV1E_SET_MAX_GF_INTERVAL, unsigned int)
#define AOM_CTRL_AV1E_SET_MAX_GF_INTERVAL

AOM_CTRL_USE_TYPE(AV1E_GET_ACTIVEMAP, aom_active_map_t *)
#define AOM_CTRL_AV1E_GET_ACTIVEMAP

AOM_CTRL_USE_TYPE(AV1E_SET_COLOR_RANGE, int)
#define AOM_CTRL_AV1E_SET_COLOR_RANGE

AOM_CTRL_USE_TYPE(AV1E_SET_RENDER_SIZE, int *)
#define AOM_CTRL_AV1E_SET_RENDER_SIZE

/*!\endcond */
/*! @} - end defgroup aom_encoder */
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOMCX_H_
