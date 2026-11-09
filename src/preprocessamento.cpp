#include "preprocessamento.hpp"

#include <limits>

Instancia preprocessar(const Instancia& inst) {
    const int nS = inst.nS(), nTotal = inst.nTotal(), nB = inst.nB();
    const double INF = std::numeric_limits<double>::infinity();

    // 1) Arestas não dominadas no grafo original.
    std::vector<std::vector<bool>> aresta(nB, std::vector<bool>(nTotal, false));
    std::vector<bool> ativo(nTotal, false);

    for (int b = 0; b < nB; ++b) {
        const auto& clo = inst.clo[b];

        // Menor e segundo menor p̄_q + c^lo_qb, para obter o mínimo sobre q ≠ s.
        double menor = INF, segundo = INF;
        int argMenor = -1;
        for (int q = 0; q < nTotal; ++q) {
            const double c = inst.precoMax(q) + clo[q];
            if (c < menor) {
                segundo = menor;
                menor = c;
                argMenor = q;
            } else if (c < segundo) {
                segundo = c;
            }
        }

        for (int s = 0; s < nTotal; ++s) {
            const double melhorOutro = (s == argMenor) ? segundo : menor;
            if (melhorOutro < inst.precoMin(s) + clo[s]) continue;  // dominada
            aresta[b][s] = true;
            ativo[s] = true;
        }
    }

    // 2) Instância reduzida, mantendo a ordem companhia -> concorrentes.
    std::vector<int> mantidos;
    Instancia red;
    red.nome = inst.nome;
    red.consumidores = inst.consumidores;
    for (int s = 0; s < nTotal; ++s) {
        if (!ativo[s]) continue;
        mantidos.push_back(s);
        if (s < nS) red.companhia.push_back(inst.companhia[s]);
        else red.concorrentes.push_back(inst.concorrentes[s - nS]);
    }

    red.clo.assign(nB, std::vector<double>(mantidos.size()));
    red.aresta.assign(nB, std::vector<bool>(mantidos.size()));
    for (int b = 0; b < nB; ++b) {
        for (size_t k = 0; k < mantidos.size(); ++k) {
            red.clo[b][k] = inst.clo[b][mantidos[k]];
            red.aresta[b][k] = aresta[b][mantidos[k]];
        }
    }
    return red;
}

int contarArestas(const Instancia& inst) {
    int total = 0;
    for (int b = 0; b < inst.nB(); ++b)
        for (int s = 0; s < inst.nTotal(); ++s)
            if (inst.temAresta(b, s)) ++total;
    return total;
}
