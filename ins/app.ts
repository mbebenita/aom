declare var angular: any;

interface AOMInternal {
  _read_frame (): number;
  _get_plane (pli: number): number;
  _get_plane_stride (pli: number): number;
  _get_plane_width (pli: number): number;
  _get_plane_height (pli: number): number;
  _get_mi_rows(): number;
  _get_mi_cols(): number;
  _get_mi_mv(c: number, r: number): number;
  _get_dering_gain(c: number, r: number): number;
  _get_frame_count(): number;
  HEAPU8: Uint8Array;
}

class AOM {
  native: AOMInternal = (<any>window).Module;
  HEAPU8: Uint8Array = this.native.HEAPU8;
  constructor () {

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
  get_dering_gain(c: number, r: number): number {
    return this.native._get_dering_gain(c, r);
  }
  get_frame_count(): number {
    return this.native._get_frame_count();
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
  w: number = 128;
  h: number = 128;

  ratio: number = 1;
  scale: number = 1;


  showGrid: boolean = true;
  showDering: boolean = true;
  showImage: boolean = true;
  showMotionVectors: boolean = true;

  frameNumber: number = 0;

  get isPlaying() {
    return !!this.playInterval;
  }

  playInterval: number = 0;

  container: HTMLDivElement;
  displayCanvas: HTMLCanvasElement;
  overlayCanvas: HTMLCanvasElement;
  displayContext: CanvasRenderingContext2D = null;
  overlayContext: CanvasRenderingContext2D = null;
  imageData: ImageData = null;

  frameCanvas: HTMLCanvasElement;
  frameContext: CanvasRenderingContext2D = null;

  $scope: any;
  $interval: any;

  constructor($scope, $interval) {
    this.$scope = $scope;
    this.$interval = $interval;
    this.ratio = window.devicePixelRatio || 1;
    this.scale = this.ratio;
    this.scale *= 2;

    this.displayCanvas = <HTMLCanvasElement>document.getElementById("display");
    this.displayContext = this.displayCanvas.getContext("2d");

    this.overlayCanvas = <HTMLCanvasElement>document.getElementById("overlay");
    this.overlayContext = this.overlayCanvas.getContext("2d");

    this.frameCanvas = document.createElement("canvas");
    this.frameContext = this.frameCanvas.getContext("2d");

    this.aom = new AOM();
    this.nextFrame();

    var parameters = getUrlParameters();
    if (parameters.frameNumber) {
      var frameNumber = parseInt(parameters.frameNumber);
      var self = this;
      this.$interval(function () {
        self.nextFrame();
      }, 1, frameNumber - 1);
    }

    ["showGrid", "showDering", "showImage", "showMotionVectors"].forEach(x => {
      if (x in parameters) {
        this[x] = parameters[x] == "true";
      }
    });

    this.change();
  }

  change() {
    this.setScale(this.scale);
    this.showFrame();
  }

  setScale(scale: number) {
    this.scale = scale;

    this.frameCanvas.width = this.w;
		this.frameCanvas.height = this.h;
    this.imageData = this.frameContext.createImageData(this.w, this.h);


    this.displayCanvas.style.width = (this.w * this.scale) + "px";
		this.displayCanvas.style.height = (this.h * this.scale) + "px";
    this.displayCanvas.width = this.w * this.scale * this.ratio;
		this.displayCanvas.height = this.h * this.scale * this.ratio;

    this.overlayCanvas.style.width = (this.w * this.scale) + "px";
		this.overlayCanvas.style.height = (this.h * this.scale) + "px";
    this.overlayCanvas.width = this.w * this.scale * this.ratio;
		this.overlayCanvas.height = this.h * this.scale * this.ratio;
  }

  playPause() {
    var self = this;
    if (this.playInterval) {
      this.$interval.cancel(this.playInterval);
      this.playInterval = 0;
      return;
    }
    this.playInterval = this.$interval(function () {
      self.nextFrame();
    }, 30);
  }

  nextFrame() {
    if (this.aom.read_frame()) {
      return false;
    }
    this.showFrame();
    this.frameNumber ++;
  }

  showFrame() {
    if (this.frameNumber === 0) {
      this.w = this.aom.get_plane_width(0);
		  this.h = this.aom.get_plane_height(0);
      this.setScale(this.scale);
    }
    var Yp = this.aom.get_plane(0);
    var Ys = this.aom.get_plane_stride(0);
    var Up = this.aom.get_plane(1);
    var Us = this.aom.get_plane_stride(1);
    var Vp = this.aom.get_plane(2);
    var Vs = this.aom.get_plane_stride(2);

    var H = this.aom.HEAPU8;
    var I = this.imageData.data;

    var w = this.w;
    var h = this.h;

    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var index = (y * w + x) * 4;

        var Y = H[Yp + y*Ys + x];
        var U = H[Up + (y>>1)*Us + (x>>1)];
        var V = H[Vp + (y>>1)*Vs + (x>>1)];

        var bgr = yuv2rgb(Y, U, V);

        var r = (bgr >>  0) & 0xFF;
        var g = (bgr >>  8) & 0xFF;
        var b = (bgr >> 16) & 0xFF;

        I[index + 0] = r; // r
        I[index + 1] = g; // g
        I[index + 2] = b; // b
        I[index + 3] = 255;
      }
    }

    if (this.showImage && this.imageData) {
      this.frameContext.putImageData(this.imageData, 0, 0);
      this.displayContext.mozImageSmoothingEnabled = false;
      this.displayContext.imageSmoothingEnabled = false;
      this.displayContext.drawImage(this.frameCanvas, 0, 0, this.w * this.scale * this.ratio, this.h * this.scale * this.ratio);
    }

    this.draw();
  }

  draw() {
    var ctx = this.overlayContext;
    var ratio = window.devicePixelRatio || 1;

    ctx.clearRect(0, 0, this.w * this.scale * ratio, this.h * this.scale * ratio);

    this.showGrid && this.drawGrid();
    this.showMotionVectors && this.drawMotionVectors();
    this.showDering && this.drawDering();
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
      ctx.lineTo(offset, this.h * scale * ratio);
    }
    for (var r = 0; r < rows + 1; r++) {
      var offset = lineOffset + r * s;
      ctx.moveTo(0, offset);
      ctx.lineTo(this.w * scale * ratio, offset);
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

function yuv2rgb(yValue, uValue, vValue) {
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

angular.module('AomInspectorApp', ['ngMaterial'])
.controller('AppCtrl', ['$scope', '$interval', AppCtrl])
.config(['$mdIconProvider', function($mdIconProvider) {
    $mdIconProvider
      // .iconSet('social', 'img/icons/sets/social-icons.svg', 24)
      .defaultIconSet('img/icons/sets/core-icons.svg', 24);
}]);

window.Module = {
  noExitRuntime: true,
  preRun: [],
  postRun: [function() {
    // startVideo();
  }],
  arguments: ['sc.ivf', 'soccer.out']
};

// var script = document.createElement('script');
// script.src = "decoder.js";
// document.body.appendChild(script);