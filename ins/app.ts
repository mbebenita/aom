declare let angular: any;
declare let FS: any;
declare let Mousetrap: any;
declare let tinycolor: any;
declare let tinygradient: any;

class Y4MFile {
  constructor(public size: Size, public buffer: Uint8Array, public frames: Y4MFrame []) {
    // ...
  }
}
class Y4MFrame {
  constructor(public y: number, public cb: number, public cr: number) {
    // ...
  }
}
interface Window {
  Module: any;
}

interface CanvasRenderingContext2D {
  mozImageSmoothingEnabled: boolean;
  imageSmoothingEnabled;
}

interface Math {
  imul: (a: number, b: number) => number;
}

let colors = [
  "#E85EBE", "#009BFF", "#00FF00", "#0000FF", "#FF0000", "#01FFFE", "#FFA6FE",
  "#FFDB66", "#006401", "#010067", "#95003A", "#007DB5", "#FF00F6", "#FFEEE8",
  "#774D00", "#90FB92", "#0076FF", "#D5FF00", "#FF937E", "#6A826C", "#FF029D",
  "#FE8900", "#7A4782", "#7E2DD2", "#85A900", "#FF0056", "#A42400", "#00AE7E",
  "#683D3B", "#BDC6FF", "#263400", "#BDD393", "#00B917", "#9E008E", "#001544",
  "#C28C9F", "#FF74A3", "#01D0FF", "#004754", "#E56FFE", "#788231", "#0E4CA1",
  "#91D0CB", "#BE9970", "#968AE8", "#BB8800", "#43002C", "#DEFF74", "#00FFC6",
  "#FFE502", "#620E00", "#008F9C", "#98FF52", "#7544B1", "#B500FF", "#00FF78",
  "#FF6E41", "#005F39", "#6B6882", "#5FAD4E", "#A75740", "#A5FFD2", "#FFB167"
];

function hexToRGB(hex: string, alpha: number = 0) {
  let r = parseInt(hex.slice(1,3), 16),
      g = parseInt(hex.slice(3,5), 16),
      b = parseInt(hex.slice(5,7), 16),
      a = "";
  if (alpha) {
    a = ", 1";
  }
  return "rgb(" + r + ", " + g + ", " + b + a + ")";
}

function getLineOffset(lineWidth: number) {
  return lineWidth % 2 == 0 ? 0 : 0.5;
}

const mi_block_size_log2 = 3;

function alignPowerOfTwo(value: number, n: number) {
  return ((value) + ((1 << n) - 1)) & ~((1 << n) - 1);
}

function tileOffset(i: number, rowsOrCols: number, tileRowsOrColsLog2: number) {
  let sbRowsOrCols = alignPowerOfTwo(rowsOrCols, mi_block_size_log2) >> mi_block_size_log2;
  let offset = ((i * sbRowsOrCols) >> tileRowsOrColsLog2) << mi_block_size_log2;
  return Math.min(offset, rowsOrCols);
}

enum AOMAnalyzerPredictionMode {
  DC_PRED 		= 0,   // Average of above and left pixels
  V_PRED 			= 1,   // Vertical
  H_PRED 			= 2,   // Horizontal
  D45_PRED 		= 3,   // Directional 45  deg = round(arctan(1/1) * 180/pi)
  D135_PRED 	= 4,   // Directional 135 deg = 180 - 45
  D117_PRED 	= 5,   // Directional 117 deg = 180 - 63
  D153_PRED 	= 6,   // Directional 153 deg = 180 - 27
  D207_PRED 	= 7,   // Directional 207 deg = 180 + 27
  D63_PRED 		= 8,   // Directional 63  deg = round(arctan(2/1) * 180/pi)
  TM_PRED 		= 9,   // True-motion
  NEARESTMV 	= 10,
  NEARMV 			= 11,
  ZEROMV 			= 12,
  NEWMV 			= 13
}

enum AOMAnalyzerBlockSize {
  BLOCK_4X4   = 0,
  BLOCK_4X8   = 1,
  BLOCK_8X4   = 2,
  BLOCK_8X8   = 3,
  BLOCK_8X16  = 4,
  BLOCK_16X8  = 5,
  BLOCK_16X16 = 6,
  BLOCK_16X32 = 7,
  BLOCK_32X16 = 8,
  BLOCK_32X32 = 9,
  BLOCK_32X64 = 10,
  BLOCK_64X32 = 11,
  BLOCK_64X64 = 12
}

enum MIProperty {
  GET_MI_MV,
  GET_MI_MODE,
  GET_MI_SKIP,
  GET_MI_REFERENCE_FRAME,
  GET_MI_BLOCK_SIZE,
  GET_MI_TRANSFORM_TYPE,
  GET_MI_TRANSFORM_SIZE,
  GET_MI_DERING_GAIN,
  GET_MI_BITS
}

enum AOMAnalyzerTransformMode {
  ONLY_4X4       = 0,
  ALLOW_8X8      = 1,
  ALLOW_16X16    = 2,
  ALLOW_32X32    = 3,
  TX_MODE_SELECT = 4
}

enum AOMAnalyzerTransformType {
  DCT_DCT        = 0,
  ADST_DCT       = 1,
  DCT_ADST       = 2,
  ADST_ADST      = 3
}

enum AOMAnalyzerTransformSize {
  TX_4X4         = 0,
  TX_8X8         = 1,
  TX_16X16       = 2,
  TX_32X32       = 3
}

let blockSizes = [
  [2, 2],
  [2, 3],
  [3, 2],
  [3, 3],
  [3, 4],
  [4, 3],
  [4, 4],
  [4, 5],
  [5, 4],
  [5, 5],
  [5, 6],
  [6, 5],
  [6, 6]
];

interface AOMInternal {
  _read_frame (): number;
  _get_plane (pli: number): number;
  _get_plane_stride (pli: number): number;
  _get_plane_width (pli: number): number;
  _get_plane_height (pli: number): number;
  _get_mi_cols_and_rows(): number;
  _get_tile_cols_and_rows_log2(): number;
  _get_frame_count(): number;
  _get_frame_width(): number;
  _get_frame_height(): number;
  _open_file(): number;

  _get_mi_property(p: MIProperty, c: number, r: number, i: number): number;

  _get_predicted_plane_buffer(pli: number): number;
  _get_predicted_plane_stride(pli: number): number;

  HEAPU8: Uint8Array;
}

class AOM {
  native: AOMInternal = (<any>window).Module;
  HEAPU8: Uint8Array = this.native.HEAPU8;
  constructor () {

  }
  open_file () {
    return this.native._open_file();
  }
  read_frame () {
    return this.native._read_frame();
  }
  get_plane (pli: number): number {
    return this.native._get_plane(pli);
  }
  get_plane_stride (pli: number): number {
    return this.native._get_plane_stride(pli);
  }
  get_plane_width (pli: number): number {
    return this.native._get_plane_width(pli);
  }
  get_plane_height (pli: number): number {
    return this.native._get_plane_height(pli);
  }
  get_mi_property(p: MIProperty, c: number, r: number, i: number = 0) {
    return this.native._get_mi_property(p, c, r, i);
  }
  get_predicted_plane_buffer(pli: number): number {
    return this.native._get_predicted_plane_buffer(pli);
  }
  get_predicted_plane_stride(pli: number): number {
    return this.native._get_predicted_plane_stride(pli);
  }
  get_frame_count(): number {
    return this.native._get_frame_count();
  }
  getFrameSize(): Size {
    return new Size(this.native._get_frame_width(), this.native._get_frame_height());
  }
  getMIGridSize(): GridSize {
    let v = this.native._get_mi_cols_and_rows();
    let cols = v >> 16;
    let rows = this.native._get_mi_cols_and_rows() & 0xFF;
    return new GridSize(cols, rows);
  }
  getTileGridSizeLog2(): GridSize {
    let v = this.native._get_tile_cols_and_rows_log2();
    let cols = v >> 16;
    let rows = v & 0xFF;
    return new GridSize(cols, rows);
  }
}

class ErrorMetrics {
  constructor(public tss: number, public mse: number) {
    // ...
  }
  toString() {
    return "tss: " + this.tss + ", mse: " + this.mse.toFixed(4);
  }
}

