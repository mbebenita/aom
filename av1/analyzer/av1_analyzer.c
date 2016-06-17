#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "av1/decoder/decoder.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "../decoder/decoder.h"

#include "av1_analyzer.h"


aom_codec_err_t av1_analyze_frame(struct AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  const int mi_rows = cm->mi_rows;
  const int mi_cols = cm->mi_cols;
  int r, c;
  if (pbi->analyzer_data == NULL) {
    return AOM_CODEC_OK;
  }
  AV1AnalyzerMVBuffer mv_grid = pbi->analyzer_data->mv_grid;
  if (mv_grid.size > 0) {
    if (mi_rows * mi_cols > mv_grid.size) {
      return AOM_CODEC_ERROR;
    }
    for (r = 0; r < mi_rows; ++r) {
      for (c = 0; c < mi_cols; ++c) {
        const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
        AV1AnalyzerMV *mv = &mv_grid.buffer[r * mi_cols + c];
        mv->col = mbmi->mv[0].as_mv.col;
        mv->row = mbmi->mv[0].as_mv.row;
      }
    }
  }

  pbi->analyzer_data->mi_rows = mi_rows;
  pbi->analyzer_data->mi_cols = mi_cols;

  return AOM_CODEC_OK;

//  return cm->mi_cols;
//
//  const MB_MODE_INFO *mbmi =
//    &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
//  return mbmi->mv[0].as_mv.row << 16 | mbmi->mv[0].as_mv.col;

}