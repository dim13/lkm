#
# prochide makefile
#
# mike@gravitino.net
#
# 9-21-01
#

all: module cli
	
module:
	gcc -D_KERNEL -I/sys -c prochide.c

load:
	modload	-o prochide -eprochide_handler prochide.o

unload:
	modunload -n prochide

cli:
	gcc -o ph ph_cli.c

clean:
	rm -rf prochide prochide.o ph