class Size {
  constructor(public w: number, public h: number) {
    // ...
  }
  clone() {
    return new Size(this.w, this.h);
  }
  equals(other: Size) {
    return this.w == other.w || this.h == other.h;
  }
  area(): number {
    return this.w * this.h;
  }
  multiplyScalar(scalar: number) {
		if (isFinite(scalar)) {
			this.w *= scalar;
			this.h *= scalar;
		} else {
			this.w = 0;
			this.h = 0;
		}
		return this;
	}
}

class Rectangle {
  constructor (public x: number, public y: number, public w: number, public h: number) {
    // ...
  }
  static createRectangleCenteredAtPoint(v: Vector, w: number, h: number) {
    return new Rectangle(v.x - w / 2, v.y - h / 2, w, h);
  }
  static createRectangleFromSize(size: Size) {
    return new Rectangle(0, 0, size.w, size.h);
  }
  clone(): Rectangle {
    return new Rectangle(this.x, this.y, this.w, this.h);
  }
  multiplyScalar(scalar: number) {
		if (isFinite(scalar)) {
			this.w *= scalar;
			this.h *= scalar;
		} else {
			this.w = 0;
			this.h = 0;
		}
		return this;
	}
}

class GridSize {
  constructor (public cols: number, public rows: number) {
    // ...
  }
}

class Vector {
  x: number;
  y: number;
  constructor (x: number, y: number) {
    this.x = x;
    this.y = y;
  }
  lerp(v: Vector, alpha: number) {
    this.x += (v.x - this.x) * alpha;
		this.y += (v.y - this.y) * alpha;
    return this;
  }
  clone(): Vector {
    return new Vector(this.x, this.y);
  }
  lengthSq() {
		return this.x * this.x + this.y * this.y;
	}
	length() {
		return Math.sqrt(this.x * this.x + this.y * this.y);
	}
  normalize () {
		return this.divideScalar(this.length());
	}
  multiplyScalar(scalar) {
		if (isFinite(scalar)) {
			this.x *= scalar;
			this.y *= scalar;
		} else {
			this.x = 0;
			this.y = 0;
		}
		return this;
	}
	divide(v) {
		this.x /= v.x;
		this.y /= v.y;
		return this;
	}
	divideScalar(scalar) {
		return this.multiplyScalar(1 / scalar);
	}
  snap() {
    // TODO: Snap to nearest pixel
		this.x = this.x | 0;
    this.y = this.y | 0;
    return this;
	}
  sub(v: Vector): Vector {
		this.x -= v.x;
		this.y -= v.y;
		return this;
	}
  add(v: Vector): Vector {
		this.x += v.x;
		this.y += v.y;
		return this;
	}
  clampLength(min: number, max: number) {
		let length = this.length();
		this.multiplyScalar(Math.max(min, Math.min(max, length)) / length);
		return this;
	}
  toString(): string {
    return this.x + "," + this.y;
  }
}

interface Decoder {
  description: string;
  path: string;
}

const BLOCK_SIZE = 8;

class AppCtrl {
  aom: AOM = null;

  decoders = {
    default: {
      description: "Default",
      path: "bin/decoder.js"
    },
    dering: {
      description: "Deringing",
      path: "bin/dering-decoder.js"
    }
  };
  selectedDecoder: string;
  frameSize: Size = new Size(128, 128);
  tileGridSize: GridSize = new GridSize(0, 0);
  fileSize: number = 0;
  ratio: number = 1;
  scale: number = 1;

  showY: boolean;
  showU: boolean;
  showV: boolean;
  showOriginalImage: boolean;
  showDecodedImage: boolean;
  showPredictedImage: boolean;

  showSuperBlockGrid: boolean;
  showTileGrid: boolean;
  showBlockSplit: boolean;
  showTransformSplit: boolean;
  showMotionVectors: boolean;
  showDering: boolean;
  showMode: boolean;
  showBits: boolean;
  showSkip: boolean;
  showInfo: boolean;

  options = {
    showY: {
      key: "y",
      description: "Show Y",
      detail: "Shows Y image plane.",
      updatesImage: true,
      default: true
    },
    showU: {
      key: "u",
      description: "Show U",
      detail: "Shows U image plane.",
      updatesImage: true,
      default: true
    },
    showV: {
      key: "v",
      description: "Show V",
      detail: "Shows V image plane.",
      updatesImage: true,
      default: true
    },
    showOriginalImage: {
      key: "w",
      description: "Show Original Image",
      detail: "Shows the loaded .y4m file.",
      updatesImage: true,
      default: false,
      disabled: true
    },
    showDecodedImage: {
      key: "i",
      description: "Show Image",
      detail: "Shows image.",
      updatesImage: true,
      default: true
    },
    showPredictedImage: {
      key: "p",
      description: "Show Predicted Image",
      detail: "Shows predicted image.",
      updatesImage: true,
      default: false
    },
    showSuperBlockGrid: {
      key: "g",
      description: "Show SB Grid",
      detail: "Shows 64x64 mode info grid.",
      default: false
    },
    showTileGrid: {
      key: "l",
      description: "Show Tile Grid",
      detail: "Shows tile grid.",
      default: false
    },
    showTransformSplit: {
      key: "t",
      description: "Show Transform Grid",
      detail: "Shows transform blocks.",
      default: false
    },
    showBlockSplit: {
      key: "s",
      description: "Show Split Grid",
      detail: "Shows block partitions.",
      default: false
    },
    showDering: {
      key: "d",
      description: "Show Dering",
      detail: "Shows blocks where the deringing filter is applied.",
      default: false
    },
    showMotionVectors: {
      key: "m",
      description: "Show Motion Vectors",
      detail: "Shows motion vectors, darker colors represent longer vectors.",
      default: false
    },
    showMode: {
      key: "o",
      description: "Show Mode",
      detail: "Shows prediction modes.",
      default: false
    },
    showBits: {
      key: "b",
      description: "Show Bits",
      detail: "Shows bits.",
      default: false
    },
    showSkip: {
      key: "k",
      description: "Show Skip",
      detail: "Shows skip flags.",
      default: false
    },
    showInfo: {
      key: "tab",
      description: "Show Info",
      detail: "Shows mode info details.",
      default: window.innerWidth > 1024
    },
    zoomLock: {
      key: "z",
      description: "Zoom Lock",
      detail: "Locks zoom at the current mouse position.",
      default: false
    }
  };

  blockInfo = {
    blockSize: {
      description: "Block Size"
    },
    blockSkip: {
      description: "Block Skip"
    },
    predictionMode: {
      description: "Prediction Mode"
    },
    deringGain: {
      description: "Dering Gain"
    },
    motionVector: {
      description: "Motion Vectors"
    },
    referenceFrames: {
      description: "Reference Frames"
    },
    transformType: {
      description: "Transform Type"
    },
    transformSize: {
      description: "Transform Size"
    },
    bits: {
      description: "Bits"
    },
    blockError: {
      description: "Block Error"
    },
    frameError: {
      description: "Frame Error"
    }
  };

  frameNumber: number = -1;
  y4mFile: Y4MFile;

  progressValue = 0;
  progressMode = "determinate";

  get isPlaying() {
    return !!this.playInterval;
  }

  // If not zero, we are playing frames.
  playInterval: number = 0;

  container: HTMLDivElement;
  displayCanvas: HTMLCanvasElement;
  overlayCanvas: HTMLCanvasElement;
  zoomCanvas: HTMLCanvasElement;
  chartCanvas: HTMLCanvasElement;

  displayContext: CanvasRenderingContext2D = null;
  overlayContext: CanvasRenderingContext2D = null;
  zoomContext: CanvasRenderingContext2D = null;
  zoomSize = 512;
  zoomLevel = 16;

  chartContext: CanvasRenderingContext2D = null;
  chartSize = 32;
  mousePosition: Vector = new Vector(0, 0);
  imageData: ImageData = null;

  frameCanvas: HTMLCanvasElement;
  frameContext: CanvasRenderingContext2D = null;

  compositionCanvas: HTMLCanvasElement;
  compositionContext: CanvasRenderingContext2D = null;

  lastDecodeFrameTime: number = 0;

  predictionModeNames: string [] = [];
  predictionModeColors: string [] = colors;
  blockSizeNames: string [] = [];

