// Teste do modelo PMCOI com uma instância pequena de ótimo conhecido.
//
// Instância:
//   Companhia (S):
//     A (s=0): p ∈ [1, 20]; intervalos: [0, 99] c^op = 5 ; [100, ∞) c^op = 2
//     B (s=1): p ∈ [1, 20]; intervalo : [0, ∞)  c^op = 4
//   Concorrente (Sc):
//     C (s=2): pc = 10
//   Consumidores (B):        d_b   c^lo (A, B, C)
//     b0                      60    (0, 6, 0)
//     b1                      50    (6, 0, 0)
//     b2                      40    (1, 1, 0)
//
// Ótimo calculado à mão:
//   A: p = 8 -> atende b0 (custo 8) e b2 (custo 9 = 10 - α, com α = 1)
//      volume 100 -> intervalo l = 1 (c^op = 2) -> lucro 6 * 100 = 600
//   B: p = 9 -> atende b1 (custo 9 = 10 - α)
//      volume 50  -> intervalo l = 0 (c^op = 4) -> lucro 5 * 50  = 250
//   FO = 850
//
// O que o teste distingue:
//   - α errado (α = 0): A cobraria 9 e empataria com C em b2 -> FO = 1000
//   - intervalos ignorados (c^op de A sempre 5): FO = 550

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

#include "instancia.hpp"
#include "modelo_pmcoi.hpp"

namespace {

int falhas = 0;

void verificar(bool ok, const std::string& descricao) {
    std::cout << (ok ? "  [OK]    " : "  [FALHA] ") << descricao << "\n";
    if (!ok) ++falhas;
}

bool igual(double a, double b, double tol = 1e-4) { return std::fabs(a - b) <= tol; }

Instancia montarInstancia() {
    const double INF = std::numeric_limits<double>::infinity();
    Instancia inst;
    inst.nome = "teste_pequeno";

    inst.companhia = {
        {"A", 1.0, 20.0, {{0.0, 99.0, 5.0}, {100.0, INF, 2.0}}},
        {"B", 1.0, 20.0, {{0.0, INF, 4.0}}},
    };
    inst.concorrentes = {{"C", 10.0}};
    inst.consumidores = {{"b0", 60.0}, {"b1", 50.0}, {"b2", 40.0}};

    //            A    B    C
    inst.clo = {{0.0, 6.0, 0.0},   // b0
                {6.0, 0.0, 0.0},   // b1
                {1.0, 1.0, 0.0}};  // b2
    return inst;
}

}  // namespace

int main() {
    try {
        Instancia inst = montarInstancia();

        ParametrosModelo params;
        params.tempoLimite = 60.0;
        params.verbose = false;

        ModeloPMCOI modelo(inst, params);
        ResultadoModelo r = modelo.resolver();

        std::cout << "Teste: " << inst.nome << "\n";
        verificar(r.status == "OPTIMAL", "status OPTIMAL (obtido: " + r.status + ")");
        if (!r.temSolucao) {
            std::cout << "\nSem solucao.\n";
            return 1;
        }
        verificar(igual(r.fo, 850.0), "FO = 850 (obtido: " + std::to_string(r.fo) + ")");
        verificar(igual(r.preco[0], 8.0), "p_A = 8 (obtido: " + std::to_string(r.preco[0]) + ")");
        verificar(igual(r.preco[1], 9.0), "p_B = 9 (obtido: " + std::to_string(r.preco[1]) + ")");
        verificar(igual(r.margem[0], 6.0), "m_A = 6 (obtido: " + std::to_string(r.margem[0]) + ")");
        verificar(igual(r.margem[1], 5.0), "m_B = 5 (obtido: " + std::to_string(r.margem[1]) + ")");
        verificar(r.intervalo[0] == 1, "A no intervalo 1 (obtido: " + std::to_string(r.intervalo[0]) + ")");
        verificar(r.intervalo[1] == 0, "B no intervalo 0 (obtido: " + std::to_string(r.intervalo[1]) + ")");
        verificar(r.vendedor[0] == 0, "b0 compra de A (obtido: " + std::to_string(r.vendedor[0]) + ")");
        verificar(r.vendedor[1] == 1, "b1 compra de B (obtido: " + std::to_string(r.vendedor[1]) + ")");
        verificar(r.vendedor[2] == 0, "b2 compra de A (obtido: " + std::to_string(r.vendedor[2]) + ")");
    } catch (const GRBException& e) {
        std::cerr << "Erro Gurobi (" << e.getErrorCode() << "): " << e.getMessage() << "\n";
        return 1;
    }

    std::cout << (falhas == 0 ? "\nTodos os testes passaram.\n" : "\nHa falhas.\n");
    return falhas == 0 ? 0 : 1;
}
