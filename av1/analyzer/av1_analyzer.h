#ifndef AV1_ANALYZER_AV1_ANALYZER_H_
#define AV1_ANALYZER_AV1_ANALYZER_H_


struct AV1Decoder;

typedef struct AV1AnalyzerMV {
  int16_t row;
  int16_t col;
} AV1AnalyzerMV;

typedef struct AV1AnalyzerMVBuffer {
  AV1AnalyzerMV *buffer;
  // Size in AV1AnalyzerMVs.
  int size;
} AV1AnalyzerMVBuffer;

// Holds everything that is needed by the stream analyzer.
typedef struct AV1AnalyzerData {
  AV1AnalyzerMVBuffer mv_grid;
  int mi_rows;
  int mi_cols;
} AV1AnalyzerData;


aom_codec_err_t av1_analyze_frame(struct AV1Decoder *pbi);

#endif  // AV1_ANALYZER_AV1_ANALYZER_H_