  modeColor = "hsl(79, 20%, 50%)";
  mvColor = "hsl(232, 36%, 41%)";
  skipColor = "hsla(326, 53%, 42%, 0.2)";
  gridColor = "rgba(55,55,55,1)";
  tileGridColor = "hsl(0, 100%, 69%)";
  splitColor = "rgba(33,33,33,1)";
  transformColor = "rgba(255,0,0,1)";

  crosshairLineWidth = 2;
  crosshairColor = "rgba(33,33,33,0.5)";

  gridLineWidth = 3;
  tileGridLineWidth = 5;
  splitLineWidth = 1;
  transformLineWidth = 1;
  modeLineWidth = 2;

  zoomLock = false;

  sharingLink = "";

  $scope: any;
  $interval: any;

  constructor($scope, $interval) {
    let self = this;
    this.$scope = $scope;
    // File input types don't have angular bindings, so we need set the
    // event handler on the scope object.
    $scope.fileInputNameChanged = function() {
      let input = <any>event.target;
      let reader = new FileReader();
      reader.onload = function() {
        let buffer = reader.result;
        self.openFileBytes(new Uint8Array(buffer));
        self.playFrameAsync(1, () => {
          self.drawFrame();
        });
      };
      reader.readAsArrayBuffer(input.files[0]);
    };
    $scope.y4mFileInputNameChanged = function() {
      let input = <any>event.target;
      let reader = new FileReader();
      reader.onload = function() {
        let buffer = reader.result;
        let y4mFile = self.loadY4MBytes(new Uint8Array(buffer));
        if (!y4mFile.size.equals(self.frameSize)) {
          alert("Y4M file frame size doesn't match current frame size.")
          return;
        }
        self.y4mFile = y4mFile;
        self.drawImages();
        // We can now show the image.
        self.options.showOriginalImage.disabled = false;
      };
      reader.readAsArrayBuffer(input.files[0]);
    };

    this.$interval = $interval;
    this.ratio = window.devicePixelRatio || 1;
    this.scale = this.ratio;
    this.scale = 1;

    this.container = <HTMLDivElement>document.getElementById("container");

    this.displayCanvas = <HTMLCanvasElement>document.getElementById("display");
    this.displayContext = this.displayCanvas.getContext("2d");

    this.overlayCanvas = <HTMLCanvasElement>document.getElementById("overlay");
    this.overlayContext = this.overlayCanvas.getContext("2d");

    this.zoomCanvas = <HTMLCanvasElement>document.getElementById("zoom");
    this.zoomContext = this.zoomCanvas.getContext("2d");
    this.zoomContext.mozImageSmoothingEnabled = false;
    this.zoomContext.imageSmoothingEnabled = false;

    this.chartCanvas = <HTMLCanvasElement>document.getElementById("chart");
    this.chartContext = this.chartCanvas.getContext("2d");
    this.chartContext.mozImageSmoothingEnabled = false;
    this.chartContext.imageSmoothingEnabled = false;

    this.overlayCanvas.addEventListener("mousemove", this.onMouseMove.bind(this));
    this.overlayCanvas.addEventListener("mousedown", this.onMouseDown.bind(this));

    this.frameCanvas = document.createElement("canvas");
    this.frameContext = this.frameCanvas.getContext("2d");

    this.compositionCanvas = document.createElement("canvas");
    this.compositionContext = this.compositionCanvas.getContext("2d");

    let parameters = getUrlParameters();
    for (let name in this.options) {
      this[name] = this.options[name].default;
      if (name in parameters) {
        // TODO: Check for boolean here..
        this[name] = parameters[name] == "true";
      }
    }

    this.installKeyboardShortcuts();
    this.initEnums();
    let frames = parseInt(parameters.frameNumber) || 1;

    this.loadDecoder("default", () => {
      this.aom = new AOM();
      let file = "media/default.ivf";
      this.openFile(file, () => {
        this.playFrameAsync(frames, () => {
          this.drawFrame();
        })
      });
    });
  }

  initEnums() {
    for (let k in AOMAnalyzerPredictionMode) {
      if (isNaN(parseInt(k))) {
        this.predictionModeNames.push(k);
      }
    }
    for (let k in AOMAnalyzerBlockSize) {
      if (isNaN(parseInt(k))) {
        this.blockSizeNames.push(k);
      }
    }
  }

  installKeyboardShortcuts() {
    Mousetrap.bind(['.'], () => {
      this.playFrame();
      this.drawFrame();
      this.uiApply();
    });

    Mousetrap.bind(['space'], (e) => {
      this.uiPlayPause();
      e.preventDefault();
    });

    Mousetrap.bind([']'], () => {
      this.uiZoom(2);
      this.uiApply();
    });

    Mousetrap.bind(['['], () => {
      this.uiZoom(1 / 2);
      this.uiApply();
    });

    Mousetrap.bind(['x'], (e) => {
      this.uiResetLayers();
      e.preventDefault();
    });

    let self = this;
    function toggle(name, event) {
      let option = this.options[name];
      self[name] = !self[name];
      if (option.updatesImage) {
        self.drawImages();
      }
      self.drawMain();
      self.showInfo && self.drawInfo();
      self.uiApply();
      event.preventDefault();
    }

    let installedKeys = {};
    for (let name in this.options) {
      let option = this.options[name];
      if (option.key) {
        if (installedKeys[option.key]) {
          console.error("Key: " + option.key + " for " + option.description  + ", is already mapped to " + installedKeys[option.key].description);
        }
        installedKeys[option.key] = option;
        Mousetrap.bind([option.key], toggle.bind(this, name));
      }
    }
  }

  onMouseDown(event: MouseEvent) {
    this.handleMouseEvent(event);
    this.zoomLock = !this.zoomLock;
    this.uiApply();
  }

  onMouseMove(event: MouseEvent) {
    this.handleMouseEvent(event);
  }

  handleMouseEvent(event: MouseEvent) {
    if (this.zoomLock) {
      return;
    }
    function getMousePosition(canvas: HTMLCanvasElement, event: MouseEvent) {
      let rect = canvas.getBoundingClientRect();
      return new Vector(
        event.clientX - rect.left,
        event.clientY - rect.top
      );
    }
    this.mousePosition = getMousePosition(this.overlayCanvas, event);
    this.showInfo && this.drawInfo();
    this.uiApply();
  }

  getMIBlockSize(c: number, r: number, miMinBlockSize: AOMAnalyzerBlockSize = AOMAnalyzerBlockSize.BLOCK_4X4): Size {
    let miBlockSize = this.aom.get_mi_property(MIProperty.GET_MI_BLOCK_SIZE, c, r);
    if (miBlockSize >= blockSizes.length) {
      // TODO: This should not happen, figure out what is going on.
      return new Size(0, 0);
    }
    if (miBlockSize < miMinBlockSize) {
      miBlockSize = miMinBlockSize;
    }
    let w = 1 << blockSizes[miBlockSize][0];
    let h = 1 << blockSizes[miBlockSize][1];
    return new Size(w, h);
  }

  /**
   * Gets the coordinates of the MI block under the mousedown.
   */
  getMIUnderMouse(): Vector {
    let v = this.mousePosition;
    let c = (v.x / this.scale) >> 3;
    let r = (v.y / this.scale) >> 3;
    return this.getMI(c, r);
  }

  /**
   * Get the coordinates of the parent MI block if this block is
   * not an 8x8 block.
   */
  getMI(c: number, r: number): Vector {
    let blockSize = this.getMIBlockSize(c, r);
    c = c & ~((blockSize.w - 1) >> 3);
    r = r & ~((blockSize.h - 1) >> 3);
    return new Vector(c, r);
  }

  getMIBits(c: number, r: number): number {
    let mi = this.getMI(c, r);
    return this.aom.get_mi_property(MIProperty.GET_MI_BITS, mi.x, mi.y);
  }

  loadDecoder(decoder: string, next: () => any) {
    this.selectedDecoder = decoder;
    let s = document.createElement('script');
    s.onload = next;
    s.setAttribute('src', this.decoders[decoder].path);
    document.body.appendChild(s);
  }

  uiChangeDecoder() {
    alert("NYI");
    this.loadDecoder(this.selectedDecoder, () => {
      // ...
    });
  }

  loadY4MBytes(buffer: Uint8Array): Y4MFile {
    return this.parseY4MBytes(buffer);
  }

