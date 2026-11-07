#pragma once
// Dados de uma instância do PMCOI (Tabela 1 do artigo).
//
// Índices de pontos de venda:
//   s ∈ S  (companhia)    -> 0 .. |S|-1
//   s ∈ Sc (concorrentes) -> |S| .. |S|+|Sc|-1
//   S̄ = S ∪ Sc            -> 0 .. |S̄|-1

#include <string>
#include <vector>

struct Intervalo {
    double uMin;  // u_sl  : mínimo de unidades vendidas no intervalo l
    double uMax;  // ū_sl  : máximo de unidades vendidas no intervalo l
    double cop;   // c^op_sl : custo operacional unitário no intervalo l
};

struct PontoVenda {
    std::string nome;
    double pMin;  // p_s : limite inferior do preço
    double pMax;  // p̄_s : limite superior do preço
    std::vector<Intervalo> intervalos;  // L_s
};

struct Concorrente {
    std::string nome;
    double preco;  // pc_s
};

struct Consumidor {
    std::string nome;
    double demanda;  // d_b
};

struct Instancia {
    std::string nome;
    std::vector<PontoVenda> companhia;      // S
    std::vector<Concorrente> concorrentes;  // Sc
    std::vector<Consumidor> consumidores;   // B
    std::vector<std::vector<double>> clo;   // clo[b][s] = c^lo_sb, s ∈ S̄

    int nS() const { return (int)companhia.size(); }
    int nSc() const { return (int)concorrentes.size(); }
    int nTotal() const { return nS() + nSc(); }  // |S̄|
    int nB() const { return (int)consumidores.size(); }

    bool ehCompanhia(int s) const { return s < nS(); }

    // Limites de preço de qualquer s ∈ S̄ (concorrente: pc_s nos dois).
    double precoMin(int s) const { return ehCompanhia(s) ? companhia[s].pMin : concorrentes[s - nS()].preco; }
    double precoMax(int s) const { return ehCompanhia(s) ? companhia[s].pMax : concorrentes[s - nS()].preco; }

    double demandaTotal() const {
        double t = 0.0;
        for (const auto& b : consumidores) t += b.demanda;
        return t;
    }
};
