SRC = kslog

all: lkm cli

lkm:
	gcc -D_KERNEL -I/sys -c $(SRC).c

cli:
	cd kslog_cli; make; cd ..

load:
	sudo modload	-o $(SRC) -ehandler $(SRC).o

unload:
	sudo modunload -n $(SRC)

clean:
	rm -rf $(SRC) $(SRC).o; cd kslog_cli; make clean; cd ..