  parseY4MBytes(buffer: Uint8Array): Y4MFile {
    let header;
    let eol = "\n".charCodeAt(0);
    let offset = 0;
    while (offset < buffer.length) {
      if (buffer[offset++] == eol) {
        header = String.fromCharCode.apply(null, (buffer.subarray(0, offset - 1)));
        break;
      }
    }
    let parameters = header.split(" ");
    let size = new Size(0, 0);
    let colorSpace = "420";
    for (let i = 0; i < parameters.length; i++) {
      let parameter = parameters[i];
      if (parameter[0] == "W") {
        size.w = parseInt(parameter.substring(1));
      } else if (parameter[0] == "H") {
        size.h = parseInt(parameter.substring(1));
      } else if (parameter[0] == "C") {
        colorSpace = parameter.substring(1);
      }
    }
    if (colorSpace != "420" && colorSpace != "420jpeg") {
      console.error("Unsupported color space: " + colorSpace);
      return;
    }
    let y = size.w * size.h;
    let cb = ((size.w + 1) >> 1) * ((size.h + 1) >> 1);
    let cr = cb;
    let frameLength = y + cb + cr;
    let frames: Y4MFrame [] = [];
    while (offset < buffer.length) {
      let start = offset;
      while (offset < buffer.length) {
        if (buffer[offset++] == eol) {
          break;
        }
      }
      let frameHeader = String.fromCharCode.apply(null, (buffer.subarray(start, offset - 1)));
      if (frameHeader != "FRAME") {
        console.error("Cannot parse frame: ");
        return;
      }
      frames.push(new Y4MFrame(offset, offset + y, offset + y + cb));
      offset += frameLength;
    }
    return new Y4MFile(size, buffer, frames);
  }

  openFileBytes(buffer: Uint8Array) {
    this.fileSize = buffer.length;
    FS.writeFile("/tmp/input.ivf", buffer, { encoding: "binary" });
    this.aom.open_file();
    this.frameNumber = -1;
    this.frameSize = this.aom.getFrameSize();
    this.resetCanvases();
  }

  openFile(path: string, next: () => any = null) {
    let fileName = path.replace(/^.*[\\\/]/, '')
    document.title = fileName;
    this.downloadFile(path, (buffer: Uint8Array) => {
      this.openFileBytes(buffer);
      next();
    });
  }

  resetCanvases() {
    this.frameCanvas.width = this.compositionCanvas.width = this.frameSize.w;
		this.frameCanvas.height = this.compositionCanvas.height = this.frameSize.h;

    this.imageData = this.frameContext.createImageData(this.frameSize.w, this.frameSize.h);

    this.container.style.width = (this.frameSize.w * this.scale) + "px";
		this.container.style.height = (this.frameSize.h * this.scale) + "px";

    this.displayCanvas.style.width = (this.frameSize.w * this.scale) + "px";
		this.displayCanvas.style.height = (this.frameSize.h * this.scale) + "px";
    this.displayCanvas.width = this.frameSize.w * this.scale * this.ratio;
		this.displayCanvas.height = this.frameSize.h * this.scale * this.ratio;

    this.overlayCanvas.style.width = (this.frameSize.w * this.scale) + "px";
		this.overlayCanvas.style.height = (this.frameSize.h * this.scale) + "px";
    this.overlayCanvas.width = this.frameSize.w * this.scale * this.ratio;
		this.overlayCanvas.height = this.frameSize.h * this.scale * this.ratio;

    this.zoomCanvas.style.width = this.zoomSize + "px";
		this.zoomCanvas.style.height = this.zoomSize + "px";
    this.zoomCanvas.width = this.zoomSize * this.ratio;
		this.zoomCanvas.height = this.zoomSize * this.ratio;

    this.chartCanvas.style.width = this.zoomSize + "px";
		this.chartCanvas.style.height = this.chartSize + "px";
    this.chartCanvas.width = this.zoomSize * this.ratio;
		this.chartCanvas.height = this.chartSize * this.ratio;
  }

  downloadFile(path: string, next: (buffer: Uint8Array) => void) {
    let xhr = new XMLHttpRequest();
    let self = this;
    self.progressMode = "determinate";
    xhr.open("GET", path, true);
    xhr.responseType = "arraybuffer";
    xhr.send();
    xhr.addEventListener("progress", (e) => {
      let progress = (e.loaded / e.total) * 100;
      this.progressValue = progress;
      this.$scope.$apply();
    });
    xhr.addEventListener("load", function () {
      if (xhr.status != 200) {
        return;
      }
      next(new Uint8Array(this.response));
    });
  }

  showFileInputDialog() {
    angular.element(document.querySelector('#fileInput'))[0].click();
  }

  showY4MFileInputDialog() {
    angular.element(document.querySelector('#y4mFileInput'))[0].click();
  }

  uiAction(name) {
    let file;
    switch (name) {
      case "open-file":
        this.showFileInputDialog();
        return;
      case "open-y4m-file":
        this.showY4MFileInputDialog();
        return;
      case "open-crosswalk":
        file = "media/crosswalk.ivf";
        break;
      case "open-soccer":
        file = "media/soccer_cif_dering.ivf";
        break;
      case "open-tiger":
        file = "media/tiger.ivf";
        break;
    }
    this.openFile(file, () => {
      this.playFrameAsync(1, () => {
        this.drawFrame();
      })
    });
  }

  updateSharingLink() {
    let url = location.protocol + '//' + location.host + location.pathname;
    let args = {
      frameNumber: this.frameNumber
    };
    for (let name in this.options) {
      // Ignore default values.
      if (this[name] == this.options[name].default) {
        continue;
      }
      args[name] = this[name];
    }
    let argList = [];
    for (let arg in args) {
      argList.push(arg + "=" + encodeURIComponent(args[arg]));
    }
    let argListString = argList.join("&");
    this.sharingLink = url + "?" + argListString;
  }

  uiResetLayers() {
    for (let name in this.options) {
      this[name] = this.options[name].default;
    }
    this.drawFrame();
  }

  uiChange() {
    this.updateSharingLink();
    this.resetCanvases();
    this.drawFrame();
  }

  playPause() {
    if (this.playInterval) {
      this.$interval.cancel(this.playInterval);
      this.playInterval = 0;
      return;
    }
    this.playInterval = this.$interval(() => {
      this.playFrame();
      this.drawFrame();
    }, 1);
  }

  playFrameAsync(count: number, step: () => void = null, stop: () => void = null) {
    this.$interval(() => {
      this.playFrame();
      step && step();
      if (--count == 0) {
        stop && stop();
      }
    }, 1, count);
  }

  playFrame(count: number = 1) {
    for (let i = 0; i < count; i++) {
      let s = performance.now();
      if (this.aom.read_frame()) {
        return false;
      }
      this.processFrame();
      this.lastDecodeFrameTime = performance.now() - s;
      this.frameNumber ++;

      let tileGridSize = this.aom.getTileGridSizeLog2();
      this.tileGridSize.cols = 1 << tileGridSize.cols;
      this.tileGridSize.rows = 1 << tileGridSize.rows;
    }
  }

  frameStatistics = {
    bits: {
      values: [],
      show: true,
      description: "Show frame bits over time.",
      detail: "Shows the number of bits per frame over time."
    },
    errors: {
      values: [],
      show: true,
      description: "Show frame mse over time.",
      detail: "Shows the mse per frame over time."
    }
  };

