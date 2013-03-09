DFLAG = -g
WFLAG = -Wall
C11FLAG = -std=c++0x
THREADFLAG = -pthread

SRCF = PracticalSocket.cc
INCF = sinc.h\
			 sconst.h\
			 PracticalSocket.h
OBJF = $(SRCF:.cc=.o)

SRCDIR = src
OBJDIR = obj
INCDIR = inc
MVOBJ = mv -f *.o obj/

SRC = $(patsubst %,$(SRCDIR)/%,$(SRCF))
OBJ = $(patsubst %,$(OBJDIR)/%,$(OBJF))
INC = $(patsubst %,$(INCDIR)/%,$(INCF))

CREATEDIR = mkdir -p obj bin

all: memstashed

memstashed: $(OBJ) obj/smain.o
	$(CREATEDIR)
	g++ -o bin/memstashed $(WFLAG) $(OBJ) obj/smain.o -lm $(THREADFLAG)

obj/PracticalSocket.o: src/PracticalSocket.cc $(INC)
	$(CREATEDIR)
	g++ -c src/PracticalSocket.cc -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/PracticalSocket.o

obj/smain.o: src/smain.cpp $(INC)
	$(CREATEDIR)
	g++ -c src/smain.cpp -I inc $(C11FLAG) $(WFLAG) $(DFLAG) -o obj/smain.o

clean:
	rm -rf bin/*
	rm -rf obj/*
