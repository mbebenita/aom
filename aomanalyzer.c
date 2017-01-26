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

// Analyzer Decoder
// ==============
//
// This is a simple decoder loop. It takes an input file containing the
// compressed data (in IVF format), passes it through the decoder, and writes
// a variety of bit stream data to stdout.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./args.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"

#include "../tools_common.h"
#include "../video_reader.h"
#include "./aom_config.h"
#include "av1/av1_dx_iface.c"
#include "../av1/common/onyxc_int.h"
#if CONFIG_ACCOUNTING
#include "../av1/common/accounting.h"
#endif
#include "../av1/analyzer/analyzer.h"
#include "../video_common.h"

typedef enum {
  ACCOUNTING_LAYER = 1,
  BLOCK_SIZE_LAYER = 1 << 1,
  TRANSFORM_SIZE_LAYER = 1 << 2,
  TRANSFORM_TYPE_LAYER = 1 << 3,
  MODE_LAYER = 1 << 4,
  SKIP_LAYER = 1 << 5,
  FILTER_LAYER = 1 << 6,
  DERING_GAIN_LAYER = 1 << 7,
  REFERENCE_FRAME_LAYER = 1 << 8,
  ALL_LAYERS = (1 << 9) - 1
} LayerType;

static LayerType layers = 0;  // ALL_LAYERS;

static int pretty = 0;
static int stop_after = 0;

static const arg_def_t limit_arg =
    ARG_DEF(NULL, "limit", 1, "Stop decoding after n frames");
static const arg_def_t dump_all_arg = ARG_DEF("A", "all", 0, "Dump All");
static const arg_def_t dump_accounting_arg =
    ARG_DEF("a", "accounting", 0, "Dump Accounting");
static const arg_def_t dump_block_size_arg =
    ARG_DEF("bs", "blockSize", 0, "Dump Block Size");
static const arg_def_t dump_transform_size_arg =
    ARG_DEF("ts", "transformSize", 0, "Dump Transform Size");
static const arg_def_t dump_transform_type_arg =
    ARG_DEF("tt", "transformType", 0, "Dump Transform Type");
static const arg_def_t dump_mode_arg = ARG_DEF("m", "mode", 0, "Dump Mode");
static const arg_def_t dump_skip_arg = ARG_DEF("s", "skip", 0, "Dump Skip");
static const arg_def_t dump_filter_arg =
    ARG_DEF("f", "filter", 0, "Dump Filter");
static const arg_def_t dump_dering_gain_arg =
    ARG_DEF("d", "dering", 0, "Dump Dering Gain");
static const arg_def_t dump_reference_frame_arg =
    ARG_DEF("r", "referenceFrame", 0, "Dump Reference Frame");
static const arg_def_t pretty_arg =
    ARG_DEF("p", "pretty", 0, "Pretty ANSI Colors");
static const arg_def_t usage_arg = ARG_DEF("h", "help", 0, "Help");

static const arg_def_t *main_args[] = { &limit_arg,
                                        &dump_all_arg,
#if CONFIG_ACCOUNTING
                                        &dump_accounting_arg,
#endif
                                        &dump_block_size_arg,
                                        &dump_transform_size_arg,
                                        &dump_transform_type_arg,
                                        &dump_mode_arg,
                                        &dump_skip_arg,
                                        &dump_filter_arg,
                                        &dump_dering_gain_arg,
                                        &dump_reference_frame_arg,
                                        &usage_arg,
                                        NULL };

static const char *exec_name;

int frame_count = 0;
int decoded_frame_count = 0;
aom_codec_ctx_t codec;
AvxVideoReader *reader = NULL;
const AvxInterface *decoder = NULL;
const AvxVideoInfo *info = NULL;
aom_image_t *img = NULL;
AV1_COMMON *cm = NULL;
struct AV1Decoder *pbi = NULL;

void on_frame_decoded();

AnalyzerData analyzer_data;

void init_analyzer() {
  const int aligned_width = ALIGN_POWER_OF_TWO(info->frame_width, MI_SIZE_LOG2);
  const int aligned_height =
      ALIGN_POWER_OF_TWO(info->frame_height, MI_SIZE_LOG2);
  const int mi_cols = aligned_width >> MI_SIZE_LOG2;
  const int mi_rows = aligned_height >> MI_SIZE_LOG2;
  const int mi_length = mi_cols * mi_rows;
  analyzer_data.mi_grid.buffer = aom_malloc(sizeof(AnalyzerMI) * mi_length);
  analyzer_data.mi_grid.length = mi_length;
  analyzer_data.ready = on_frame_decoded;
}

