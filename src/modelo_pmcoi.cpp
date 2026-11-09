#include "modelo_pmcoi.hpp"

#include <cmath>

using std::to_string;

namespace {

std::string statusParaTexto(int st) {
    switch (st) {
        case GRB_OPTIMAL:     return "OPTIMAL";
        case GRB_INFEASIBLE:  return "INFEASIBLE";
        case GRB_INF_OR_UNBD: return "INF_OR_UNBD";
        case GRB_UNBOUNDED:   return "UNBOUNDED";
        case GRB_TIME_LIMIT:  return "TIME_LIMIT";
        case GRB_NODE_LIMIT:  return "NODE_LIMIT";
        case GRB_INTERRUPTED: return "INTERRUPTED";
        case GRB_SUBOPTIMAL:  return "SUBOPTIMAL";
        default:              return "STATUS_" + to_string(st);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Ambiente e parâmetros do Gurobi
// ---------------------------------------------------------------------------
ModeloPMCOI::ModeloPMCOI(const Instancia& inst, const ParametrosModelo& params)
    : inst_(inst), params_(params) {
    env_ = std::make_unique<GRBEnv>(true);  // true = configurar antes de iniciar
    env_->set(GRB_IntParam_OutputFlag, params_.verbose ? 1 : 0);
    if (!params_.arquivoLog.empty()) env_->set(GRB_StringParam_LogFile, params_.arquivoLog);
    env_->start();

    modelo_ = std::make_unique<GRBModel>(*env_);
    modelo_->set(GRB_StringAttr_ModelName, "PMCOI_" + inst_.nome);
    modelo_->set(GRB_DoubleParam_TimeLimit, params_.tempoLimite);
    modelo_->set(GRB_IntParam_Threads, params_.threads);
    modelo_->set(GRB_DoubleParam_MIPGap, 0.0);  // só para no ótimo exato
}

std::string ModeloPMCOI::nome(const std::string& base) const {
    return params_.usarNomes ? base : std::string();
}

void ModeloPMCOI::construir() {
    criarVariaveis();
    criarFuncaoObjetivo();
    restricaoAtendimento();
    restricaoMargem();
    restricaoLimitesPreco();
    restricaoPrecoConcorrente();
    restricaoLucroMargem();
    restricaoLucroAtendimento();
    restricaoMenorCusto();
    restricaoUmIntervalo();
    restricaoIntervaloVolume();
    modelo_->update();
    construido_ = true;
}

// ---------------------------------------------------------------------------
// Variáveis (Tabela 2) — todas não negativas
// ---------------------------------------------------------------------------
void ModeloPMCOI::criarVariaveis() {
    const int nS = inst_.nS(), nTotal = inst_.nTotal(), nB = inst_.nB();

    // p_s >= 0, s ∈ S̄ (preço)
    p_.resize(nTotal);
    for (int s = 0; s < nTotal; ++s)
        p_[s] = modelo_->addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, nome("p_" + to_string(s)));

    // m_s >= 0, s ∈ S (margem de lucro unitária)
    m_.resize(nS);
    for (int s = 0; s < nS; ++s)
        m_[s] = modelo_->addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, nome("m_" + to_string(s)));

    // x_sb >= 0, s ∈ S, b ∈ B (lucro de s com b)
    x_.assign(nS, std::vector<GRBVar>(nB));
    for (int s = 0; s < nS; ++s)
        for (int b = 0; b < nB; ++b)
            x_[s][b] = modelo_->addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS,
                                       nome("x_" + to_string(s) + "_" + to_string(b)));

    // y_sb ∈ {0,1}, s ∈ S̄, b ∈ B (s atende b)
    y_.assign(nTotal, std::vector<GRBVar>(nB));
    for (int s = 0; s < nTotal; ++s)
        for (int b = 0; b < nB; ++b)
            y_[s][b] = modelo_->addVar(0.0, 1.0, 0.0, GRB_BINARY,
                                       nome("y_" + to_string(s) + "_" + to_string(b)));

    // w_sl ∈ {0,1}, s ∈ S, l ∈ L_s (volume de s está no intervalo l)
    w_.resize(nS);
    for (int s = 0; s < nS; ++s) {
        const int L = (int)inst_.companhia[s].intervalos.size();
        w_[s].resize(L);
        for (int l = 0; l < L; ++l)
            w_[s][l] = modelo_->addVar(0.0, 1.0, 0.0, GRB_BINARY,
                                       nome("w_" + to_string(s) + "_" + to_string(l)));
    }
}

