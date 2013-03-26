ENVIRON = linux-gcc-x86
DFLAG = -g
WFLAG = -Wall
C11FLAG = -std=c++0x
THREADFLAG = -pthread

SRCF = PracticalSocket.cpp\
			 smain.cpp\
			 dblru.cpp\
			 ssock.cpp\
			 scmd.cpp
INCF = sinc.h\
			 sconst.h\
			 dblru.h\
			 PracticalSocket.h
OBJF = $(SRCF:.cpp=.o)

SRCDIR = src
OBJDIR = obj
INCDIR = inc
MVOBJ = mv -f *.o obj/

SRC = $(patsubst %,$(SRCDIR)/%,$(SRCF))
OBJ = $(patsubst %,$(OBJDIR)/%,$(OBJF))
INC = $(patsubst %,$(INCDIR)/%,$(INCF))

CREATEDIR = mkdir -p obj bin

all: hoard memstashed

hoard: hoard/src/libhoard.so

hoard/src/libhoard.so:
	make -C hoard/src $(ENVIRON)

test:
	./libmem.sh
	g++ -o tests/cli tests/genclient.cpp

memstashed: $(OBJ)
	$(CREATEDIR)
	g++ -o bin/memstashed $(WFLAG) $(OBJ) -lm $(THREADFLAG) $(HOARD)

obj/PracticalSocket.o: src/PracticalSocket.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/PracticalSocket.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/PracticalSocket.o

obj/smain.o: src/smain.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/smain.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/smain.o

obj/dblru.o: src/dblru.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/dblru.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/dblru.o

obj/ssock.o: src/ssock.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/ssock.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/ssock.o

obj/scmd.o: src/scmd.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/scmd.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/scmd.o

clean:
	rm -rf bin/*
	rm -rf obj/*
