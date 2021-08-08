#
# SPIM S20 MIPS Simulator.
# Makefile for SPIM.
#
# SPIM is covered by a BSD license.
#
# Copyright (c) 1990-2020, James R. Larus.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
#
# Neither the name of the James R. Larus nor the names of its contributors may be
# used to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# To make spim, type:
#
#   make spim
#
# To verify spim works, type:
#
#   make test
#

.SUFFIXES:
.SUFFIXES: .cpp .o

#
# The following parameters must be set for the target machine:
#
#

# Path for directory that contains the source of the CPU code:
CPU_DIR = ../CPU
VPATH = src:$(CPU_DIR)

# Path of directory that contains SPIM tests:
TEST_DIR = ../Tests

# Path of directory that contains documentation:
DOC_DIR = ../Documentation



# Full path for the parent directory of all installed files:
PREFIX = .

# Full path for the directory that will hold the executable files:
BIN_DIR = $(PREFIX)/bin

# Full path for the directory that will hold the exception handler:
EXCEPTION_DIR = $(PREFIX)/res/exception

# Full path for the directory that will hold the man files:
MAN_DIR = $(PREFIX)/share/man/man1



# SPIM needs flex's -I flag since the scanner is used interactively.
# You can set the -8 flag so that funny characters do not hang the scanner.
MYLEX = flex
LEXFLAGS += -I -8 -o lex.yy.cpp

YACC=bison
YFLAGS += -d --defines=parser_yacc.h --output=parser_yacc.cpp -p yy


# Size of the segments when spim starts up (data segment must be >= 64K).
# (These sizes are fine for most users since SPIM dynamically expands
# the memory as necessary.)
MEM_SIZES = -DTEXT_SIZE=65536 -DDATA_SIZE=131072 -DK_TEXT_SIZE=65536


#
# End of parameters
#



DEFINES = $(MEM_SIZES) -DDEFAULT_EXCEPTION_HANDLER="\"$(EXCEPTION_DIR)/exceptions.s\""

# Uncomment to test the functions for encoding/decoding binary instructions:
#DEFINES += -DTEST_ASM

CXX = g++
CXXFLAGS += -I. -I$(CPU_DIR) $(DEFINES) -O -g -Wall -pedantic -Wextra -Wunused -Wno-write-strings -x c++
YCFLAGS +=
LDFLAGS += -lm -lncursesw
CSH = bash

# lex.yy.cpp is usually compiled with -O to speed it up.

LEXCFLAGS += -O $(CXXFLAGS)

OBJS = spimcurses.o simpane.o spim-utils.o run.o mem.o inst.o data.o sym-tbl.o parser_yacc.o lex.yy.o syscall.o display-utils.o string-stream.o

spim: $(OBJS)
	$(CXX) -g $(OBJS) $(LDFLAGS) -o $@


TAGS:	*.cpp *.h *.l *.y
	etags *.l *.y *.cpp *.h


clean:
	rm -f spim spim.exe *.o TAGS test.out lex.yy.cpp parser_yacc.cpp parser_yacc.h y.output

install: spim
	install -d $(DESTDIR)$(BIN_DIR)
	install spim $(DESTDIR)$(BIN_DIR)/spim
	install -d $(DESTDIR)$(EXCEPTION_DIR)
	install -m 0444 $(CPU_DIR)/exceptions.s $(DESTDIR)$(EXCEPTION_DIR)/exceptions.s

install-man:
	install -d $(DESTDIR)$(MAN_DIR)
	install -m 0444 $(DOC_DIR)/spim.man $(DESTDIR)$(MAN_DIR)/spim.1

splint: spim
	splint -weak -preproc -warnposix +matchanyintegral spim.cpp parser_yacc.cpp lex.yy.cpp


.PHONY: test test_bare clean install install-man splint

#
# Dependences not handled well by makedepend:
#

parser_yacc.h: parser_yacc.cpp

parser_yacc.cpp: $(CPU_DIR)/parser.y
	$(YACC) $(YFLAGS) $(CPU_DIR)/parser.y

parser_yacc.o: parser_yacc.cpp
	$(CXX) $(CXXFLAGS) $(YCFLAGS) -c parser_yacc.cpp


lex.yy.cpp: $(CPU_DIR)/scanner.l
	$(MYLEX) $(LEXFLAGS) $(CPU_DIR)/scanner.l

lex.yy.o: lex.yy.cpp
	$(CXX) $(LEXCFLAGS) -c lex.yy.cpp

data.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/parser.h $(CPU_DIR)/run.h $(CPU_DIR)/data.h

display-utils.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/data.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/run.h $(CPU_DIR)/sym-tbl.h

dump_ops.o: $(CPU_DIR)/op.h

inst.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/parser.h $(CPU_DIR)/scanner.h parser_yacc.h $(CPU_DIR)/data.h $(CPU_DIR)/op.h

mem.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h


run.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/sym-tbl.h parser_yacc.h $(CPU_DIR)/syscall.h $(CPU_DIR)/run.h

spim-utils.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/data.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/scanner.h $(CPU_DIR)/parser.h parser_yacc.h $(CPU_DIR)/run.h $(CPU_DIR)/sym-tbl.h

string-stream.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h
sym-tbl.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/data.h $(CPU_DIR)/parser.h $(CPU_DIR)/sym-tbl.h parser_yacc.h

syscall.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/syscall.h

lex.yy.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/parser.h $(CPU_DIR)/scanner.h parser_yacc.h $(CPU_DIR)/op.h

simpane.o: curses/simpane.cpp curses/simpane.h
	$(CXX) $(CXXFLAGS) $(YCFLAGS) -c $<

spimcurses.o: $(CPU_DIR)/spim.h curses/simpane.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/parser.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/scanner.h parser_yacc.h

parser_yacc.o: $(CPU_DIR)/spim.h $(CPU_DIR)/string-stream.h $(CPU_DIR)/spim-utils.h $(CPU_DIR)/inst.h $(CPU_DIR)/reg.h $(CPU_DIR)/mem.h $(CPU_DIR)/sym-tbl.h $(CPU_DIR)/data.h $(CPU_DIR)/scanner.h $(CPU_DIR)/parser.h