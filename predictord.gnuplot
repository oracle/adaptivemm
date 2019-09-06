# If predictord is run with
#
#	-o output -z 1,Normal
#
# then the corresponding output files may be read by gnuplot (version 5),
# loading this script file with the commands
#
#	basename='output.1,Normal'
#	load 'predictord.gnuplot'
#
o=basename.'.observations'
p=basename.'.predictions'

set xl 'Time/s'
set yl 'Free memory/pages'
set tit basename

plot \
o u 1:2 w l lc 1 lw 1 t 'Total', \
for[c=12:3:-1] o u 1:c w l lc (c-2) lw 1 t 'Order '.(c-2), \
\
p u 2:3:4:5 w vec nohead lc 1 dt 1 lw 2 noti, \
p u 2:6:7:8 w vec nohead lc 1 dt 2 lw 2 noti, \
p u 2:9:10:11:1 w vec nohead lc var dt 1 lw 2 noti, \
p u 12:13:14:15:1 w vec nohead lc var dt 2 lw 2 noti
