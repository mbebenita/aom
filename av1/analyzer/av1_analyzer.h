#ifndef AV1_ANALYZER_AV1_ANALYZER_H_
#define AV1_ANALYZER_AV1_ANALYZER_H_


struct AV1Decoder;

//
//DC_PRED 0    // Average of above and left pixels
//V_PRED 1     // Vertical
//H_PRED 2     // Horizontal
//D45_PRED 3   // Directional 45  deg = round(arctan(1/1) * 180/pi)
//D135_PRED 4  // Directional 135 deg = 180 - 45
//D117_PRED 5  // Directional 117 deg = 180 - 63
//D153_PRED 6  // Directional 153 deg = 180 - 27
//D207_PRED 7  // Directional 207 deg = 180 + 27
//D63_PRED 8   // Directional 63  deg = round(arctan(2/1) * 180/pi)
//TM_PRED 9    // True-motion
//NEARESTMV 10
//NEARMV 11
//ZEROMV 12
//NEWMV 13
//MB_MODE_COUNT 14
//typedef uint8_t PREDICTION_MODE;

//typedef enum {
//  DC_PRED       = 0,
//  V_PRED        = 1,
//  H_PRED        = 2,
//  D45_PRED      = 3,
//  D135_PRED     = 4,
//  D117_PRED     = 5,
//  D153_PRED     = 6,
//  D207_PRED     = 7,
//  D63_PRED      = 8,
//  TM_PRED       = 9,
//  NEARESTMV     = 10,
//  NEARMV        = 11,
//  ZEROMV        = 12,
//  NEWMV         = 13
//} AV1AnalyzerPredictionMode;

typedef uint8_t AV1AnalyzerPredictionMode;
typedef int8_t AV1ReferenceFrame;
typedef int8_t AV1InterpFilter;

typedef uint8_t AV1TransformType;
typedef uint8_t AV1TransformSize;

typedef struct AV1Image {
  unsigned char *planes[4];
  int stride[4];
} AV1Image;

/**
 * Motion Vector (MV)
 */
typedef struct AV1AnalyzerMV {
  int16_t row;
  int16_t col;
} AV1AnalyzerMV;

/**
 * Mode Info (MI)
 */
typedef struct AV1AnalyzerMI {
  AV1AnalyzerMV mv[2];
  AV1ReferenceFrame reference_frame[2];
  AV1InterpFilter filter;
  AV1AnalyzerPredictionMode mode;
  int8_t dering_gain;
  int8_t skip;
  uint8_t block_size;

  AV1TransformType transform_type;
  AV1TransformSize transform_size;
} AV1AnalyzerMI;

typedef struct AV1AnalyzerMIBuffer {
  AV1AnalyzerMI *buffer;
  // Size in AV1AnalyzerMVs.
  int size;
} AV1AnalyzerMIBuffer;

// Holds everything that is needed by the stream analyzer.
typedef struct AV1AnalyzerData {
  AV1Image image;
  AV1AnalyzerMIBuffer mi_grid;
  int mi_rows;
  int mi_cols;
  int tile_rows_log2;
  int tile_cols_log2;

} AV1AnalyzerData;


aom_codec_err_t av1_analyze_frame(struct AV1Decoder *pbi);

#endif  // AV1_ANALYZER_AV1_ANALYZER_H_