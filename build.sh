#!/bin/sh

CXXFLAGS="-Wall -I ./src/ -Wno-unused -Wno-deprecated  -Wno-write-strings -Wno-free-nonheap-object"

bison -d -v -y -b cool --debug -p cool_yy -o obj/cool-bison-parser.cc src/cool.bison
flex -d -o obj/cool-flex-lexer.cc src/cool.flex

g++ $CXXFLAGS -c src/sem-phase.cc -o obj/sem-phase.o
g++ $CXXFLAGS -c src/utilities.cc -o obj/utilities.o
g++ $CXXFLAGS -c src/stringtab.cc -o obj/stringtab.o
g++ $CXXFLAGS -c src/cool-tree.cc -o obj/cool-tree.o
g++ $CXXFLAGS -c obj/cool-flex-lexer.cc -o obj/cool-flex-lexer.o
g++ $CXXFLAGS -c obj/cool-bison-parser.cc -o obj/cool-bison-parser.o
g++ $LDFLAGS $CXXFLAGS obj/sem-phase.o obj/utilities.o obj/stringtab.o obj/cool-tree.o obj/cool-flex-lexer.o obj/cool-bison-parser.o -o semanticanalyzer