  processFrame() {
    let {cols, rows} = this.aom.getMIGridSize();
    let miTotalBits = 0;
    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        miTotalBits += this.aom.get_mi_property(MIProperty.GET_MI_BITS, c, r);
      }
    }
    this.frameStatistics.bits.values.push(miTotalBits);
    this.frameStatistics.errors.values.push(this.getFrameError());
  }

  uiZoom(value: number) {
    this.scale *= value;
    this.resetCanvases();
    this.drawFrame();
  }

  uiApply() {
    this.updateSharingLink();
    this.$scope.$apply();
  }

  uiPreviousFrame() {
    this.uiApply();
  }

  uiPlayPause() {
    this.playPause();
  }

  uiNextFrame() {
    this.playFrame();
    this.drawFrame();
  }

  drawInfo() {
    let mousePosition = this.mousePosition.clone().divideScalar(this.scale).snap();
    let src = Rectangle.createRectangleCenteredAtPoint(mousePosition, 64, 64);
    let dst = new Rectangle(0, 0, this.zoomSize * this.ratio, this.zoomSize * this.ratio);

    this.zoomContext.clearRect(0, 0, dst.w, dst.h);
    if (this.showOriginalImage || this.showDecodedImage || this.showPredictedImage) {
      this.zoomContext.mozImageSmoothingEnabled = false;
      this.zoomContext.imageSmoothingEnabled = false;
      this.zoomContext.clearRect(dst.x, dst.y, dst.w, dst.h);
      this.zoomContext.drawImage(this.compositionCanvas,
        src.x, src.y, src.w, src.h,
        dst.x, dst.y, dst.w, dst.h);
    }
    this.drawLayers(this.zoomContext, src, dst);
    this.drawCrosshair(this.zoomContext, mousePosition, dst);
    this.drawFrameStatistics();
  }

  drawFrameStatistics() {
    let dst = new Rectangle(0, 0, this.chartCanvas.width, this.chartCanvas.height);
    let ctx = this.chartContext;
    ctx.clearRect(0, 0, dst.w, dst.h);

    let barW = 6 * this.ratio;
    let barWPadding = 2 * this.ratio;
    let barWTotal = barW + barWPadding;
    let maxFrames = dst.w / barWTotal | 0;

    if (this.frameStatistics.bits.show) {
      ctx.fillStyle = "#9400D3";
      let bits = this.frameStatistics.bits.values;
      let maxBitsPerFrame = Math.max.apply(null, bits);
      bits = bits.slice(Math.max(bits.length - maxFrames, 0));
      for (let i = 0; i < bits.length; i++) {
        let h = (bits[i] / maxBitsPerFrame) * dst.h | 0;
        ctx.fillRect(i * barWTotal, dst.h - h, barW, h);
      }
    }

    if (this.frameStatistics.errors.show) {
      ctx.fillStyle = "#E91E63";
      let mses = this.frameStatistics.errors.values.map(x => x ? x.mse : 0);
      let maxMsePerFrame = Math.max.apply(null, mses);
      mses = mses.slice(Math.max(mses.length - maxFrames, 0));
      for (let i = 0; i < mses.length; i++) {
        let h = (mses[i] / maxMsePerFrame) * dst.h | 0;
        ctx.fillRect(i * barWTotal, dst.h - h, barW, h);
      }
    }
  }

  drawCrosshair(ctx: CanvasRenderingContext2D, center: Vector, dst: Rectangle) {
    ctx.save();
    ctx.globalCompositeOperation = "difference";
    ctx.lineWidth = this.crosshairLineWidth;
    ctx.fillStyle = this.crosshairColor;
    ctx.fillStyle = "#FFFFFF";
    ctx.strokeStyle = "#FFFFFF";

    // Draw Lines
    ctx.beginPath();
    let lineOffset = getLineOffset(this.crosshairLineWidth);
    ctx.translate(lineOffset, lineOffset)
    ctx.moveTo(dst.x, dst.y + dst.h / 2);
    ctx.lineTo(dst.x + dst.w, dst.y + dst.h / 2);
    ctx.moveTo(dst.x + dst.w / 2, 0);
    ctx.lineTo(dst.x + dst.w / 2, dst.y + dst.h);
    ctx.closePath();
    ctx.stroke();

    // Draw Dot
    ctx.beginPath();
    ctx.arc(dst.x + dst.w / 2, dst.y + dst.h / 2, this.crosshairLineWidth * 2, 0, Math.PI * 2, true);
    ctx.closePath();
    ctx.fill();

    // Draw Text
    ctx.fillStyle = "#FFFFFF";

    ctx.lineWidth = 4;
    let textHeight = 12 * this.ratio;
    let textPadding = 4 * this.ratio;
    ctx.font = "bold " + textHeight + "px sans-serif";

    let x, y, text;
    x = dst.x + dst.w / 2 + textPadding;
    y = textHeight + textPadding;
    text = String(center.y) + " (" + (center.y / BLOCK_SIZE | 0) + ")";
    ctx.fillText(text, x, y);

    x = textPadding;
    y = dst.y + dst.h / 2 + textHeight + textPadding;
    text = String(center.x) + " (" + (center.x / BLOCK_SIZE | 0) + ")";
    ctx.fillText(text, x, y);

    ctx.restore();

  }

  drawSuperBlockGrid(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w | 0;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);
    ctx.save();
    ctx.globalAlpha = 1;
    let lineOffset = getLineOffset(this.gridLineWidth);
    ctx.translate(lineOffset, lineOffset);
    ctx.translate(-src.x * scale, -src.y * scale);
    ctx.beginPath();
    for (let c = 0; c <= cols; c += 8) {
      let offset = c * BLOCK_SIZE * scale;
      ctx.moveTo(offset, 0);
      ctx.lineTo(offset, scaledFrameSize.h);
    }
    for (let r = 0; r <= rows; r += 8) {
      let offset = r * BLOCK_SIZE * scale;
      ctx.moveTo(0, offset);
      ctx.lineTo(scaledFrameSize.w, offset);
    }
    ctx.closePath();
    ctx.strokeStyle = this.gridColor;
    ctx.lineWidth = this.gridLineWidth;
    ctx.stroke();
    ctx.restore();
  }

  drawTileGrid(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let {cols: tileColsLog2, rows: tileRowsLog2} = this.aom.getTileGridSizeLog2();
    let scale = dst.w / src.w | 0;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);
    ctx.save();
    ctx.globalAlpha = 1;
    let lineOffset = getLineOffset(this.tileGridLineWidth);
    ctx.translate(lineOffset, lineOffset);
    ctx.translate(-src.x * scale, -src.y * scale);
    ctx.beginPath();

    for (let c = 0; c <= 1 << tileColsLog2; c ++) {
      let offset = tileOffset(c, cols, tileColsLog2) * BLOCK_SIZE * scale;
      ctx.moveTo(offset, 0);
      ctx.lineTo(offset, scaledFrameSize.h);
    }
    for (let r = 0; r <= 1 << tileRowsLog2; r ++) {
      let offset = tileOffset(r, rows, tileRowsLog2) * BLOCK_SIZE * scale;
      ctx.moveTo(0, offset);
      ctx.lineTo(scaledFrameSize.w, offset);
    }
    ctx.closePath();
    ctx.strokeStyle = this.tileGridColor;
    ctx.lineWidth = this.tileGridLineWidth;
    ctx.stroke();
    ctx.restore();
  }


  drawFrame() {
    this.drawImages();
    this.showInfo && this.drawInfo();
    this.drawMain();
  }

  clearImage() {
    this.compositionContext.clearRect(0, 0, this.frameSize.w, this.frameSize.h);
    this.displayContext.clearRect(0, 0, this.frameSize.w * this.scale * this.ratio, this.frameSize.h * this.scale * this.ratio);
  }

  drawImages() {
    this.clearImage();
    this.showOriginalImage && this.drawOriginalImage();
    if (this.showDecodedImage && this.showPredictedImage) {
      this.drawDecodedImage();
      this.drawPredictedImage("difference");
      this.invertImage();
    } else {
      this.showDecodedImage && this.drawDecodedImage();
      this.showPredictedImage && this.drawPredictedImage();
    }
  }

  drawOriginalImage(compositeOperation: string = "source-over") {
    if (!this.y4mFile) {
      return;
    }
    let file = this.y4mFile;
    let frame = file.frames[this.frameNumber];
    let Yp = frame.y;
    let Ys = file.size.w;
    let Up = frame.cb;
    let Us = file.size.w >> 1;
    let Vp = frame.cr;
    let Vs = file.size.w >> 1;
    this.drawImage(file.buffer, Yp, Ys, Up, Us, Vp, Vs, compositeOperation);
  }

  drawDecodedImage(compositeOperation: string = "source-over") {
    let Yp = this.aom.get_plane(0);
    let Ys = this.aom.get_plane_stride(0);
    let Up = this.aom.get_plane(1);
    let Us = this.aom.get_plane_stride(1);
    let Vp = this.aom.get_plane(2);
    let Vs = this.aom.get_plane_stride(2);
    this.drawImage(this.aom.HEAPU8, Yp, Ys, Up, Us, Vp, Vs, compositeOperation);
  }

  drawPredictedImage(compositeOperation: string = "source-over") {
    let Yp = this.aom.get_predicted_plane_buffer(0);
    let Ys = this.aom.get_predicted_plane_stride(0);

    let Up = this.aom.get_predicted_plane_buffer(1);
    let Us = this.aom.get_predicted_plane_stride(1);

    let Vp = this.aom.get_predicted_plane_buffer(2);
    let Vs = this.aom.get_predicted_plane_stride(2);
    this.drawImage(this.aom.HEAPU8, Yp, Ys, Up, Us, Vp, Vs, compositeOperation);
  }

  invertImage() {
    this.compositionContext.globalCompositeOperation = "difference";
    this.compositionContext.fillStyle = "#FFFFFF";
    this.compositionContext.fillRect(0, 0, this.frameSize.w, this.frameSize.h);
  }

  drawImage(H: Uint8Array, Yp, Ys, Up, Us, Vp, Vs, compositeOperation: string) {
    let I = this.imageData.data;

    let w = this.frameSize.w;
    let h = this.frameSize.h;


    let showY = this.showY;
    let showU = this.showU;
    let showV = this.showV;

    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        let index = (Math.imul(y, w) + x) << 2;

        let Y = showY ? H[Yp + Math.imul(y, Ys) + x] : 128;
        let U = showU ? H[Up + Math.imul(y >> 1, Us) + (x >> 1)] : 128;
        let V = showV ? H[Vp + Math.imul(y >> 1, Vs) + (x >> 1)] : 128;

        let bgr = YUV2RGB(Y, U, V);

        let r = (bgr >>  0) & 0xFF;
        let g = (bgr >>  8) & 0xFF;
        let b = (bgr >> 16) & 0xFF;

        I[index + 0] = r;
        I[index + 1] = g;
        I[index + 2] = b;
        I[index + 3] = 255;
      }
    }

    if (this.imageData) {
      this.frameContext.putImageData(this.imageData, 0, 0);
      this.compositionContext.globalCompositeOperation = compositeOperation;
      this.compositionContext.drawImage(this.frameCanvas, 0, 0, this.frameSize.w, this.frameSize.h);
    }

    this.drawMain();
  }

  drawMain() {
    // Draw composited image.
    this.displayContext.mozImageSmoothingEnabled = false;
    this.displayContext.imageSmoothingEnabled = false;
    let dw = this.frameSize.w * this.scale * this.ratio;
    let dh = this.frameSize.h * this.scale * this.ratio;
    this.displayContext.drawImage(this.compositionCanvas, 0, 0, dw, dh);

    // Draw Layers
    let ctx = this.overlayContext;
    let ratio = window.devicePixelRatio || 1;
    ctx.clearRect(0, 0, this.frameSize.w * this.scale * ratio, this.frameSize.h * this.scale * ratio);

    let src = Rectangle.createRectangleFromSize(this.frameSize);
    let dst = src.clone().multiplyScalar(this.scale * this.ratio);

    this.drawLayers(ctx, src, dst);
  }

  drawLayers(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    this.showMotionVectors && this.drawMotionVectors(ctx, src, dst);
    this.showDering && this.drawDering(ctx, src, dst);
    this.showBits &&  this.drawBits(ctx, src, dst);
    this.showMode && this.drawMode(ctx, src, dst);
    this.showSkip && this.drawSkip(ctx, src, dst);
    this.showTransformSplit && this.drawTransformSplit(ctx, src, dst);
    this.showBlockSplit && this.drawBlockSplit(ctx, src, dst);
    this.showSuperBlockGrid && this.drawSuperBlockGrid(ctx, src, dst);
    this.showTileGrid && this.drawTileGrid(ctx, src, dst);
  }

  drawMode(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);

    ctx.save();
    ctx.lineWidth = this.modeLineWidth;
    ctx.strokeStyle = this.modeColor;
    ctx.globalAlpha = 0.5;
    let lineOffset = getLineOffset(this.modeLineWidth);
    ctx.translate(-src.x * scale, -src.y * scale);

    function line(x, y, dx, dy) {
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + dx, y + dy);
      ctx.closePath();
      ctx.stroke();
    }

    function mode(m: AOMAnalyzerPredictionMode, x, y, dx, dy) {
      let hx = dx / 2;
      let hy = dy / 2;
      switch (m) {
        case AOMAnalyzerPredictionMode.V_PRED:
          line(x + hx + lineOffset, y, 0, dy);
          break;
        case AOMAnalyzerPredictionMode.H_PRED:
          line(x, y + hy + lineOffset, dx, 0);
          break;
        case AOMAnalyzerPredictionMode.D45_PRED:
          line(x, y + dy, dx, -dy);
          break;
        case AOMAnalyzerPredictionMode.D63_PRED:
          line(x, y + dy, hx, -dy);
          break;
        case AOMAnalyzerPredictionMode.D135_PRED:
          line(x, y, dx, dy);
          break;
        case AOMAnalyzerPredictionMode.D117_PRED:
          line(x + hx, y, hx, dy);
          break;
        case AOMAnalyzerPredictionMode.D153_PRED:
          line(x, y + hy, dx, hy);
          break;
        case AOMAnalyzerPredictionMode.D207_PRED:
          line(x, y + hy, dx, -hy);
          break;
        default:
          ctx.fillStyle = colors[m];
          ctx.fillRect(x, y, dx, dy);
          break;
      }
    }

    let S = scale * BLOCK_SIZE;

    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let i = this.aom.get_mi_property(MIProperty.GET_MI_MODE, c, r);
        mode(i, c * S, r * S, S, S);
      }
    }

    ctx.restore();
  }

  drawBlockSplit(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);

    ctx.save();
    ctx.lineWidth = this.splitLineWidth;
    ctx.strokeStyle = this.splitColor;
    ctx.globalAlpha = 1;
    let lineOffset = getLineOffset(this.splitLineWidth);
    ctx.translate(lineOffset, lineOffset);
    ctx.translate(-src.x * scale, -src.y * scale);
    ctx.beginPath();

    let lineWidth = 1;
    ctx.lineWidth = lineWidth;

    function split(x, y, dx, dy) {
      ctx.beginPath();
      ctx.save();
      ctx.moveTo(x, y);
      ctx.lineTo(x + dx, y);
      ctx.moveTo(x, y);
      ctx.lineTo(x, y + dy);
      ctx.restore();
      ctx.closePath();
      ctx.stroke();
    }

    let S = scale * BLOCK_SIZE;
    // Draw block sizes above 8x8.
    for (let i = 3; i < blockSizes.length; i++) {
      let dc = 1 << (blockSizes[i][0] - 3);
      let dr = 1 << (blockSizes[i][1] - 3);
      for (let c = 0; c < cols; c += dc) {
        for (let r = 0; r < rows; r += dr) {
          let t = this.aom.get_mi_property(MIProperty.GET_MI_BLOCK_SIZE, c, r);
          if (t == i) {
            split(c * S, r * S, dc * S, dr * S);
          }
        }
      }
    }

    // Draw block sizes below 8x8.
    for (let c = 0; c < cols; c ++) {
      for (let r = 0; r < rows; r ++) {
        let t = this.aom.get_mi_property(MIProperty.GET_MI_BLOCK_SIZE, c, r);
        let x = c * S;
        let y = r * S;
        switch (t) {
          case 0: {
            let dx = S >> 1;
            let dy = S >> 1;
            split(x,      y,      dx, dy);
            split(x + dx, y,      dx, dy);
            split(x,      y + dy, dx, dy);
            split(x + dx, y + dy, dx, dy);
            break;
          }
          case 1: {
            let dx = S >> 1;
            let dy = S;
            split(x,      y, dx, dy);
            split(x + dx, y, dx, dy);
            break;
          }
          case 2: {
            let dx = S;
            let dy = S >> 1;
            split(x, y,      dx, dy);
            split(x, y + dy, dx, dy);
            break;
          }
        }
      }
    }

    ctx.closePath();
    ctx.stroke();
    ctx.restore();
  }

  drawTransformSplit(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);

    ctx.save();
    ctx.lineWidth = this.transformLineWidth;
    ctx.strokeStyle = this.transformColor;
    ctx.globalAlpha = 1;
    let lineOffset = getLineOffset(this.transformLineWidth);
    ctx.translate(lineOffset, lineOffset);
    ctx.translate(-src.x * scale, -src.y * scale);
    ctx.beginPath();
    let lineWidth = 1;
    ctx.lineWidth = lineWidth;

    function split(x, y, dx, dy) {
      ctx.beginPath();
      ctx.save();
      ctx.moveTo(x, y);
      ctx.lineTo(x + dx, y);
      ctx.moveTo(x, y);
      ctx.lineTo(x, y + dy);
      ctx.restore();
      ctx.closePath();
      ctx.stroke();
    }

    // Draw >= 8x8 transforms
    let S = scale * BLOCK_SIZE;
    for (let i = 1; i < 4; i++) {
      let side = 1 << (i - 1);
      for (let c = 0; c < cols; c += side) {
        for (let r = 0; r < rows; r += side) {
          let t = this.aom.get_mi_property(MIProperty.GET_MI_TRANSFORM_SIZE, c, r);
          if (t == i) {
            split(c * S, r * S, side * S, side * S);
          }
        }
      }
    }

    // Draw 4x4 transforms
    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let t = this.aom.get_mi_property(MIProperty.GET_MI_TRANSFORM_SIZE, c, r);
        if (t == 0) {
          let x = c * S;
          let y = r * S;
          let dx = S >> 1;
          let dy = S >> 1;
          split(x,      y,      dx, dy);
          split(x + dx, y,      dx, dy);
          split(x,      y + dy, dx, dy);
          split(x + dx, y + dy, dx, dy);
        }
      }
    }

    ctx.closePath();
    ctx.stroke();
    ctx.restore();
  }

  drawFillBlock(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle, setFillStyle: (miCol: number, miRow: number, col: number, row: number) => boolean) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);
    ctx.save();
    ctx.globalAlpha = 0.75;
    ctx.translate(-src.x * scale, -src.y * scale);
    let s = scale * BLOCK_SIZE;
    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let miBlockSize = this.aom.get_mi_property(MIProperty.GET_MI_BLOCK_SIZE, c, r);
        let w = scale * (1 << blockSizes[miBlockSize][0]);
        let h = scale * (1 << blockSizes[miBlockSize][1]);
        switch (miBlockSize) {
          case AOMAnalyzerBlockSize.BLOCK_4X4:
            setFillStyle(c, r, 0, 0) && ctx.fillRect(c * s,     r * s,     w, h);
            setFillStyle(c, r, 0, 1) && ctx.fillRect(c * s,     r * s + h, w, h);
            setFillStyle(c, r, 1, 0) && ctx.fillRect(c * s + w, r * s,     w, h);
            setFillStyle(c, r, 1, 1) && ctx.fillRect(c * s + w, r * s + h, w, h);
            break;
          case AOMAnalyzerBlockSize.BLOCK_8X4:
            setFillStyle(c, r, 0, 0) && ctx.fillRect(c * s,     r * s,     w, h);
            setFillStyle(c, r, 0, 1) && ctx.fillRect(c * s,     r * s + h, w, h);
            break;
          case AOMAnalyzerBlockSize.BLOCK_4X8:
            setFillStyle(c, r, 0, 0) && ctx.fillRect(c * s,     r * s,     w, h);
            setFillStyle(c, r, 1, 0) && ctx.fillRect(c * s + w, r * s,     w, h);
            break;
          default:
            setFillStyle(c, r, 0, 0) && ctx.fillRect(c * s, r * s, s, s);
            break;
        }
      }
    }
    ctx.restore();
  }

  drawFillSuperBlock(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle, setFillStyle: (miCol: number, miRow: number) => boolean) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);
    ctx.save();
    ctx.globalAlpha = 0.75;
    ctx.translate(-src.x * scale, -src.y * scale);
    let s = scale * BLOCK_SIZE;
    for (let c = 0; c < cols; c += 8) {
      for (let r = 0; r < rows; r += 8) {
        let w = s * 8;
        let h = s * 8;
        setFillStyle(c, r) && ctx.fillRect(c * s, r * s, w, h);
      }
    }
    ctx.restore();
  }

  drawDering(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    this.drawFillSuperBlock(ctx, src, dst, (miCol, miRow) => {
      let i = this.aom.get_mi_property(MIProperty.GET_MI_DERING_GAIN, miCol, miRow);
      if (i == 0) return false;
      ctx.fillStyle = "rgba(33,33,33," + (i / 4) + ")";
      return true;
    });
  }

  drawSkip(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    this.drawFillBlock(ctx, src, dst, (miCol, miRow, col, row) => {
      let i = this.aom.get_mi_property(MIProperty.GET_MI_SKIP, miCol, miRow);
      if (i == 0) return false;
      ctx.fillStyle = this.skipColor;
      return true;
    });
  }

  getMIBlockBitsPerPixel(c: number, r: number): number {
    // Bits are stored at the 8x8 level, even if the block is split further.
    let blockSize = this.getMIBlockSize(c, r, AOMAnalyzerBlockSize.BLOCK_8X8);
    let blockArea = blockSize.w * blockSize.h;
    let miBits = this.getMIBits(c, r);
    return miBits / blockArea;
  }

  drawBits(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let miMaxBitsPerPixel = 0;
    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let miBitsPerPixel = this.getMIBlockBitsPerPixel(c, r);
        miMaxBitsPerPixel = Math.max(miMaxBitsPerPixel, miBitsPerPixel);
      }
    }
    let gradient = tinygradient([
      {color: tinycolor("#9400D3").brighten(100), pos: 0},
      {color: tinycolor("#9400D3"), pos: 1}
    ]);
    let colorRange = gradient.rgb(32).map(x => x.toString());
    this.drawFillBlock(ctx, src, dst, (miCol, miRow, col, row) => {
      let miBitsPerPixel = this.getMIBlockBitsPerPixel(miCol, miRow);
      let color = colorRange[((miBitsPerPixel / miMaxBitsPerPixel) * (colorRange.length - 1)) | 0];
      ctx.fillStyle = color;
      return true;
    });
  }

  getMotionVector(c: number, r: number, i: number): Vector {
    let mv = this.aom.get_mi_property(MIProperty.GET_MI_MV, c, r, i);
    let y = (mv >> 16);
    let x = (((mv & 0xFFFF) << 16) >> 16);
    return new Vector(x, y);
  }

  getFrameError(): ErrorMetrics {
    let file = this.y4mFile;
    if (!this.y4mFile) {
      return;
    }
    let frame = file.frames[this.frameNumber];

    let AYp = frame.y;
    let AYs = file.size.w;
    let AH = file.buffer;

    let BYp = this.aom.get_plane(0);
    let BYs = this.aom.get_plane_stride(0);
    let BH = this.aom.HEAPU8;

    let h = this.frameSize.h;
    let w = this.frameSize.w;

    let Ap = AYp;
    let Bp = BYp;
    let error = 0;
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        let d = AH[Ap + x] - BH[Bp + x];
        error += d * d;
      }
      Ap += AYs;
      Bp += BYs;
    }
    return new ErrorMetrics(error, error / this.frameSize.area());
  }

  getMIError(mi: Vector): ErrorMetrics {
    let file = this.y4mFile;
    if (!this.y4mFile) {
      return;
    }
    let frame = file.frames[this.frameNumber];
    let AYp = frame.y;
    let AYs = file.size.w;
    let AH = file.buffer;

    let BYp = this.aom.get_plane(0);
    let BYs = this.aom.get_plane_stride(0);
    let BH = this.aom.HEAPU8;
    let size = this.getMIBlockSize(mi.x, mi.y);
    let Ap = AYp + mi.y * BLOCK_SIZE * AYs + mi.x * BLOCK_SIZE;
    let Bp = BYp + mi.y * BLOCK_SIZE * BYs + mi.x * BLOCK_SIZE;
    let error = 0;
    for (let y = 0; y < size.h; y++) {
      for (let x = 0; x < size.w; x++) {
        let d = AH[Ap + x] - BH[Bp + x];
        error += d * d;
      }
      Ap += AYs;
      Bp += BYs;
    }
    return new ErrorMetrics(error, error / size.area());
  }

  uiBlockInfo(name: string): string | number {
    if (!this.aom) {
      return;
    }
    let mi = this.getMIUnderMouse();
    switch (name) {
      case "blockSize":
        return AOMAnalyzerBlockSize[this.aom.get_mi_property(MIProperty.GET_MI_BLOCK_SIZE, mi.x, mi.y)];
      case "blockSkip":
        return this.aom.get_mi_property(MIProperty.GET_MI_SKIP, mi.x, mi.y);
      case "predictionMode":
        return AOMAnalyzerPredictionMode[this.aom.get_mi_property(MIProperty.GET_MI_MODE, mi.x, mi.y)];
      case "deringGain":
        return String(this.aom.get_mi_property(MIProperty.GET_MI_DERING_GAIN, mi.x, mi.y));
      case "motionVector":
        return this.getMotionVector(mi.x, mi.y, 0).toString() + " " +
               this.getMotionVector(mi.x, mi.y, 1).toString();
      case "referenceFrames":
        return this.aom.get_mi_property(MIProperty.GET_MI_REFERENCE_FRAME, mi.x, mi.y, 0) + ", " +
               this.aom.get_mi_property(MIProperty.GET_MI_REFERENCE_FRAME, mi.x, mi.y, 1);
      case "transformType":
        return  AOMAnalyzerTransformType[this.aom.get_mi_property(MIProperty.GET_MI_TRANSFORM_TYPE, mi.x, mi.y)];
      case "transformSize":
        return AOMAnalyzerTransformSize[this.aom.get_mi_property(MIProperty.GET_MI_TRANSFORM_SIZE, mi.x, mi.y)];
      case "bits":
        return this.aom.get_mi_property(MIProperty.GET_MI_BITS, mi.x, mi.y);
      case "blockError": {
        let error = this.getMIError(mi);
        return error ? error.toString() : "N/A";
      }
      case "frameError": {
        let error = this.getFrameError();
        return error ? error.toString() : "N/A";
      }
    }
    return "?";
  }

  drawMotionVectors(ctx: CanvasRenderingContext2D, src: Rectangle, dst: Rectangle) {
    let {cols, rows} = this.aom.getMIGridSize();
    let scale = dst.w / src.w;
    let scaledFrameSize = this.frameSize.clone().multiplyScalar(scale);

    ctx.save();
    ctx.globalAlpha = 0.75;
    ctx.translate(-src.x * scale, -src.y * scale);
    let S = scale * BLOCK_SIZE;

    let gradient = tinygradient([
      {color: tinycolor(this.mvColor).brighten(50), pos: 0},
      {color: tinycolor(this.mvColor), pos: 1}
    ]);

    let colorRange = gradient.rgb(32);

    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let v = this.getMotionVector(c, r, 0);
        v.clampLength(0, 31);
        let l = v.length() | 0;
        ctx.fillStyle = colorRange[l | 0];
        ctx.fillRect(c * S, r * S, S, S);
      }
    }
    ctx.restore();
  }

  drawVector(a: Vector, b: Vector) {
    let ctx = this.overlayContext;
    let c = b.clone().sub(a);
    let length = c.length();

    c.add(a);
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(c.x, c.y);
    ctx.closePath();
    ctx.stroke();
    return;
  }

  drawVector3(a: Vector, b: Vector) {
    let ctx = this.overlayContext;
    let c = b.clone().sub(a);
    let length = c.length();

    c.add(a);

    ctx.fillStyle = "#000000";
    ctx.beginPath();
    ctx.arc(c.x, c.y, this.scale, 0, 2 * Math.PI);
    ctx.closePath();
    ctx.fill();

    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(c.x, c.y);
    ctx.closePath();
    ctx.stroke();


    return;
  }

  drawVector2(p1: Vector, p2: Vector) {
    let ctx = this.overlayContext;
    ctx.save();
    let dist = Math.sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));

    ctx.beginPath();
    ctx.lineWidth = 1;
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();

    let angle = Math.acos((p2.y - p1.y) / dist);

    if (p2.x < p1.x) angle = 2 * Math.PI - angle;

    let size = 2 * Math.min(this.scale, 4);

    ctx.beginPath();
    ctx.translate(p2.x, p2.y);
    ctx.rotate(-angle);
    ctx.lineWidth = 1;

    ctx.moveTo(0, 0);
    ctx.lineTo(-size, -size);
    ctx.lineTo(0, size * 2);
    ctx.lineTo(size, -size);
    ctx.lineTo(0, 0);

    // ctx.moveTo(0, -size);
    // ctx.lineTo(-size, -size);
    // ctx.lineTo(0, size * 2);
    // ctx.lineTo(size, -size);
    // ctx.lineTo(0, -size);

    ctx.closePath();
    ctx.fill();
    ctx.restore();
  }

  drawMotionVectors2() {
    let ctx = this.overlayContext;
    let {cols, rows} = this.aom.getMIGridSize();
    let s = this.scale * this.ratio;
    ctx.globalAlpha = 0.5;


    let gradient = tinygradient([
      {color: tinycolor(this.mvColor).brighten(50), pos: 0},
      {color: tinycolor(this.mvColor), pos: 1}
    ]);

    let colorRange = gradient.rgb(32);

    for (let c = 0; c < cols; c++) {
      for (let r = 0; r < rows; r++) {
        let v = this.getMotionVector(c, r, 0);
        v.clampLength(0, 31);
        let l = v.length() | 0;
        ctx.fillStyle = colorRange[l | 0];
        ctx.fillRect(c * 8 * s, r * 8 * s, 8 * s, 8 * s);

        // let offset = s * 8 / 2;
        // let a = new Vector(c * 8 * s + offset, r * 8 * s + offset);
        // let v = new Vector(x, y).divideScalar(8).multiplyScalar(s);
        // let l = v.length() | 0;
        // v.clampLength(0, 31);

        // this.drawVector3(
        //   a.clone().add(v),
        //   a
        // )
      }
    }
    ctx.globalAlpha = 1;
  }
}