int print_list(char *buffer, const char **list) {
  char *buf = buffer;
  const char **name = list;
  while (*name != NULL) {
    buf += sprintf(buf, "\"%s\"", *name);
    name++;
    if (*name != NULL) {
      buf += sprintf(buf, ", ");
    }
  }
  return buf - buffer;
}

int print_block_info(char *buffer, const char **map, const char *name,
                     size_t offset) {
  const int mi_rows = analyzer_data.mi_rows;
  const int mi_cols = analyzer_data.mi_cols;
  char *buf = buffer;
  int r, c, v;
  if (map) {
    buf += sprintf(buf, "  \"%sMap\": [", name);
    buf += print_list(buf, map);
    buf += sprintf(buf, "],\n");
  }
  buf += sprintf(buf, "  \"%s\": [", name);
  for (r = 0; r < mi_rows; ++r) {
    buf += sprintf(buf, "[");
    for (c = 0; c < mi_cols; ++c) {
      AnalyzerMI *mi = &analyzer_data.mi_grid.buffer[r * mi_cols + c];
      v = *(((int8_t *)mi) + offset);
      if (c) buf += sprintf(buf, ",");
      buf += sprintf(buf, "%d", v);
    }
    buf += sprintf(buf, "]");
    if (r < mi_rows - 1) buf += sprintf(buf, ",");
  }
  buf += sprintf(buf, "],\n");
  return buf - buffer;
}

#if CONFIG_EXT_REFS
const char *refs_map[] = { "INTRA_FRAME",  "LAST_FRAME",
                           "LAST2_FRAME",  "LAST3_FRAME",
                           "GOLDEN_FRAME", "BWDREF_FRAME",
                           "ALTREF_FRAME", NULL };
#else
const char *refs_map[] = { "GOLDEN_FRAME", "ALTREF_FRAME", NULL };
#endif

const char *block_size_map[] = {
#if CONFIG_CB4X4
  "BLOCK_2X2",
  "BLOCK_2X4",
  "BLOCK_4X2",
#endif
  "BLOCK_4X4",
  "BLOCK_4X8",
  "BLOCK_8X4",
  "BLOCK_8X8",
  "BLOCK_8X16",
  "BLOCK_16X8",
  "BLOCK_16X16",
  "BLOCK_16X32",
  "BLOCK_32X16",
  "BLOCK_32X32",
  "BLOCK_32X64",
  "BLOCK_64X32",
  "BLOCK_64X64",
#if CONFIG_EXT_PARTITION
  "BLOCK_64X128",
  "BLOCK_128X64",
  "BLOCK_128X128",
#endif
  NULL
};

const char *transform_size_map[] = {
#if CONFIG_CB4X4
  "TX_2X2"
#endif
  "TX_4X4",
  "TX_8X8", "TX_16X16", "TX_32X32",
#if CONFIG_TX64X64
  "TX_64X64",
#endif
  "TX_4X8", "TX_8X4", "TX_8X16", "TX_16X8", "TX_16X32", "TX_32X16", "TX_4X16",
  "TX_16X4", "TX_8X32", "TX_32X8", NULL
};

const char *transform_type_map[] = { "DCT_DCT",
                                     "ADST_DCT",
                                     "DCT_ADST",
                                     "ADST_ADST",
#if CONFIG_EXT_TX
                                     "FLIPADST_DCT",
                                     "DCT_FLIPADST",
                                     "FLIPADST_FLIPADST",
                                     "ADST_FLIPADST",
                                     "FLIPADST_ADST",
                                     "IDTX",
                                     "V_DCT",
                                     "H_DCT",
                                     "V_ADST",
                                     "H_ADST",
                                     "V_FLIPADST",
                                     "H_FLIPADST",
#endif
                                     NULL };

