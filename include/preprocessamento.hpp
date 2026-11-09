#pragma once
// Pré-processamento de Leal, Frota & Sotelo (SBPO 2023, Seção 4).
//
// A instância é vista como um grafo bipartido (S̄ ∪ B, E), onde a aresta (b, s)
// indica que s pode atender b. A aresta (b, s) é dominada se existe q ≠ s com
//
//     p̄_q + c^lo_qb  <  p_s + c^lo_sb
//
// (mesmo com s no preço mínimo e q no máximo, q é mais barato para b), e então
// é removida. Pontos de venda sem arestas são removidos da instância.

#include "instancia.hpp"

// Devolve a instância reduzida: só os pontos de venda com ao menos uma aresta
// (|S|_P e |Sc|_P), todos os consumidores, e o campo `aresta` preenchido.
Instancia preprocessar(const Instancia& inst);

int contarArestas(const Instancia& inst);
