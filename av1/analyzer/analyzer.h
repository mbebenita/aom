#ifndef ANALYZER_H_
#define ANALYZER_H_

#if CONFIG_ACCOUNTING
#include "av1/common/accounting.h"
#endif

struct AV1Decoder;

typedef enum {
  ANALYZER_OK,
  ANALYZER_ERROR
} AnalyzerError;

/**
 * Motion Vector (MV)
 */
typedef struct AnalyzerMV {
  int16_t row;
  int16_t col;
} AnalyzerMV;

/**
 * Mode Info (MI)
 */
typedef struct AnalyzerMI {
  AnalyzerMV mv[2];
  int8_t mv_reference_frame[2];
  int8_t filter;
  int8_t mode;
  int8_t dering_gain;
  int8_t skip;
  int8_t block_size;
  int8_t segment_id;
  int8_t transform_type;
  int8_t transform_size;
} AnalyzerMI;

typedef struct AnalyzerMIBuffer {
  AnalyzerMI *buffer;
  uint32_t length;
} AnalyzerMIBuffer;

#define ANALYZER_MAX_SEGMENTS 8

/**
 * Holds everything that is needed by the stream analyzer.
 */
typedef struct AnalyzerData {
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
  AnalyzerMIBuffer mi_grid;
  int show_frame;
  int frame_type;
  int base_qindex;
  int mi_rows;
  int mi_cols;
  int tile_rows_log2;
  int tile_cols_log2;
  int clpf_strength_y;
  int dering_level;
  int y_dequant[ANALYZER_MAX_SEGMENTS][2];
  int uv_dequant[ANALYZER_MAX_SEGMENTS][2];
  void (*ready)();
} AnalyzerData;

AnalyzerError analyzer_record_frame(struct AV1Decoder *pbi);
#endif  // ANALYZER_H_