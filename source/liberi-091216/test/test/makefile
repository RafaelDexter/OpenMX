#-----------------------------------------------------------------------
# Makefile for LIBERI (20090723)
#-----------------------------------------------------------------------
.PHONY: compile clean rebuild demo selfcheck

SRCDIR=../../source
INSTALL_DIR=.


#-----------------------------------------------------------------------
# CONFIGURAGION

# cygwin 
#CC   = gcc
#OPT  = -O3 -w -static 
#FFTW3_INCLUDE_DIR = $(HOME)/local/include
#LIB  = -lm -g2c -L$(HOME)/local/lib -lfftw3 -lblas -llapack

# Linux , gcc 4.3.2 
#CC   = gcc
#LOADER = gfortran
#OPT  = -O3 -w -static 
#FFTW3_INCLUDE_DIR = $(HOME)/local/include
#LIB  = -lm -g2c -L$(HOME)/local/lib -lfftw3 -lblas -llapack

# eniac2 cluster in RCIS (Linux, Intel Xeon, icc 10.0, Intel MKL 10.0)
CC   = icc
OPT  = -O3 -static -w -no-multibyte-chars -openmp -mp
FFTW3_INCLUDE_DIR = /opt/intel/mkl/10.0.2.018/include/fftw
LIB  = -lm -lfftw3 -lmkl_lapack


# chopin2 cluster in RCIS (Linux, Intel Xeon, icc 10.0, Intel MKL 10.0)
#CC   = icc
#OPT  = -O3 -static -w -no-multibyte-chars -openmp -mp
#FFTW3_INCLUDE_DIR = /opt/intel/mkl/10.0.2.018/include/fftw
#LIB  = -lm -lfftw3 -L/opt/intel/mkl/10.0.2.018/lib/em64t -lmkl_lapack


# abacus2 cluster in RCIS (Linux, AMD Opteron, pgcc)
#CC   = pgcc
#OPT  = -O3 -w -Bstatic
#FFTW3_INCLUDE_DIR = /usr/local/fftw3/include 
#LIB  = -lm -lf2c -L/usr/local/fftw3/lib -lfftw3 -llapack -lpgftnrtl -pgf77libs


# Clay XT-5
#CC   = cc
#OPT  = -O3 -Bstatic -w


# SGI Altix 4700 
#CC   = icc
#OPT  = -O3 -static -w
#FFTW3_INCLUDE_DIR = /opt/fftw/include
#LIB = -f2c -L/opt/fftw/lib -lfftw3 -lmkl_intel_lp64 -lmkl_lapack -lguide -pthread


FLAG = $(OPT) -DNDEBUG

ifdef FFTW3_INCLUDE_DIR
  FLAG += -I$(FFTW3_INCLUDE_DIR)
endif

ifndef LOADER
  LOADER=$(CC)
endif


FLAG += -I$(SRCDIR) 



#----- FILES -----
EXENAME = ./test_pp

LIBOBJ = \
  $(SRCDIR)/eri.o  \
  $(SRCDIR)/eri_ll.o  \
  $(SRCDIR)/eri_sf.o  \
  $(SRCDIR)/eri_interpolate.o  \
  $(SRCDIR)/eri_gtbl.o \
  $(SRCDIR)/sbt/eri_sbt.o \
  $(SRCDIR)/sbt/log/eri_fsbt.o \
  $(SRCDIR)/sbt/log/eri_logfsbt.o \
  $(SRCDIR)/sbt/linear/eri_linfsbt.o \
  
OBJ= ./test.o

HEADERS = $(SRCDIR)/eri.h 


#----- MAKE COMMANDS -----
compile:
	make $(EXENAME)

clean:
	rm -f $(LIBNAME) $(LIBOBJ) $(OBJ1) $(OBJ2) $(TEST1NAME) $(TEST2NAME)

rebuild:
	make clean
	make

$(EXENAME): $(LIBOBJ) $(OBJ)
	$(LOADER) $(FLAG) -o $(EXENAME) $(LIBOBJ) $(OBJ) $(LIB)

.c.o:
	$(CC) $(FLAG) -c $< -o $@