// ---------------------------------------------------------------------------
// (1)  max  Σ_{s∈S} Σ_{b∈B} x_sb
// ---------------------------------------------------------------------------
void ModeloPMCOI::criarFuncaoObjetivo() {
    GRBLinExpr fo = 0;
    for (int s = 0; s < inst_.nS(); ++s)
        for (int b = 0; b < inst_.nB(); ++b) fo += x_[s][b];
    modelo_->setObjective(fo, GRB_MAXIMIZE);
}

// ---------------------------------------------------------------------------
// (2)  Σ_{s∈S̄} y_sb = 1                                  ∀ b ∈ B
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoAtendimento() {
    for (int b = 0; b < inst_.nB(); ++b) {
        GRBLinExpr soma = 0;
        for (int s = 0; s < inst_.nTotal(); ++s) soma += y_[s][b];
        modelo_->addConstr(soma == 1.0, nome("R2_" + to_string(b)));
    }
}

// ---------------------------------------------------------------------------
// (3)  p_s = m_s + Σ_{l∈L_s} c^op_sl w_sl                ∀ s ∈ S
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoMargem() {
    for (int s = 0; s < inst_.nS(); ++s) {
        GRBLinExpr custo = 0;
        const auto& ivs = inst_.companhia[s].intervalos;
        for (int l = 0; l < (int)ivs.size(); ++l) custo += ivs[l].cop * w_[s][l];
        modelo_->addConstr(p_[s] == m_[s] + custo, nome("R3_" + to_string(s)));
    }
}

// ---------------------------------------------------------------------------
// (4)  p_s ≤ p_s ≤ p̄_s                                    ∀ s ∈ S
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoLimitesPreco() {
    for (int s = 0; s < inst_.nS(); ++s) {
        modelo_->addConstr(p_[s] >= inst_.companhia[s].pMin, nome("R4_min_" + to_string(s)));
        modelo_->addConstr(p_[s] <= inst_.companhia[s].pMax, nome("R4_max_" + to_string(s)));
    }
}

// ---------------------------------------------------------------------------
// (5)  p_s = pc_s                                         ∀ s ∈ Sc
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoPrecoConcorrente() {
    for (int s = inst_.nS(); s < inst_.nTotal(); ++s)
        modelo_->addConstr(p_[s] == inst_.concorrentes[s - inst_.nS()].preco,
                           nome("R5_" + to_string(s)));
}

// ---------------------------------------------------------------------------
// (6)  x_sb ≤ m_s d_b                                     ∀ s ∈ S, b ∈ B
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoLucroMargem() {
    for (int s = 0; s < inst_.nS(); ++s)
        for (int b = 0; b < inst_.nB(); ++b)
            modelo_->addConstr(x_[s][b] <= inst_.consumidores[b].demanda * m_[s],
                               nome("R6_" + to_string(s) + "_" + to_string(b)));
}

// ---------------------------------------------------------------------------
// (7)  x_sb ≤ y_sb d_b p̄_s                                ∀ s ∈ S, b ∈ B
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoLucroAtendimento() {
    for (int s = 0; s < inst_.nS(); ++s) {
        const double pMax = inst_.companhia[s].pMax;
        for (int b = 0; b < inst_.nB(); ++b)
            modelo_->addConstr(x_[s][b] <= inst_.consumidores[b].demanda * pMax * y_[s][b],
                               nome("R7_" + to_string(s) + "_" + to_string(b)));
    }
}

