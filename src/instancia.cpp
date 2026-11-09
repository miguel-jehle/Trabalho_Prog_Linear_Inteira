#include "instancia.hpp"

#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

// Formato dos arquivos InstanciasPCMUS/INST<|S|>_<|Sc|>_<|B|><nome>:
//
//   |S| |Sc| |B|
//   codigoIBGE  precoMin  precoMax  niveisGastosOp          (|S| linhas)
//   codigoIBGE  LimiteFinalGasto  GastoOperacional           (Σ niveis linhas; -1 = sem limite)
//   codigoIBGE  precoPCMUS                                   (|Sc| linhas)
//   codIBGE  demanda                                         (|B| linhas)
//   codIBGE  codIBGE  custologistico                         (|S̄|·|B| linhas)
//
// Cada tabela é precedida por uma linha de cabeçalho. Os custos logísticos vêm
// em blocos de |B| linhas por ponto de venda, na ordem companhia -> concorrentes,
// e dentro de cada bloco na ordem dos consumidores.
//
// O mesmo código IBGE pode aparecer na companhia e nos concorrentes, por isso
// os pontos de venda são identificados pela posição, não pelo código.

namespace {

using Linha = std::vector<std::string>;

bool ehNumero(const std::string& tok) {
    return !tok.empty() && (std::isdigit((unsigned char)tok[0]) || tok[0] == '-');
}

// Lê todas as linhas de dados do arquivo, descartando cabeçalhos e linhas vazias.
std::vector<Linha> lerLinhasDados(const std::string& caminho) {
    std::ifstream in(caminho);
    if (!in) throw std::runtime_error("Nao foi possivel abrir a instancia: " + caminho);

    std::vector<Linha> linhas;
    std::string texto;
    while (std::getline(in, texto)) {
        std::istringstream ss(texto);
        Linha campos;
        std::string tok;
        while (ss >> tok) campos.push_back(tok);
        if (!campos.empty() && ehNumero(campos[0])) linhas.push_back(std::move(campos));
    }
    return linhas;
}

std::string nomeArquivo(const std::string& caminho) {
    auto barra = caminho.find_last_of("/\\");
    return (barra == std::string::npos) ? caminho : caminho.substr(barra + 1);
}

void exigir(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error("Instancia invalida: " + msg);
}

}  // namespace

Instancia lerInstancia(const std::string& caminho) {
    const std::vector<Linha> linhas = lerLinhasDados(caminho);
    size_t i = 0;
    auto proxima = [&](size_t numCampos, const char* tabela) -> const Linha& {
        exigir(i < linhas.size(), std::string("fim do arquivo ao ler ") + tabela);
        const Linha& l = linhas[i++];
        exigir(l.size() >= numCampos, std::string("linha com campos faltando em ") + tabela);
        return l;
    };

    Instancia inst;
    inst.nome = nomeArquivo(caminho);

    const Linha& cab = proxima(3, "cabecalho");
    const int nS = std::stoi(cab[0]);
    const int nSc = std::stoi(cab[1]);
    const int nB = std::stoi(cab[2]);

    // Pontos de venda da companhia
    std::vector<int> numNiveis(nS);
    inst.companhia.resize(nS);
    for (int s = 0; s < nS; ++s) {
        const Linha& l = proxima(4, "pontos da companhia");
        inst.companhia[s].nome = l[0];
        inst.companhia[s].pMin = std::stod(l[1]);
        inst.companhia[s].pMax = std::stod(l[2]);
        numNiveis[s] = std::stoi(l[3]);
        exigir(numNiveis[s] > 0, "ponto " + l[0] + " sem niveis de gasto operacional");
    }

    // Intervalos de gasto operacional: [limite anterior, LimiteFinalGasto]
    for (int s = 0; s < nS; ++s) {
        double limiteAnterior = 0.0;
        for (int l = 0; l < numNiveis[s]; ++l) {
            const Linha& lin = proxima(3, "gastos operacionais");
            exigir(lin[0] == inst.companhia[s].nome,
                   "gasto operacional de " + lin[0] + " fora da ordem (esperado " + inst.companhia[s].nome + ")");
            const double limite = std::stod(lin[1]);
            Intervalo iv;
            iv.uMin = limiteAnterior;
            iv.uMax = (limite < 0) ? std::numeric_limits<double>::infinity() : limite;
            iv.cop = std::stod(lin[2]);
            inst.companhia[s].intervalos.push_back(iv);
            limiteAnterior = iv.uMax;
        }
    }

    // Concorrentes
    inst.concorrentes.resize(nSc);
    for (int q = 0; q < nSc; ++q) {
        const Linha& l = proxima(2, "concorrentes");
        inst.concorrentes[q].nome = l[0];
        inst.concorrentes[q].preco = std::stod(l[1]);
    }

    // Consumidores
    inst.consumidores.resize(nB);
    for (int b = 0; b < nB; ++b) {
        const Linha& l = proxima(2, "consumidores");
        inst.consumidores[b].nome = l[0];
        inst.consumidores[b].demanda = std::stod(l[1]);
    }

    // Custos logísticos: blocos de |B| linhas por ponto de venda s ∈ S̄
    const int nTotal = nS + nSc;
    inst.clo.assign(nB, std::vector<double>(nTotal));
    for (int s = 0; s < nTotal; ++s) {
        const std::string& codVendedor = inst.ehCompanhia(s) ? inst.companhia[s].nome
                                                             : inst.concorrentes[s - nS].nome;
        for (int b = 0; b < nB; ++b) {
            const Linha& l = proxima(3, "custos logisticos");
            exigir(l[0] == codVendedor && l[1] == inst.consumidores[b].nome,
                   "custo logistico fora da ordem: (" + l[0] + ", " + l[1] + "), esperado (" +
                       codVendedor + ", " + inst.consumidores[b].nome + ")");
            inst.clo[b][s] = std::stod(l[2]);
        }
    }

    exigir(i == linhas.size(), "linhas extras no fim do arquivo");
    return inst;
}
