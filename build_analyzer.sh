echo Building Analyzer
if [ ! -d "asm" ]; then
  echo Configuring Analyzer
  mkdir asm
  cd asm && emconfigure ../configure --disable-multithread --disable-runtime-cpu-detect --target=generic-gnu --enable-accounting --enable-analyzer --enable-aom_highbitdepth --extra-cflags="-D_POSIX_SOURCE"
fi

cd asm
emmake make -j 8
cp aomanalyzer aomanalyzer.bc
emcc -O3 aomanalyzer.bc -o aomanalyzer.js -s TOTAL_MEMORY=134217728 -s MODULARIZE=1 -s EXPORT_NAME="'DecoderModule'" --post-js "../aomanalyzer-post.js" --memory-init-file 0
cp aomanalyzer.js ../aomanalyzer.js
