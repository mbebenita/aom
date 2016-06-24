#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "av1/decoder/decoder.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "../decoder/decoder.h"

#include "av1_analyzer.h"

// Saves the decoder state.
aom_codec_err_t av1_analyze_frame(struct AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  const int mi_rows = cm->mi_rows;
  const int mi_cols = cm->mi_cols;
  int r, c;
  if (pbi->analyzer_data == NULL) {
    return AOM_CODEC_OK;
  }

  pbi->analyzer_data->mi_rows = mi_rows;
  pbi->analyzer_data->mi_cols = mi_cols;

  // Save mode info.
  AV1AnalyzerMIBuffer mi_grid = pbi->analyzer_data->mi_grid;
  if (mi_grid.size > 0) {
    if (mi_rows * mi_cols > mi_grid.size) {
      return AOM_CODEC_ERROR;
    }
    for (r = 0; r < mi_rows; ++r) {
      for (c = 0; c < mi_cols; ++c) {
        const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
        AV1AnalyzerMI *mi = &mi_grid.buffer[r * mi_cols + c];
        // MVs.
        mi->mv.col = mbmi->mv[0].as_mv.col;
        mi->mv.row = mbmi->mv[0].as_mv.row;
        // Prediction Mode
        mi->mode = mbmi->mode;
        // Deringing Gain
        mi->dering_gain = mbmi->dering_gain;
      }
    }
  }
  // Sa
  return AOM_CODEC_OK;
}