const char *prediction_mode_map[] = { "DC_PRED",
                                      "V_PRED",
                                      "H_PRED",
                                      "D45_PRED",
                                      "D135_PRED",
                                      "D117_PRED",
                                      "D153_PRED",
                                      "D207_PRED",
                                      "D63_PRED",
#if CONFIG_ALT_INTRA
                                      "SMOOTH_PRED",
#endif
                                      "TM_PRED",
                                      "NEARESTMV",
                                      "NEARMV",
                                      "ZEROMV",
                                      "NEWMV",
#if CONFIG_EXT_INTER
                                      "NEWFROMNEARMV",
                                      "NEAREST_NEARESTMV",
                                      "NEAREST_NEARMV",
                                      "NEAR_NEARESTMV",
                                      "NEAR_NEARMV",
                                      "NEAREST_NEWMV",
                                      "NEW_NEARESTMV",
                                      "NEAR_NEWMV",
                                      "NEW_NEARMV",
                                      "ZERO_ZEROMV",
                                      "NEW_NEWMV",
#endif
                                      NULL };

const char *skip_map[] = { "NO SKIP", "SKIP", NULL };

#if CONFIG_ACCOUNTING
int print_accounting(char *buffer) {
  char *buf = buffer;
  int i;
  const Accounting *accounting = analyzer_data.accounting;
  const int num_syms = accounting->syms.num_syms;
  const int num_strs = accounting->syms.dictionary.num_strs;
  buf += sprintf(buf, "  \"symbolsMap\": [");
  for (i = 0; i < num_strs; i++) {
    buf += sprintf(buf, "\"%s\"", accounting->syms.dictionary.strs[i]);
    if (i < num_strs - 1) buf += sprintf(buf, ",");
  }
  buf += sprintf(buf, "],\n");
  buf += sprintf(buf, "  \"symbols\": [\n    ");
  AccountingSymbolContext context;
  context.x = -2;
  context.y = -2;
  AccountingSymbol *sym;
  for (i = 0; i < num_syms; i++) {
    sym = &accounting->syms.syms[i];
    if (memcmp(&context, &sym->context, sizeof(AccountingSymbolContext)) ==
        -1) {
      buf += sprintf(buf, "[%d,%d]", sym->context.x, sym->context.y);
    } else {
      buf += sprintf(buf, "[%d,%d,%d]", sym->id, sym->bits, sym->samples);
    }
    context = sym->context;
    if (i < num_syms - 1) buf += sprintf(buf, ",");
  }
  buf += sprintf(buf, "],\n");
  return buf - buffer;
}
#endif

void on_frame_decoded() {
  const int MAX_BUFFER = 1024 * 1024;
  char *buffer = aom_malloc(MAX_BUFFER);
  char *buf = buffer;

  aom_codec_control(&codec, ANALYZER_SET_DATA, &analyzer_data);
  buf += sprintf(buf, "{\n");
  if (layers & BLOCK_SIZE_LAYER)
    buf += print_block_info(buf, block_size_map, "blockSize",
                            offsetof(AnalyzerMI, block_size));
  if (layers & TRANSFORM_SIZE_LAYER)
    buf += print_block_info(buf, transform_size_map, "transformSize",
                            offsetof(AnalyzerMI, transform_size));
  if (layers & TRANSFORM_TYPE_LAYER)
    buf += print_block_info(buf, transform_type_map, "transformType",
                            offsetof(AnalyzerMI, transform_type));
  if (layers & MODE_LAYER)
    buf += print_block_info(buf, prediction_mode_map, "mode",
                            offsetof(AnalyzerMI, mode));
  if (layers & SKIP_LAYER)
    buf += print_block_info(buf, skip_map, "skip", offsetof(AnalyzerMI, skip));
  if (layers & FILTER_LAYER)
    buf += print_block_info(buf, NULL, "filter", offsetof(AnalyzerMI, filter));
  if (layers & DERING_GAIN_LAYER)
    buf += print_block_info(buf, NULL, "deringGain",
                            offsetof(AnalyzerMI, dering_gain));
  if (layers & REFERENCE_FRAME_LAYER) {
    buf += print_block_info(buf, refs_map, "referenceFrame0",
                            offsetof(AnalyzerMI, mv_reference_frame[0]));
    buf += print_block_info(buf, refs_map, "referenceFrame1",
                            offsetof(AnalyzerMI, mv_reference_frame[1]));
  }
#if CONFIG_ACCOUNTING
  if (layers & ACCOUNTING_LAYER) buf += print_accounting(buf);
#endif
  buf += sprintf(buf, "  \"frame\": %d,\n", decoded_frame_count);
  buf += sprintf(buf, "  \"showFrame\": %d,\n", analyzer_data.show_frame);
  buf += sprintf(buf, "  \"frameType\": %d,\n", analyzer_data.frame_type);
  buf += sprintf(buf, "  \"baseQIndex\": %d\n", analyzer_data.base_qindex);
  decoded_frame_count++;
  buf += sprintf(buf, "},\n");
  printf("%s", buffer);
  aom_free(buffer);
}

