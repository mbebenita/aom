var AccountingSymbol = (function () {
    function AccountingSymbol(name, bits, samples, x, y) {
        this.name = name;
        this.bits = bits;
        this.samples = samples;
        this.x = x;
        this.y = y;
        // ...
    }
    return AccountingSymbol;
}());
var Accounting = (function () {
    function Accounting(symbols) {
        if (symbols === void 0) { symbols = []; }
        this.symbols = null;
        this.frameSymbols = null;
        this.symbols = symbols;
    }
    Accounting.prototype.createFrameSymbols = function () {
        if (this.frameSymbols) {
            return this.frameSymbols;
        }
        this.frameSymbols = Object.create(null);
        this.frameSymbols = Accounting.flatten(this.symbols);
    };
    Accounting.prototype.createBlockSymbols = function (mi) {
        return Accounting.flatten(this.symbols.filter(function (symbol) {
            return symbol.x === mi.x && symbol.y === mi.y;
        }));
    };
    Accounting.flatten = function (sybmols) {
        var map = Object.create(null);
        sybmols.forEach(function (symbol) {
            var s = map[symbol.name];
            if (!s) {
                s = map[symbol.name] = new AccountingSymbol(symbol.name, 0, 0, symbol.x, symbol.y);
            }
            s.bits += symbol.bits;
            s.samples += symbol.samples;
        });
        var ret = Object.create(null);
        var names = [];
        for (var name_1 in map)
            names.push(name_1);
        // Sort by bits.
        names.sort(function (a, b) { return map[b].bits - map[a].bits; });
        names.forEach(function (name) {
            ret[name] = map[name];
        });
        return ret;
    };
    Accounting.makeTraces = function (accountings) {
        var allFrameSymbols = {};
        var x = [];
        function makeEmptyArray(n) {
            var a = [];
            for (var i = 0; i < n; i++)
                a[i] = 0;
            return a;
        }
        var count = Math.max(32, accountings.length);
        for (var i = 0; i < count; i++) {
            x.push(i);
        }
        for (var i = 0; i < accountings.length; i++) {
            var frameSymbols = accountings[i].createFrameSymbols();
            var totalBits = 0;
            for (var symbolName in frameSymbols) {
                totalBits += frameSymbols[symbolName].bits;
            }
            for (var symbolName in frameSymbols) {
                if (!allFrameSymbols[symbolName]) {
                    allFrameSymbols[symbolName] = makeEmptyArray(count);
                }
                allFrameSymbols[symbolName][i] = frameSymbols[symbolName].bits / totalBits;
            }
        }
        // Sort to make Plotly happy.
        var names = [];
        for (var symbolName in allFrameSymbols)
            names.push(symbolName);
        names.sort();
        var traces = [];
        names.forEach(function (symbolName) {
            var y = allFrameSymbols[symbolName];
            traces.push({
                x: x,
                y: y,
                name: symbolName,
                type: "bar",
                marker: {
                    color: getColor(symbolName)
                }
            });
        });
        return traces;
    };
    return Accounting;
}());
//# sourceMappingURL=accounting.js.map