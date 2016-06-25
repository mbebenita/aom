declare var angular: any;
declare var FS: any;
declare var Mousetrap: any;
declare var tinycolor: any;
declare var tinygradient: any;

var colors = [
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
  var r = parseInt(hex.slice(1,3), 16),
      g = parseInt(hex.slice(3,5), 16),
      b = parseInt(hex.slice(5,7), 16),
      a = "";
  if (alpha) {
    a = ", 1";
  }
  return "rgb(" + r + ", " + g + ", " + b + a + ")";
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

interface AOMInternal {
  _read_frame (): number;
  _get_plane (pli: number): number;
  _get_plane_stride (pli: number): number;
  _get_plane_width (pli: number): number;
  _get_plane_height (pli: number): number;
  _get_mi_rows(): number;
  _get_mi_cols(): number;
  _get_mi_mv(c: number, r: number): number;
  _get_mi_mode(c: number, r: number): AOMAnalyzerPredictionMode;
  _get_mi_skip(c: number, r: number): number;
  _get_mi_block_size(c: number, r: number): AOMAnalyzerBlockSize;
  _get_dering_gain(c: number, r: number): number;
  _get_frame_count(): number;
  _get_frame_width(): number;
  _get_frame_height(): number;
  _open_file(): number;
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
  get_mi_rows(): number {
    return this.native._get_mi_rows();
  }
  get_mi_cols(): number {
    return this.native._get_mi_cols();
  }
  get_mi_mv(c: number, r: number): number {
    return this.native._get_mi_mv(c, r);
  }
  get_mi_mode(c: number, r: number): AOMAnalyzerPredictionMode {
    return this.native._get_mi_mode(c, r);
  }
  get_mi_skip(c: number, r: number): number {
    return this.native._get_mi_skip(c, r);
  }
  get_mi_block_size(c: number, r: number): AOMAnalyzerBlockSize {
    return this.native._get_mi_block_size(c, r);
  }
  get_dering_gain(c: number, r: number): number {
    return this.native._get_dering_gain(c, r);
  }
  get_frame_count(): number {
    return this.native._get_frame_count();
  }
  getFrameSize(): Size {
    return new Size(this.native._get_frame_width(), this.native._get_frame_height());
  }
}

class Size {
  constructor(public w: number, public h: number) {
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
		var length = this.length();
		this.multiplyScalar(Math.max(min, Math.min(max, length)) / length);
		return this;
	}
}

class AppCtrl {
  aom: AOM = null;
  size: Size = new Size(128, 128);
  fileSize: number = 0;
  ratio: number = 1;
  scale: number = 1;

  showY: boolean = true;
  showU: boolean = true;
  showV: boolean = true;
  showImage: boolean = true;

  showGrid: boolean = false;
  showSplit: boolean = false;
  showMotionVectors: boolean = false;
  showDering: boolean = false;
  showMode: boolean = false;
  showInfo: boolean = false;
  showSkip: boolean = false;

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
    showImage: {
      key: "1",
      description: "Show Image",
      detail: "Shows image planes.",
      updatesImage: true,
      default: true
    },
    showGrid: {
      key: "2",
      description: "Show SB Grid",
      detail: "Shows 64x64 mode info grid.",
      default: false
    },
    showSplit: {
      key: "3",
      description: "Show Split Grid",
      detail: "Shows block partitions.",
      default: false
    },
    showDering: {
      key: "4",
      description: "Show Dering",
      detail: "Shows blocks where the deringing filter is applied.",
      default: false
    },
    showMotionVectors: {
      key: "5",
      description: "Show Motion Vectors",
      detail: "Shows motion vectors, darker colors represent longer vectors.",
      default: false
    },
    showMode: {
      key: "6",
      description: "Show Mode",
      detail: "Shows prediction modes.",
      default: false
    },
    showInfo: {
      key: "7",
      description: "Show Info",
      detail: "Shows mode info details.",
      default: false
    },
    showSkip: {
      key: "8",
      description: "Show Skip",
      detail: "Shows skip flags.",
      default: false
    }
  };

  frameNumber: number = 0;

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

  displayContext: CanvasRenderingContext2D = null;
  overlayContext: CanvasRenderingContext2D = null;
  zoomContext: CanvasRenderingContext2D = null;
  zoomSize = 512;
  mousePosition: Vector = new Vector(0, 0);
  imageData: ImageData = null;

  frameCanvas: HTMLCanvasElement;
  frameContext: CanvasRenderingContext2D = null;

  lastDecodeFrameTime: number = 0;

  predictionModeNames: string [] = [];
  predictionModeColors: string [] = colors;
  blockSizeNames: string [] = [];

  modeColor = "hsl(79, 20%, 50%)";
  mvColor = "hsl(232, 36%, 41%)";
  skipColor = "hsla(326, 53%, 42%, 0.19)";
  gridColor = "rgba(33,33,33,0.75)";
  splitGridColor = "rgba(33,33,33,0.50)";
  sharingLink = "";

  $scope: any;
  $interval: any;

  constructor($scope, $interval) {
    var self = this;

    this.$scope = $scope;
    // File input types don't have angular bindings, so we need set the
    // event handler on the scope object.
    $scope.fileInputNameChanged = function() {
      var input = <any>event.target;
      var reader = new FileReader();
      reader.onload = function() {
        var buffer = reader.result;
        self.openFileBytes(new Uint8Array(buffer));
        self.playFrameAsync(1, () => {
          self.drawFrame();
        });
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

    this.overlayCanvas.addEventListener("mousemove", this.onMouseMove.bind(this));

    this.frameCanvas = document.createElement("canvas");
    this.frameContext = this.frameCanvas.getContext("2d");

    var parameters = getUrlParameters();
    for (var name in this.options) {
      this[name] = this.options[name].default;
      if (name in parameters) {
        // TODO: Check for boolean here..
        this[name] = parameters[name] == "true";
      }
    }
    var frames = parseInt(parameters.frameNumber) || 1;

    this.aom = new AOM();
    var file = "media/crosswalk.ivf";
    this.openFile(file, () => {
      this.playFrameAsync(frames, () => {
        this.drawFrame();
      })
    });

    this.installKeyboardShortcuts();
    this.initEnums();
  }

  initEnums() {
    for (var k in AOMAnalyzerPredictionMode) {
      if (isNaN(parseInt(k))) {
        this.predictionModeNames.push(k);
      }
    }
    for (var k in AOMAnalyzerBlockSize) {
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

    var self = this;
    function toggle(name) {
      var option = this.options[name];
      self[name] = !self[name];
      if (option.updatesImage) {
        self.clearImage();
        self.drawImage();
      }
      self.drawLayers();
      self.uiApply();
    }

    for (var name in this.options) {
      var option = this.options[name];
      if (option.key) {
        Mousetrap.bind([option.key], toggle.bind(this, name));
      }
    }
  }

  onMouseMove(event: MouseEvent) {
    function getMousePosition(canvas: HTMLCanvasElement, event: MouseEvent) {
      var rect = canvas.getBoundingClientRect();
      return new Vector(
        event.clientX - rect.left,
        event.clientY - rect.top
      );
    }
    // console.info(getMousePosition(this.overlayCanvas, event));
    this.mousePosition = getMousePosition(this.overlayCanvas, event);
    this.drawZoom();
    this.uiApply();
  }

  getMI(): Vector {
    var v = this.mousePosition;
    var x = v.x / this.scale | 0;
    var y = v.y / this.scale | 0;
    return new Vector(x / 8 | 0, y / 8 | 0);
  }

  uiMIMode() {
    var mi = this.getMI();
    return this.predictionModeNames[this.aom.get_mi_mode(mi.x, mi.y)];
  }

  uiMIDeringGain() {
    var mi = this.getMI();
    return this.aom.get_dering_gain(mi.x, mi.y);
  }

  uiMIBlockSize() {
    var mi = this.getMI();
    return this.blockSizeNames[this.aom.get_mi_block_size(mi.x, mi.y)];
  }

  openFileBytes(buffer: Uint8Array) {
    this.fileSize = buffer.length;
    FS.writeFile("/tmp/input.ivf", buffer, { encoding: "binary" });
    this.aom.open_file();
    this.size = this.aom.getFrameSize();
    this.resetCanvases();
  }

  openFile(path: string, next: () => any = null) {
    this.downloadFile(path, (buffer: Uint8Array) => {
      this.openFileBytes(buffer);
      next();
    });
  }

  resetCanvases() {
    this.frameCanvas.width = this.size.w;
		this.frameCanvas.height = this.size.h;

    this.imageData = this.frameContext.createImageData(this.size.w, this.size.h);

    this.container.style.width = (this.size.w * this.scale) + "px";
		this.container.style.height = (this.size.h * this.scale) + "px";

    this.displayCanvas.style.width = (this.size.w * this.scale) + "px";
		this.displayCanvas.style.height = (this.size.h * this.scale) + "px";
    this.displayCanvas.width = this.size.w * this.scale * this.ratio;
		this.displayCanvas.height = this.size.h * this.scale * this.ratio;

    this.overlayCanvas.style.width = (this.size.w * this.scale) + "px";
		this.overlayCanvas.style.height = (this.size.h * this.scale) + "px";
    this.overlayCanvas.width = this.size.w * this.scale * this.ratio;
		this.overlayCanvas.height = this.size.h * this.scale * this.ratio;

    this.zoomCanvas.style.width = this.zoomSize + "px";
		this.zoomCanvas.style.height = this.zoomSize + "px";
    this.zoomCanvas.width = this.zoomSize * this.ratio;
		this.zoomCanvas.height = this.zoomSize * this.ratio;
  }

  downloadFile(path: string, next: (buffer: Uint8Array) => void) {
    var xhr = new XMLHttpRequest();
    var self = this;
    self.progressMode = "determinate";
    xhr.open("GET", path, true);
    xhr.responseType = "arraybuffer";
    xhr.send();
    xhr.addEventListener("progress", (e) => {
      var progress = (e.loaded / e.total) * 100;
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

  uiAction(name) {
    var file;
    switch (name) {
      case "open-file":
        angular.element(document.querySelector('#fileInput'))[0].click();
        break;
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
    var url = location.protocol + '//' + location.host + location.pathname;
    var args = {
      frameNumber: this.frameNumber
    };
    for (var name in this.options) {
      // Ignore default values.
      if (this[name] == this.options[name].default) {
        continue;
      }
      args[name] = this[name];
    }
    var argList = [];
    for (var arg in args) {
      argList.push(arg + "=" + encodeURIComponent(args[arg]));
    }
    var argListString = argList.join("&");
    this.sharingLink = url + "?" + argListString;
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
    for (var i = 0; i < count; i++) {
      var s = performance.now();
      if (this.aom.read_frame()) {
        return false;
      }
      this.lastDecodeFrameTime = performance.now() - s;
      this.frameNumber ++;
    }
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

  drawZoom() {
    var v = this.mousePosition;

    var rect = this.displayCanvas.getBoundingClientRect();

    var size = 32;
    var halfSize = size / 2;
    // this.zoomCanvas.style.left = (rect.left + v.x - halfSize) + "px";
    // this.zoomCanvas.style.top = (rect.top + v.y + 10) + "px";

    this.zoomContext.mozImageSmoothingEnabled = false;
    this.zoomContext.imageSmoothingEnabled = false;

    var x = v.x / this.scale | 0;
    var y = v.y / this.scale | 0;
    this.zoomContext.clearRect(0, 0, this.zoomSize * this.ratio, this.zoomSize * this.ratio);
    this.zoomContext.drawImage(this.frameCanvas,
      x - halfSize, y - halfSize, size, size,
      0, 0, this.zoomSize * this.ratio, this.zoomSize * this.ratio);
  }

  drawFrame() {
    this.clearImage();
    this.drawImage();
    this.drawZoom();
    this.drawLayers();
  }

  clearImage() {
    var ctx = this.displayContext;
    ctx.clearRect(0, 0, this.size.w * this.scale * this.ratio, this.size.h * this.scale * this.ratio);
  }

  drawImage() {
    var Yp = this.aom.get_plane(0);
    var Ys = this.aom.get_plane_stride(0);
    var Up = this.aom.get_plane(1);
    var Us = this.aom.get_plane_stride(1);
    var Vp = this.aom.get_plane(2);
    var Vs = this.aom.get_plane_stride(2);

    var I = this.imageData.data;
    var H = this.aom.HEAPU8;

    var w = this.size.w;
    var h = this.size.h;


    var showY = this.showY;
    var showU = this.showU;
    var showV = this.showV;

    // var s = performance.now();
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var index = (Math.imul(y, w) + x) << 2;

        var Y = showY ? H[Yp + Math.imul(y, Ys) + x] : 128;
        var U = showU ? H[Up + Math.imul(y >> 1, Us) + (x >> 1)] : 128;
        var V = showV ? H[Vp + Math.imul(y >> 1, Vs) + (x >> 1)] : 128;

        var bgr = YUV2RGB(Y, U, V);

        var r = (bgr >>  0) & 0xFF;
        var g = (bgr >>  8) & 0xFF;
        var b = (bgr >> 16) & 0xFF;

        I[index + 0] = r;
        I[index + 1] = g;
        I[index + 2] = b;
        I[index + 3] = 255;
      }
    }
    // console.log(performance.now() - s);

    if (this.showImage && this.imageData) {
      this.frameContext.putImageData(this.imageData, 0, 0);
      this.displayContext.mozImageSmoothingEnabled = false;
      this.displayContext.imageSmoothingEnabled = false;

      var dw = this.size.w * this.scale * this.ratio;
      var dh = this.size.h * this.scale * this.ratio;
      this.displayContext.drawImage(this.frameCanvas, 0, 0, dw, dh);


    }

    this.drawLayers();
  }

  drawLayers() {
    var ctx = this.overlayContext;
    var ratio = window.devicePixelRatio || 1;

    ctx.clearRect(0, 0, this.size.w * this.scale * ratio, this.size.h * this.scale * ratio);

    this.showGrid && this.drawGrid();
    this.showMotionVectors && this.drawMotionVectors();
    this.showDering && this.drawDering();
    this.showMode && this.drawMode();
    this.showSkip && this.drawSkip();
    this.showSplit && this.drawSplit();
  }

  drawMode() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    var s = this.scale * this.ratio;
    var lineWidth = 3;
    var lineOffset = 0.5;
    ctx.lineWidth = lineWidth;
    ctx.strokeStyle = this.modeColor;
    ctx.globalAlpha = 0.5;
    function line(x, y, dx, dy) {
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + dx, y + dy);
      ctx.closePath();
      ctx.stroke();
    }

    function mode(m: AOMAnalyzerPredictionMode, x, y, dx, dy) {
      var hx = dx / 2;
      var hy = dy / 2;
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

    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_mi_mode(c, r);
        mode(i, c * 8 * s, r * 8 * s, 8 * s, 8 * s);
      }
    }

  }

  drawSkip() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    var s = this.scale * this.ratio;
    ctx.fillStyle = this.skipColor;
    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_mi_skip(c, r);
        if (i) {
          ctx.fillRect(c * 8 * s, r * 8 * s, 8 * s, 8 * s);
        }
      }
    }
  }

  drawSplit() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    ctx.strokeStyle = this.splitGridColor;
    var s = this.scale * this.ratio;
    ctx.globalAlpha = 1;

    var sizes = [
      [2, 2],
      [2, 3],
      [3, 2],
      [3, 3, "rgba(33, 33, 33, .02)"],
      [3, 4, "rgba(33, 33, 33, .04)"],
      [4, 3, "rgba(33, 33, 33, .06)"],
      [4, 4, "rgba(33, 33, 33, .08)"],
      [4, 5, "rgba(33, 33, 33, .10)"],
      [5, 4, "rgba(33, 33, 33, .12)"],
      [5, 5, "rgba(33, 33, 33, .14)"],
      [5, 6, "rgba(33, 33, 33, .16)"],
      [6, 5, "rgba(33, 33, 33, .18)"],
      [6, 6, "rgba(33, 33, 33, .20)"]
    ];

    var lineWidth = 1;
    var lineOffset = lineWidth / 2;
    ctx.lineWidth = lineWidth;

    function split(x, y, dx, dy) {
      ctx.beginPath();
      ctx.save();
      ctx.translate(lineOffset, lineOffset);
      ctx.moveTo(x, y);
      ctx.lineTo(x + dx, y);
      ctx.moveTo(x, y);
      ctx.lineTo(x, y + dy);
      ctx.restore();
      ctx.closePath();
      ctx.stroke();
    }

    // Draw block sizes above 8x8.
    for (var i = 3; i < sizes.length; i++) {
      var dc = 1 << (sizes[i][0] - 3);
      var dr = 1 << (sizes[i][1] - 3);
      for (var c = 0; c < cols; c += dc) {
        for (var r = 0; r < rows; r += dr) {
          var t = this.aom.get_mi_block_size(c, r);
          if (t == i) {
            split(c * s * 8, r * s * 8, dc * s * 8, dr * s * 8);
          }
        }
      }
    }

    // Draw block sizes below 8x8.
    for (var c = 0; c < cols; c ++) {
      for (var r = 0; r < rows; r ++) {
        var t = this.aom.get_mi_block_size(c, r);
        var x = c * s * 8;
        var y = r * s * 8;
        switch (t) {
          case 0:
            var dx = 4 * s;
            var dy = 4 * s;
            split(x,      y,      dx, dy);
            split(x + dx, y,      dx, dy);
            split(x,      y + dy, dx, dy);
            split(x + dx, y + dy, dx, dy);
            break;
          case 1:
            var dx = 4 * s;
            var dy = 8 * s;
            split(x,      y, dx, dy);
            split(x + dx, y, dx, dy);
            break;
          case 2:
            var dx = 8 * s;
            var dy = 4 * s;
            split(x, y,      dx, dy);
            split(x, y + dy, dx, dy);
            break;
        }
      }
    }
  }

  drawDering() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    ctx.strokeStyle = "rgba(33,33,33,0.75)";

    var s = this.scale * this.ratio;
    ctx.globalAlpha = 1;

    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_dering_gain(c, r);
        ctx.fillStyle = "rgba(33,33,33," + (i / 4) + ")";
        ctx.fillRect(c * 8 * s, r * 8 * s, 8 * s, 8 * s);
      }
    }
  }

  drawGrid() {
    var ctx = this.overlayContext;
    ctx.strokeStyle = this.gridColor;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    ctx.beginPath();
    var ratio = window.devicePixelRatio || 1;
    var scale = this.scale;
    var s = 8 * scale * ratio;

    var lineWidth = 3;
    var lineOffset = 1 / 2;
    for (var c = 0; c < cols + 1; c += 8) {
      var offset = lineOffset + c * s;
      ctx.moveTo(offset, 0);
      ctx.lineTo(offset, this.size.h * scale * ratio);
    }
    for (var r = 0; r < rows + 1; r += 8) {
      var offset = lineOffset + r * s;
      ctx.moveTo(0, offset);
      ctx.lineTo(this.size.w * scale * ratio, offset);
    }
    ctx.lineWidth = lineWidth;
    ctx.closePath();
    ctx.globalAlpha = 0.5;
    ctx.stroke();
  }

  drawVector(a: Vector, b: Vector) {
    var ctx = this.overlayContext;
    var c = b.clone().sub(a);
    var length = c.length();

    c.add(a);
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(c.x, c.y);
    ctx.closePath();
    ctx.stroke();
    return;
  }

  drawVector3(a: Vector, b: Vector) {
    var ctx = this.overlayContext;
    var c = b.clone().sub(a);
    var length = c.length();

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
    var ctx = this.overlayContext;
    ctx.save();
    var dist = Math.sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));

    ctx.beginPath();
    ctx.lineWidth = 1;
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();

    var angle = Math.acos((p2.y - p1.y) / dist);

    if (p2.x < p1.x) angle = 2 * Math.PI - angle;

    var size = 2 * Math.min(this.scale, 4);

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

  drawMotionVectors() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    var s = this.scale * this.ratio;
    ctx.globalAlpha = 0.5;


    var gradient = tinygradient([
      {color: tinycolor(this.mvColor).brighten(50), pos: 0},
      {color: tinycolor(this.mvColor), pos: 1}
    ]);

    var colorRange = gradient.rgb(32);

    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_mi_mv(c, r);
        var y = (i >> 16);
        var x = (((i & 0xFFFF) << 16) >> 16);
        if (x == 0 && y == 0) {
          continue;
        }
        var v = new Vector(x, y);
        v.clampLength(0, 31);
        var l = v.length() | 0;
        ctx.fillStyle = colorRange[l | 0];
        ctx.fillRect(c * 8 * s, r * 8 * s, 8 * s, 8 * s);

        // var offset = s * 8 / 2;
        // var a = new Vector(c * 8 * s + offset, r * 8 * s + offset);
        // var v = new Vector(x, y).divideScalar(8).multiplyScalar(s);
        // var l = v.length() | 0;
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
  var rTmp = yValue + (1.370705 * (vValue - 128));
  var gTmp = yValue - (0.698001 * (vValue - 128)) - (0.337633 * (uValue - 128));
  var bTmp = yValue + (1.732446 * (uValue - 128));
  var r = clamp(rTmp | 0, 0, 255) | 0;
  var g = clamp(gTmp | 0, 0, 255) | 0;
  var b = clamp(bTmp | 0, 0, 255) | 0;
  return (b << 16) | (g << 8) | (r << 0);
}

function getUrlParameters(): any {
  var url = window.location.search.substring(1);
  url = url.replace(/\/$/, ""); // Replace / at the end that gets inserted by browsers.
  var params = {};
  url.split('&').forEach(function (s) {
    var t = s.split('=');
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
    var keys = str.split('-');
    var isOSX = /Mac OS X/.test($window.navigator.userAgent);
    var seperator = (!isOSX || keys.length > 2) ? '+' : '';
    var abbreviations = {
      M: isOSX ? '⌘' : 'Ctrl',
      A: isOSX ? 'Option' : 'Alt',
      S: 'Shift'
    };
    return keys.map(function(key, index) {
      var last = index == keys.length - 1;
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