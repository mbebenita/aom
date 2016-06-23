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

// Simple Decoder
// ==============
//
// This is an example of a simple decoder loop. It takes an input file
// containing the compressed data (in IVF format), passes it through the
// decoder, and writes the decompressed frames to disk. Other decoder
// examples build upon this one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For decoders, you only have to include `aom_decoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// aom.
//
// Initializing The Codec
// ----------------------
// The libaom decoder is initialized by the call to aom_codec_dec_init().
// Determining the codec interface to use is handled by AvxVideoReader and the
// functions prefixed with aom_video_reader_. Discussion of those functions is
// beyond the scope of this example, but the main gist is to open the input file
// and parse just enough of it to determine if it's a AVx file and which AVx
// codec is contained within the file.
// Note the NULL pointer passed to aom_codec_dec_init(). We do that in this
// example because we want the algorithm to determine the stream configuration
// (width/height) and allocate memory automatically.
//
// Decoding A Frame
// ----------------
// Once the frame has been read into memory, it is decoded using the
// `aom_codec_decode` function. The call takes a pointer to the data
// (`frame`) and the length of the data (`frame_size`). No application data
// is associated with the frame in this example, so the `user_priv`
// parameter is NULL. The `deadline` parameter is left at zero for this
// example. This parameter is generally only used when doing adaptive post
// processing.
//
// Codecs may produce a variable number of output frames for every call to
// `aom_codec_decode`. These frames are retrieved by the
// `aom_codec_get_frame` iterator function. The iterator variable `iter` is
// initialized to NULL each time `aom_codec_decode` is called.
// `aom_codec_get_frame` is called in a loop, returning a pointer to a
// decoded image or NULL to indicate the end of list.
//
// Processing The Decoded Data
// ---------------------------
// In this example, we simply write the encoded data to disk. It is
// important to honor the image's `stride` values.
//
// Cleanup
// -------
// The `aom_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exceptions, aom_codec functions return an enumerated error status,
// with the value `0` indicating success.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "../av1/analyzer/av1_analyzer.h"
#include "../video_common.h"


static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <outfile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int frame_count = 0;
FILE *outfile = NULL;
aom_codec_ctx_t codec;
AvxVideoReader *reader = NULL;
const AvxInterface *decoder = NULL;
const AvxVideoInfo *info = NULL;
aom_image_t *img = NULL;
AV1_COMMON *cm = NULL;

int read_frame();

AV1AnalyzerData analyzer_data;

void init_analyzer() {
  const int mi_count = info->frame_width * info->frame_height / 64;
  analyzer_data.mv_grid.buffer = aom_malloc(sizeof(AV1AnalyzerMV) * mi_count);
  analyzer_data.mv_grid.size = mi_count;
}

void dump_analyzer() {
  aom_codec_control(&codec, AV1_ANALYZER_SET_DATA, &analyzer_data);
  const int mi_rows = analyzer_data.mi_rows;
  const int mi_cols = analyzer_data.mi_cols;
  int r, c;
  for (r = 0; r < mi_rows; ++r) {
    for (c = 0; c < mi_cols; ++c) {
       AV1AnalyzerMV mv = analyzer_data.mv_grid.buffer[r * mi_cols + c];
      // printf("%3d:%-3d ", abs(mv.row), abs(mv.col));
       printf("%d:%d ", mv.row, mv.col);
      // ...
    }
    printf("\n");
  }
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
  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  init_analyzer();
  aom_codec_control(&codec, AV1_ANALYZER_SET_DATA, &analyzer_data);

  printf("Opened file %s okay.\n", file);
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
int read_frame() {
  if (!aom_video_reader_read_frame(reader)) {
    return EXIT_FAILURE;
  }
  img = NULL;
  aom_codec_iter_t iter = NULL;
  
  size_t frame_size = 0;
  const unsigned char *frame =
      aom_video_reader_get_frame(reader, &frame_size);
  if (aom_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0) != AOM_CODEC_OK) {
    die_codec(&codec, "Failed to decode frame.");
  }
  img = aom_codec_get_frame(&codec, &iter);
  if (img == NULL) {
    return EXIT_FAILURE;
  }
  ++frame_count;
  aom_codec_alg_priv_t* t = (aom_codec_alg_priv_t*)codec.priv;
  AVxWorker *const worker = &t->frame_workers[0];
  FrameWorkerData *const frame_worker_data = (FrameWorkerData *)worker->data1;
  cm = &frame_worker_data->pbi->common;

  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
int get_frame_count() {
  return frame_count;
}

EMSCRIPTEN_KEEPALIVE
int get_mi_rows() {
  return cm->mi_rows;
}

EMSCRIPTEN_KEEPALIVE
int get_mi_cols() {
  return cm->mi_cols;
}

EMSCRIPTEN_KEEPALIVE
int get_mi_mv(int c, int r) {
  AV1AnalyzerMV *mv =
    &analyzer_data.mv_grid.buffer[r * analyzer_data.mi_cols + c];
  return mv->row << 16 | mv->col;
}

EMSCRIPTEN_KEEPALIVE
int get_dering_gain(int c, int r) {
#if DERING_REFINEMENT
  const MB_MODE_INFO *mbmi =
    &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
  return mbmi->dering_gain;
#else
  return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE
unsigned char *get_plane(int plane) {
  return img->planes[plane];
}

EMSCRIPTEN_KEEPALIVE
int get_plane_stride(int plane) {
  return img->stride[plane];
}

EMSCRIPTEN_KEEPALIVE
int get_plane_width(int plane) {
  return aom_img_plane_width(img, plane);
}

EMSCRIPTEN_KEEPALIVE
int get_plane_height(int plane) {
  return aom_img_plane_height(img, plane);
}

EMSCRIPTEN_KEEPALIVE
int get_frame_width() {
  return info->frame_width;
}

EMSCRIPTEN_KEEPALIVE
int get_frame_height() {
  return info->frame_height;
}


EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  // ...
  if (argc == 2) {
    open_file(argv[1]);
    while (!read_frame()) {
      printf("%d\n", frame_count);
    }
  }
}

EMSCRIPTEN_KEEPALIVE
void quit() {
  printf("Processed %d frames.\n", frame_count);
  if (aom_codec_destroy(&codec)) 
    die_codec(&codec, "Failed to destroy codec");

  // printf("Play: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d %s\n",
  //        info->frame_width, info->frame_height, argv[2]);

  aom_video_reader_close(reader);

  fclose(outfile);
}