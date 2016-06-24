if [ ! -d "asm" ]; then
  # Control will enter here if $DIRECTORY doesn't exist.
  mkdir asm
  cd asm && emconfigure ../configure --disable-multithread --disable-runtime-cpu-detect --target=generic-gnu --enable-experimental --enable-dering
fi

cd asm
emmake make
cp ../sc.ivf .
cp examples/emscripten_decoder examples/emscripten_decoder.bc
# emcc examples/emscripten_decoder.bc -o decoder.html --embed-file sc.ivf
emcc -O3 examples/emscripten_decoder.bc -o examples/decoder.js -s TOTAL_MEMORY=134217728
cd ..
cp asm/examples/decoder.js ins/bin/decoder.js
cp asm/examples/decoder.js.mem ins/bin/decoder.js.mem