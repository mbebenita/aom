echo Building Analyzer
if [ ! -d "asm" ]; then
  echo Configuring Analyzer
  mkdir asm
  cd asm && emconfigure ../configure --disable-multithread --disable-runtime-cpu-detect --target=generic-gnu --enable-experimental --enable-dering
fi

cd asm
emmake make
cp examples/emscripten_decoder examples/emscripten_decoder.bc
emcc -O3 examples/emscripten_decoder.bc -o examples/decoder.js -s TOTAL_MEMORY=134217728
cp examples/decoder.js ../ins/bin/decoder.js
cp examples/decoder.js.mem ../ins/bin/decoder.js.mem
echo Analyzer is ready, serve it from the ins directory using your favorite web server. E.g. 'cd ins && python -m SimpleHTTPServer'