CFLAGS= -g -O0

all: filesrv filecli tstls

filesrv: filesrv.cpp
	g++ $(CFLAGS) -o filesrv filesrv.cpp

filecli: filecli.cpp
	g++ $(CFLAGS) -o filecli filecli.cpp

tstls: tstls.cpp
	g++ $(CFLAGS) -o tstls tstls.cpp

clean:
	rm -f *.o filesrv filecli tstls
