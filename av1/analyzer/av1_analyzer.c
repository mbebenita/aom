#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "av1/decoder/decoder.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "../decoder/decoder.h"

#include "av1_analyzer.h"
#include "../common/blockd.h"

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
        // MVs
        mi->mv[0].col = mbmi->mv[0].as_mv.col;
        mi->mv[0].row = mbmi->mv[0].as_mv.row;
        mi->mv[1].col = mbmi->mv[1].as_mv.col;
        mi->mv[1].row = mbmi->mv[1].as_mv.row;
        // Reference Frames
        mi->reference_frame[0] = mbmi->ref_frame[0];
        mi->reference_frame[1] = mbmi->ref_frame[1];
        // Prediction Mode
        mi->mode = mbmi->mode;
        // Deringing Gain
        mi->dering_gain = mbmi->dering_gain;
        // Block size
        mi->block_size = mbmi->sb_type;
        // Skip flag
        mi->skip = mbmi->skip;
        // Filters
        mi->filter = mbmi->interp_filter;
        // Transform
        mi->transform_type = mbmi->tx_type;
        mi->transform_size = mbmi->tx_size;
      }
    }
  }
  // Sa
  return AOM_CODEC_OK;
}