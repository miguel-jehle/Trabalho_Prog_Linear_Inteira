// Resolve uma instância do PMCOI com o modelo exato (Gurobi).
//
// Uso: ./bin/pmcoi <instancia> [-t tempoLimite] [-o resultados.csv] [-q]
//   -t  tempo limite em segundos (padrão: 3600, como no artigo)
//   -o  acrescenta uma linha com o resultado ao CSV (cria com cabeçalho se não existir)
//   -q  desliga o log do Gurobi

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "instancia.hpp"
#include "modelo_pmcoi.hpp"
#include "preprocessamento.hpp"

namespace {

void uso(const char* prog) {
    std::cerr << "Uso: " << prog << " <instancia> [-t tempoLimite] [-o resultados.csv] [-q]\n";
}

void imprimirResultado(const Instancia& original, const Instancia& inst, const ResultadoModelo& r) {
    std::cout << "\n===== Resultado =====\n"
              << "Instancia : " << inst.nome << "\n"
              << "Original  : |S|=" << original.nS() << "  |Sc|=" << original.nSc()
              << "  |B|=" << original.nB() << "  arestas=" << original.nTotal() * original.nB() << "\n"
              << "Pre-proc. : |S|P=" << inst.nS() << "  |Sc|P=" << inst.nSc()
              << "  |B|=" << inst.nB() << "  arestas=" << contarArestas(inst) << "\n"
              << "Status    : " << r.status << "\n"
              << std::fixed << std::setprecision(2)
              << "FO_EX     : " << r.fo << "\n"
              << "Limitante : " << r.limitante << "\n"
              << "Gap (%)   : " << r.gap << "\n"
              << "T_EX (s)  : " << r.tempo << "\n"
              << std::setprecision(0)
              << "Nos B&B   : " << r.nos << "\n"
              << "Vars/Restr: " << r.numVars << " / " << r.numRestricoes << "\n";

    if (!r.temSolucao) return;
    std::cout << "\nPrecos da companhia:\n" << std::setprecision(2);
    for (int s = 0; s < inst.nS(); ++s)
        std::cout << "  " << std::setw(20) << std::left << inst.companhia[s].nome << std::right
                  << " p=" << std::setw(10) << r.preco[s]
                  << " m=" << std::setw(10) << r.margem[s]
                  << " l=" << r.intervalo[s] << "\n";
}

void salvarCsv(const std::string& arquivo, const Instancia& original, const Instancia& inst,
               const ResultadoModelo& r) {
    bool existe = std::ifstream(arquivo).good();
    std::ofstream out(arquivo, std::ios::app);
    if (!out) {
        std::cerr << "Nao foi possivel abrir " << arquivo << "\n";
        return;
    }
    if (!existe) out << "instancia,S,Sc,B,S_P,Sc_P,arestas,status,FO_EX,limitante,gap,T_EX,nos\n";
    out << std::fixed << std::setprecision(4)
        << inst.nome << "," << original.nS() << "," << original.nSc() << "," << original.nB() << ","
        << inst.nS() << "," << inst.nSc() << "," << contarArestas(inst) << ","
        << r.status << "," << r.fo << "," << r.limitante << "," << r.gap << ","
        << r.tempo << "," << r.nos << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        uso(argv[0]);
        return 1;
    }

    std::string caminhoInstancia = argv[1];
    std::string arquivoCsv;
    ParametrosModelo params;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-t" && i + 1 < argc) params.tempoLimite = std::stod(argv[++i]);
        else if (arg == "-o" && i + 1 < argc) arquivoCsv = argv[++i];
        else if (arg == "-q") params.verbose = false;
        else {
            uso(argv[0]);
            return 1;
        }
    }

    try {
        Instancia original = lerInstancia(caminhoInstancia);
        Instancia inst = preprocessar(original);

        ModeloPMCOI modelo(inst, params);
        modelo.construir();
        ResultadoModelo r = modelo.resolver();

        imprimirResultado(original, inst, r);
        if (!arquivoCsv.empty()) salvarCsv(arquivoCsv, original, inst, r);
    } catch (const GRBException& e) {
        std::cerr << "Erro Gurobi (" << e.getErrorCode() << "): " << e.getMessage() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