function clamp(v, a, b) {
	if (v < a) {
		v = a;
	}
	if (v > b) {
		v = b;
	}
	return v;
}

function YUV2RGB(yValue, uValue, vValue) {
  let rTmp = yValue + (1.370705 * (vValue - 128));
  let gTmp = yValue - (0.698001 * (vValue - 128)) - (0.337633 * (uValue - 128));
  let bTmp = yValue + (1.732446 * (uValue - 128));
  let r = clamp(rTmp | 0, 0, 255) | 0;
  let g = clamp(gTmp | 0, 0, 255) | 0;
  let b = clamp(bTmp | 0, 0, 255) | 0;
  return (b << 16) | (g << 8) | (r << 0);
}

function getUrlParameters(): any {
  let url = window.location.search.substring(1);
  url = url.replace(/\/$/, ""); // Replace / at the end that gets inserted by browsers.
  let params = {};
  url.split('&').forEach(function (s) {
    let t = s.split('=');
    params[t[0]] = decodeURIComponent(t[1]);
  });
  return params;
};

angular
.module('AomInspectorApp', ['ngMaterial', 'color.picker'])
.config(['$mdIconProvider', function($mdIconProvider) {
    $mdIconProvider
      // .iconSet('social', 'img/icons/sets/social-icons.svg', 24)
      .defaultIconSet('img/icons/sets/core-icons.svg', 24);
}])
.filter('keyboardShortcut', function($window) {
  return function(str) {
    if (!str) return;
    let keys = str.split('-');
    let isOSX = /Mac OS X/.test($window.navigator.userAgent);
    let seperator = (!isOSX || keys.length > 2) ? '+' : '';
    let abbreviations = {
      M: isOSX ? '⌘' : 'Ctrl',
      A: isOSX ? 'Option' : 'Alt',
      S: 'Shift'
    };
    return keys.map(function(key, index) {
      let last = index == keys.length - 1;
      return last ? key : abbreviations[key];
    }).join(seperator);
  };
})
.controller('AppCtrl', ['$scope', '$interval', AppCtrl])
.directive('selectOnClick', ['$window', function ($window) {
  return {
    restrict: 'A',
    link: function (scope, element, attrs) {
      element.on('click', function () {
        if (!$window.getSelection().toString()) {
          // Required for mobile Safari
          this.setSelectionRange(0, this.value.length)
        }
      });
    }
  };
}]);
;

window.Module = {
  noExitRuntime: true,
  preRun: [],
  postRun: [function() {
    // startVideo();
  }],
  memoryInitializerPrefixURL: "bin/",
  arguments: ['input.ivf', 'output.raw']
};