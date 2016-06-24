declare var angular: any;
declare var FS: any;
declare var Mousetrap: any;

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

  showGrid: boolean = false;
  showMotionVectors: boolean = false;
  showImage: boolean = true;
  showDering: boolean = false;
  showMode: boolean = false;

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

  displayContext: CanvasRenderingContext2D = null;
  overlayContext: CanvasRenderingContext2D = null;
  imageData: ImageData = null;

  frameCanvas: HTMLCanvasElement;
  frameContext: CanvasRenderingContext2D = null;

  lastDecodeFrameTime: number = 0;

  predictionModeNames: string [] = [];
  predictionModeColors: string [] = colors;

  $scope: any;
  $interval: any;

  constructor($scope, $interval) {
    this.$scope = $scope;
    this.$interval = $interval;
    this.ratio = window.devicePixelRatio || 1;
    this.scale = this.ratio;
    this.scale = 1;

    this.container = <HTMLDivElement>document.getElementById("container");

    this.displayCanvas = <HTMLCanvasElement>document.getElementById("display");
    this.displayContext = this.displayCanvas.getContext("2d");

    this.overlayCanvas = <HTMLCanvasElement>document.getElementById("overlay");
    this.overlayContext = this.overlayCanvas.getContext("2d");

    this.frameCanvas = document.createElement("canvas");
    this.frameContext = this.frameCanvas.getContext("2d");

    var parameters = getUrlParameters();
    ["showGrid", "showDering", "showImage", "showMotionVectors", "showMode"].forEach(x => {
      if (x in parameters) {
        this[x] = parameters[x] == "true";
      }
    });
    var frames = parseInt(parameters.frameNumber) || 1;

    this.aom = new AOM();
    var file = "media/Crosswalk.ivf";
    this.openFile(file, () => {
      this.playFrameAsync(frames, () => {
        this.drawFrame();
      })
    });

    this.installKeyboardShortcuts();
    this.initPredictionModeColors();
  }

  initPredictionModeColors() {
    for (var k in AOMAnalyzerPredictionMode) {
      if (isNaN(parseInt(k))) {
        this.predictionModeNames.push(k);
      }
    }
  }

  installKeyboardShortcuts() {
    Mousetrap.bind(['ctrl+right'], this.uiNextFrame.bind(this));
    Mousetrap.bind(['space'], this.uiPlayPause.bind(this));

    Mousetrap.bind([']'], () => {
      this.scale *= 2;
      this.resetCanvases();
      this.drawFrame();
      this.uiApply();
    });

    Mousetrap.bind(['['], () => {
      this.scale /= 2;
      this.resetCanvases();
      this.drawFrame();
      this.uiApply();
    });

    var self = this;
    function toggle(name) {
      self[name] = !self[name];
      self.drawFrame();
      self.uiApply();
    }

    Mousetrap.bind(['1'], toggle.bind(this, "showGrid"));
    Mousetrap.bind(['2'], toggle.bind(this, "showMotionVectors"));
    Mousetrap.bind(['3'], toggle.bind(this, "showImage"));
    Mousetrap.bind(['4'], toggle.bind(this, "showDering"));
    Mousetrap.bind(['5'], toggle.bind(this, "showMode"));
  }

  openFile(path: string, next: () => any = null) {
    this.downloadFile(path, (buffer: Uint8Array) => {
      this.fileSize = buffer.length;
      FS.writeFile("/tmp/input.ivf", buffer, { encoding: "binary" });
      this.aom.open_file();
      this.size = this.aom.getFrameSize();
      this.resetCanvases();
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
      case "open":
        file = "media/soccer_cif_dering.ivf";
        break;
      case "open-0":
        file = "media/tiger.ivf";
        break;
      case "open-1":
        file = "media/Crosswalk.ivf";
        break;
    }
    this.openFile(file, () => {
      this.playFrameAsync(1, () => {
        this.drawFrame();
      })
    });
  }

  uiViewChange() {
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

  uiApply() {
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
    this.uiApply();
  }

  drawFrame() {
    this.clearImage();
    this.drawImage();
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

    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var index = (y * w + x) * 4;

        var Y = H[Yp + y * Ys + x];
        var U = H[Up + (y >> 1) * Us + (x >> 1)];
        var V = H[Vp + (y >> 1) * Vs + (x >> 1)];

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

    if (this.showImage && this.imageData) {
      this.frameContext.putImageData(this.imageData, 0, 0);
      this.displayContext.mozImageSmoothingEnabled = false;
      this.displayContext.imageSmoothingEnabled = false;
      this.displayContext.drawImage(this.frameCanvas, 0, 0, this.size.w * this.scale * this.ratio, this.size.h * this.scale * this.ratio);
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
  }

  drawMode() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    ctx.strokeStyle = "rgba(33,33,33,0.75)";

    var s = this.scale * this.ratio;
    ctx.globalAlpha = 0.5;

    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_mi_mode(c, r);
        // ctx.fillStyle = "rgba(33,33,33," + (i / 13) + ")";
        ctx.fillStyle = colors[i];
        ctx.fillRect(c * 8 * s, r * 8 * s, 8 * s, 8 * s);
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
    ctx.strokeStyle = "rgba(33,33,33,0.75)";
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    ctx.beginPath();
    var ratio = window.devicePixelRatio || 1;
    var scale = this.scale;
    var s = 8 * scale * ratio;

    var lineWidth = 1;
    var lineOffset = lineWidth / 2;
    for (var c = 0; c < cols + 1; c++) {
      var offset = lineOffset + c * s;
      ctx.moveTo(offset, 0);
      ctx.lineTo(offset, this.size.h * scale * ratio);
    }
    for (var r = 0; r < rows + 1; r++) {
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

    var size = 2 * Math.min(this.scale, 2);

    ctx.beginPath();
    ctx.translate(p2.x, p2.y);
    ctx.rotate(-angle);
    ctx.lineWidth = 1;
    ctx.moveTo(0, -size);
    ctx.lineTo(-size, -size);
    ctx.lineTo(0, size * 2);
    ctx.lineTo(size, -size);
    ctx.lineTo(0, -size);
    ctx.closePath();
    ctx.fill();
    ctx.restore();
  }

  drawMotionVectors() {
    var ctx = this.overlayContext;
    var cols = this.aom.get_mi_cols();
    var rows = this.aom.get_mi_rows();
    var s = this.scale * this.ratio;
    ctx.globalAlpha = 1;

    for (var c = 0; c < cols; c++) {
      for (var r = 0; r < rows; r++) {
        var i = this.aom.get_mi_mv(c, r);
        var y = (i >> 16);
        var x = (((i & 0xFFFF) << 16) >> 16);
        if (x == 0 && y == 0) {
          continue;
        }
        var offset = s * 8 / 2;
        var a = new Vector(c * 8 * s + offset, r * 8 * s + offset);
        var v = new Vector(x, y).divideScalar(8).multiplyScalar(s);
        var l = v.length();
        v.clampLength(4, Infinity);
        // v.clampLength(4, s * 8);
        // ctx.lineWidth = Math.max(1, l / 32);
        ctx.fillStyle = ctx.strokeStyle = "rgba(33,33,33," + (l / 32) + ")";
        this.drawVector2(
          a.clone().add(v),
          a
        )
      }
    }
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
  var r = clamp(rTmp, 0, 255) | 0;
  var g = clamp(gTmp, 0, 255) | 0;
  var b = clamp(bTmp, 0, 255) | 0;
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
.module('AomInspectorApp', ['ngMaterial'])
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

// var script = document.createElement('script');
// script.src = "decoder.js";
// document.body.appendChild(script);