EMSCRIPTEN_KEEPALIVE
int open_file(char *file) {
  if (file == NULL) {
    file = "/tmp/input.ivf";
  }
  reader = aom_video_reader_open(file);
  if (!reader) die("Failed to open %s for reading.", file);
  info = aom_video_reader_get_info(reader);
  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  fprintf(stderr, "Using %s\n",
          aom_codec_iface_name(decoder->codec_interface()));
  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");
  init_analyzer();
  aom_codec_control(&codec, ANALYZER_SET_DATA, &analyzer_data);
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
int read_frame() {
  if (!aom_video_reader_read_frame(reader)) return EXIT_FAILURE;
  img = NULL;
  aom_codec_iter_t iter = NULL;
  size_t frame_size = 0;
  const unsigned char *frame = aom_video_reader_get_frame(reader, &frame_size);
  if (aom_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0) !=
      AOM_CODEC_OK) {
    die_codec(&codec, "Failed to decode frame.");
  }
  img = aom_codec_get_frame(&codec, &iter);
  if (img == NULL) {
    return EXIT_FAILURE;
  }
  ++frame_count;
  aom_codec_alg_priv_t *t = (aom_codec_alg_priv_t *)codec.priv;
  AVxWorker *const worker = &t->frame_workers[0];
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  cm = &frame_worker_data->pbi->common;
  pbi = frame_worker_data->pbi;
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
unsigned char *get_plane(int plane) { return img->planes[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_stride(int plane) { return img->stride[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_width(int plane) { return aom_img_plane_width(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_plane_height(int plane) { return aom_img_plane_height(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_frame_width() { return info->frame_width; }

EMSCRIPTEN_KEEPALIVE
int get_frame_height() { return info->frame_height; }

static void parse_args(char **argv) {
  char **argi, **argj;
  struct arg arg;
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;
    if (arg_match(&arg, &dump_block_size_arg, argi)) layers |= BLOCK_SIZE_LAYER;
#if CONFIG_ACCOUNTING
    else if (arg_match(&arg, &dump_accounting_arg, argi))
      layers |= ACCOUNTING_LAYER;
#endif
    else if (arg_match(&arg, &dump_transform_size_arg, argi))
      layers |= TRANSFORM_SIZE_LAYER;
    else if (arg_match(&arg, &dump_transform_type_arg, argi))
      layers |= TRANSFORM_TYPE_LAYER;
    else if (arg_match(&arg, &dump_mode_arg, argi))
      layers |= MODE_LAYER;
    else if (arg_match(&arg, &dump_skip_arg, argi))
      layers |= SKIP_LAYER;
    else if (arg_match(&arg, &dump_filter_arg, argi))
      layers |= FILTER_LAYER;
    else if (arg_match(&arg, &dump_dering_gain_arg, argi))
      layers |= DERING_GAIN_LAYER;
    else if (arg_match(&arg, &dump_reference_frame_arg, argi))
      layers |= REFERENCE_FRAME_LAYER;
    else if (arg_match(&arg, &dump_all_arg, argi))
      layers |= ALL_LAYERS;
    else if (arg_match(&arg, &usage_arg, argi))
      usage_exit();
    else if (arg_match(&arg, &pretty_arg, argi))
      pretty = 1;
    else if (arg_match(&arg, &limit_arg, argi))
      stop_after = arg_parse_uint(&arg);
    else
      argj++;
  }
}

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s src_filename <options>\n", exec_name);
  fprintf(stderr, "\nOptions:\n");
  arg_show_usage(stderr, main_args);
  exit(EXIT_FAILURE);
}

EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  exec_name = argv[0];
  parse_args(argv);
  if (argc >= 2) {
    open_file(argv[1]);
    printf("[\n");
    while (1) {
      if (stop_after && (decoded_frame_count >= stop_after)) break;
      if (read_frame()) break;
    }
    printf("null\n");
    printf("]");
  } else {
    usage_exit();
  }
}

EMSCRIPTEN_KEEPALIVE
void quit() {
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
}

EMSCRIPTEN_KEEPALIVE
void set_layers(LayerType v) { layers = v; }