// ---------------------------------------------------------------------------
// (8)  p_s + c^lo_sb − p_q − c^lo_qb + α ≤ (1 − y_sb) M     ∀ s,q ∈ S̄, s≠q, b ∈ B
//      α = 1 se s ∈ S e q ∈ Sc; α = 0 caso contrário.
//
//      M_sqb = p̄_s + c^lo_sb − p_q − c^lo_qb + α  (maior valor do lado esquerdo).
//      Se M_sqb ≤ 0, a restrição é sempre satisfeita e não é criada.
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoMenorCusto() {
    const int nTotal = inst_.nTotal(), nB = inst_.nB();
    for (int b = 0; b < nB; ++b) {
        const auto& clo = inst_.clo[b];
        for (int s = 0; s < nTotal; ++s) {
            for (int q = 0; q < nTotal; ++q) {
                if (s == q) continue;
                // α = 1 se s ∈ S e q ∈ Sc; α = 0 caso contrário
                const double alpha = (inst_.ehCompanhia(s) && !inst_.ehCompanhia(q)) ? 1.0 : 0.0;
                const double cte = clo[s] - clo[q] + alpha;
                const double M = inst_.precoMax(s) - inst_.precoMin(q) + cte;
                if (M <= 0.0) continue;
                // p_s − p_q + M y_sb ≤ M − cte
                modelo_->addConstr(p_[s] - p_[q] + M * y_[s][b] <= M - cte,
                                   nome("R8_" + to_string(s) + "_" + to_string(q) + "_" + to_string(b)));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// (9)  Σ_{l∈L_s} w_sl = 1                                 ∀ s ∈ S
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoUmIntervalo() {
    for (int s = 0; s < inst_.nS(); ++s) {
        GRBLinExpr soma = 0;
        for (const auto& v : w_[s]) soma += v;
        modelo_->addConstr(soma == 1.0, nome("R9_" + to_string(s)));
    }
}

// ---------------------------------------------------------------------------
// (10) w_sl u_sl ≤ Σ_{b∈B} y_sb d_b ≤ (1 − w_sl) M + ū_sl     ∀ s ∈ S, l ∈ L_s
//
//      M_sl = Σ_b d_b − ū_sl  (volume máximo possível menos ū_sl).
//      Se ū_sl = ∞ ou M_sl ≤ 0, o lado direito é sempre satisfeito e não é criado.
// ---------------------------------------------------------------------------
void ModeloPMCOI::restricaoIntervaloVolume() {
    const double demandaTotal = inst_.demandaTotal();
    for (int s = 0; s < inst_.nS(); ++s) {
        GRBLinExpr volume = 0;
        for (int b = 0; b < inst_.nB(); ++b) volume += inst_.consumidores[b].demanda * y_[s][b];

        const auto& ivs = inst_.companhia[s].intervalos;
        for (int l = 0; l < (int)ivs.size(); ++l) {
            const std::string id = to_string(s) + "_" + to_string(l);
            modelo_->addConstr(ivs[l].uMin * w_[s][l] <= volume, nome("R10_min_" + id));

            if (std::isinf(ivs[l].uMax)) continue;
            const double M = demandaTotal - ivs[l].uMax;
            if (M <= 0.0) continue;
            modelo_->addConstr(volume <= (1.0 - w_[s][l]) * M + ivs[l].uMax, nome("R10_max_" + id));
        }
    }
}

// ---------------------------------------------------------------------------
// Otimização e extração da solução
// ---------------------------------------------------------------------------
ResultadoModelo ModeloPMCOI::resolver() {
    if (!construido_) construir();

    modelo_->optimize();

    ResultadoModelo r;
    const int st = modelo_->get(GRB_IntAttr_Status);
    r.status = statusParaTexto(st);
    r.tempo = modelo_->get(GRB_DoubleAttr_Runtime);
    r.numVars = modelo_->get(GRB_IntAttr_NumVars);
    r.numRestricoes = modelo_->get(GRB_IntAttr_NumConstrs);
    r.temSolucao = modelo_->get(GRB_IntAttr_SolCount) > 0;
    if (st != GRB_INFEASIBLE && st != GRB_INF_OR_UNBD) {
        r.nos = modelo_->get(GRB_DoubleAttr_NodeCount);
        r.limitante = modelo_->get(GRB_DoubleAttr_ObjBound);
    }
    if (!r.temSolucao) return r;

    r.fo = modelo_->get(GRB_DoubleAttr_ObjVal);
    r.gap = 100.0 * modelo_->get(GRB_DoubleAttr_MIPGap);

    const int nS = inst_.nS(), nTotal = inst_.nTotal(), nB = inst_.nB();
    r.preco.resize(nS);
    r.margem.resize(nS);
    r.intervalo.assign(nS, -1);
    for (int s = 0; s < nS; ++s) {
        r.preco[s] = p_[s].get(GRB_DoubleAttr_X);
        r.margem[s] = m_[s].get(GRB_DoubleAttr_X);
        for (int l = 0; l < (int)w_[s].size(); ++l)
            if (w_[s][l].get(GRB_DoubleAttr_X) > 0.5) r.intervalo[s] = l;
    }
    r.vendedor.assign(nB, -1);
    for (int b = 0; b < nB; ++b)
        for (int s = 0; s < nTotal; ++s)
            if (y_[s][b].get(GRB_DoubleAttr_X) > 0.5) r.vendedor[b] = s;
    return r;
}

void ModeloPMCOI::exportar(const std::string& arquivo) {
    if (!construido_) construir();
    modelo_->write(arquivo);
}
