#pragma once
// Modelo exato do PMCOI (Precificação em Mercados Competitivos com Oferta
// Irrestrita) — formulação (1)-(10) de Leal, Frota & Sotelo (SBPO 2025),
// resolvido com Gurobi.

#include <memory>
#include <string>
#include <vector>

#include "gurobi_c++.h"
#include "instancia.hpp"

struct ParametrosModelo {
    double tempoLimite = 3600.0;   // artigo: 1 hora
    int threads = 1;               // artigo: uma única thread
    bool verbose = true;           // log do Gurobi na tela
    bool usarNomes = true;         // nomes em variáveis/restrições (desligar em instâncias grandes)
    std::string arquivoLog;        // log do Gurobi em arquivo (opcional)
};

struct ResultadoModelo {
    std::string status;
    bool temSolucao = false;
    double fo = 0.0;          // FO_EX
    double limitante = 0.0;   // melhor bound
    double gap = 0.0;         // gap (%)
    double tempo = 0.0;       // T_EX (s)
    double nos = 0.0;         // nós de B&B explorados
    int numVars = 0, numRestricoes = 0;

    std::vector<double> preco;    // p_s,  s ∈ S
    std::vector<double> margem;   // m_s,  s ∈ S
    std::vector<int> intervalo;   // l com w_sl = 1, s ∈ S
    std::vector<int> vendedor;    // s ∈ S̄ com y_sb = 1, para cada b ∈ B
};

class ModeloPMCOI {
public:
    ModeloPMCOI(const Instancia& inst, const ParametrosModelo& params);

    void construir();                          // cria variáveis, FO (1) e restrições (2)-(10)
    ResultadoModelo resolver();                // otimiza e extrai a solução
    void exportar(const std::string& arquivo); // .lp / .mps / .sol (pela extensão)

private:
    const Instancia& inst_;
    ParametrosModelo params_;

    std::unique_ptr<GRBEnv> env_;
    std::unique_ptr<GRBModel> modelo_;

    // Variáveis (Tabela 2)
    std::vector<GRBVar> p_;                // p_s,   s ∈ S̄
    std::vector<GRBVar> m_;                // m_s,   s ∈ S
    std::vector<std::vector<GRBVar>> x_;   // x_sb,  s ∈ S,  b ∈ B
    std::vector<std::vector<GRBVar>> y_;   // y_sb,  s ∈ S̄, b ∈ B
    std::vector<std::vector<GRBVar>> w_;   // w_sl,  s ∈ S,  l ∈ L_s

    bool construido_ = false;

    void criarVariaveis();
    void criarFuncaoObjetivo();        // (1)
    void restricaoAtendimento();       // (2)
    void restricaoMargem();            // (3)
    void restricaoLimitesPreco();      // (4)
    void restricaoPrecoConcorrente();  // (5)
    void restricaoLucroMargem();       // (6)
    void restricaoLucroAtendimento();  // (7)
    void restricaoMenorCusto();        // (8)
    void restricaoUmIntervalo();       // (9)
    void restricaoIntervaloVolume();   // (10)

    std::string nome(const std::string& base) const;  // "" se usarNomes == false
};
