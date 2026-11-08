# PMCOI — modelo exato com Gurobi
#
#   make          compila bin/pmcoi
#   make teste    compila e roda o teste do modelo (testes/teste_modelo.cpp)
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

BIN_TESTE := bin/teste_modelo

.PHONY: all teste clean

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

teste: $(BIN_TESTE)
	./$(BIN_TESTE)

$(BIN_TESTE): build/teste_modelo.o build/modelo_pmcoi.o | bin
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.cpp $(wildcard include/*.hpp) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/teste_modelo.o: testes/teste_modelo.cpp $(wildcard include/*.hpp) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

clean:
	rm -rf build bin
