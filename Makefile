# PMCOI — modelo exato com Gurobi
#
#   make          compila bin/pmcoi
#   make clean    remove build/ e bin/
#
# Para outro Gurobi: make GUROBI_HOME=/caminho/para/gurobi

GUROBI_HOME ?= /Library/gurobi1203/macos_universal2

CXX      := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -I$(GUROBI_HOME)/include
LDFLAGS  := -L$(GUROBI_HOME)/lib -Wl,-rpath,$(GUROBI_HOME)/lib
LDLIBS   := -lgurobi_c++ -lgurobi120

SRC := src/main.cpp src/modelo_pmcoi.cpp src/instancia.cpp
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := bin/pmcoi

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.cpp $(wildcard include/*.hpp) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

clean:
	rm -rf build bin
