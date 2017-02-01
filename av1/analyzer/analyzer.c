#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "av1/decoder/decoder.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "../decoder/decoder.h"

#include "analyzer.h"
#include "../common/blockd.h"
#include "../common/onyxc_int.h"

// Saves the decoder state.
AnalyzerError analyzer_record_frame(struct AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  const uint32_t mi_rows = cm->mi_rows;
  const uint32_t mi_cols = cm->mi_cols;
  uint32_t r, c;
  AnalyzerData *analyzer_data = pbi->analyzer_data;
  if (analyzer_data == NULL) {
    return ANALYZER_OK;
  }
  analyzer_data->show_frame = cm->show_frame;
  analyzer_data->frame_type = cm->frame_type;
  analyzer_data->base_qindex = cm->base_qindex;
  analyzer_data->mi_rows = mi_rows;
  analyzer_data->mi_cols = mi_cols;
  analyzer_data->tile_rows_log2 = cm->log2_tile_rows;
  analyzer_data->tile_cols_log2 = cm->log2_tile_cols;
#if CONFIG_ACCOUNTING
  analyzer_data->accounting = &pbi->accounting;
#endif
#if CONFIG_CLPF
  analyzer_data->clpf_strength_y = cm->clpf_strength_y;
#endif
#if CONFIG_DERING
  analyzer_data->dering_level = cm->dering_level;
#endif
  int i;
  for (i = 0; i < ANALYZER_MAX_SEGMENTS; i++) {
    analyzer_data->y_dequant[i][0] = cm->y_dequant[i][0];
    analyzer_data->y_dequant[i][1] = cm->y_dequant[i][1];
    analyzer_data->uv_dequant[i][0] = cm->uv_dequant[i][0];
    analyzer_data->uv_dequant[i][1] = cm->uv_dequant[i][1];
  }
  // Save mode info.
  AnalyzerMIBuffer mi_grid = analyzer_data->mi_grid;
  if (mi_grid.length > 0) {
    if (mi_rows * mi_cols > mi_grid.length) {
      return ANALYZER_ERROR;
    }
    for (r = 0; r < mi_rows; ++r) {
      for (c = 0; c < mi_cols; ++c) {
        const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
        AnalyzerMI *mi = &mi_grid.buffer[r * mi_cols + c];
        // Segment
        mi->segment_id = mbmi->segment_id;
        // MVs
        mi->mv[0].col = mbmi->mv[0].as_mv.col;
        mi->mv[0].row = mbmi->mv[0].as_mv.row;
        mi->mv[1].col = mbmi->mv[1].as_mv.col;
        mi->mv[1].row = mbmi->mv[1].as_mv.row;
        // Reference Frames
        mi->mv_reference_frame[0] = mbmi->ref_frame[0];
        mi->mv_reference_frame[1] = mbmi->ref_frame[1];
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
  if (analyzer_data->ready) {
    analyzer_data->ready();
  }
  return ANALYZER_OK;
}
