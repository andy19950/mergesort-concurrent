reset
set ylabel 'time(sec)'
set xlabel 'number of threads'
set style fill solid
set title 'perfomance comparison'
set term png enhanced font 'Verdana,10'
set output 'runtime.png'

plot [:][:] 'time.txt' using 2:xtic(1) with histogram title ' ', \
'' using ($0+0.06):($2+0.5):2 with labels title ' ', \
