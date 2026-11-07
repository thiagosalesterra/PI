#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "../include/sistema.h"
#include "../include/modelos.h"
#include "../include/algoritmos.h"
#include "../include/banco.h"
#include "../include/util.h"

static CatalogoProdutos catalogo;
static const char *CAMINHO_BANCO = "database/estoque.db";

static void exibir_cabecalho_tabela(void) {
    printf("\n%-6s | %-24s | %-14s | %-10s | %-8s | %-8s | %-12s\n",
           "CÓDIGO", "NOME", "CATEGORIA", "PREÇO(R$)", "ESTOQUE", "ESTOQUE MÍNIMO", "STATUS");
    printf("--------------------------------------------------------------------------------------------\n");
}

static void exibir_linha_produto(const Produto *p) {
    const char *status = (p->quantidade <= p->estoque_minimo) ? "[ALERTA: BAIXO]" : "NORMAL";
    printf("%-6d | %-24.24s | %-14.14s | %10.2f | %8d | %8d | %-12s\n",
           p->codigo, p->nome, p->categoria, p->preco, p->quantidade, p->estoque_minimo, status);
}

void inicializar_sistema(void) {
#if defined(_WIN32) || defined(_WIN64)
    // Configura o console do Windows para UTF-8 de forma segura
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    catalogo.total = 0;
    memset(catalogo.itens, 0, sizeof(catalogo.itens));

    printf("========================================================\n");
    printf("  SISTEMA DE CONTROLE DE ESTOQUE - ENGENHARIA 2026      \n");
    printf("========================================================\n");
    printf("[SISTEMA] Conectando ao banco SQLite: %s...\n", CAMINHO_BANCO);

    if (!banco_inicializar(CAMINHO_BANCO)) {
        printf("[FATAL] Não foi possível inicializar o banco de dados. Encerrando...\n");
        exit(EXIT_FAILURE);
    }

    if (!banco_carregar_produtos(catalogo.itens, &catalogo.total, MAX_PRODUTOS)) {
        printf("[AVISO] Falha ao sincronizar dados do SQLite para a memória RAM.\n");
    } else {
        printf("[SUCESSO] %d produtos carregados do SQLite para a memória RAM.\n", catalogo.total);
    }

    // Mantem o array ordenado por codigo para habilitar busca binaria
    if (catalogo.total > 1) {
        ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);
    }
}

void finalizar_sistema(void) {
    printf("\n[SISTEMA] Encerrando conexão com o SQLite...\n");
    banco_fechar();
    printf("[SISTEMA] Recursos liberados com sucesso. Até logo!\n");
}

void sistema_cadastrar_produto(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("                CADASTRAR NOVO PRODUTO                 \n");
    printf("========================================================\n");

    if (catalogo.total >= MAX_PRODUTOS) {
        printf("[ERRO] Capacidade máxima atingida (%d produtos).\n", MAX_PRODUTOS);
        pausar_console();
        return;
    }

    Produto novo;
    memset(&novo, 0, sizeof(Produto));

    // Validacao de Codigo unico
    int cod = 0;
    while (1) {
        if (!ler_inteiro_seguro("Digite o código do produto (1 a 999999): ", &cod, 1, 999999)) {
            return;
        }

        // Busca se ja existe na memoria RAM
        int existe = 0;
        for (int i = 0; i < catalogo.total; i++) {
            if (catalogo.itens[i].codigo == cod) {
                existe = 1;
                break;
            }
        }

        if (existe) {
            printf("[ERRO] O código %d já está cadastrado. Escolha outro código.\n", cod);
            continue;
        }

        novo.codigo = cod;
        break;
    }

    ler_string_segura("Nome do produto: ", novo.nome, sizeof(novo.nome));
    ler_string_segura("Descrição: ", novo.descricao, sizeof(novo.descricao));
    ler_string_segura("Categoria: ", novo.categoria, sizeof(novo.categoria));
    ler_double_seguro("Preço unitário (R$): ", &novo.preco, 0.01, 1000000.0);
    ler_inteiro_seguro("Quantidade inicial em estoque: ", &novo.quantidade, 0, 1000000);
    ler_inteiro_seguro("Estoque mínimo de segurança: ", &novo.estoque_minimo, 0, 1000000);

    // Persistencia no SQLite
    if (!banco_inserir_produto(&novo)) {
        printf("[ERRO] Falha ao persistir produto no banco de dados.\n");
        pausar_console();
        return;
    }

    // Se houver quantidade inicial > 0, registra a primeira movimentacao
    if (novo.quantidade > 0) {
        banco_registrar_movimentacao_transacional(novo.codigo, MOV_ENTRADA, novo.quantidade, "Estoque inicial de cadastro", novo.quantidade);
    }

    // Insercao no Array de Structs em RAM
    catalogo.itens[catalogo.total] = novo;
    catalogo.total++;

    // Reordena por codigo para manter garantia da busca binaria
    ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);

    printf("\n[SUCESSO] Produto '%s' (Cód: %d) cadastrado e persistido com sucesso!\n", novo.nome, novo.codigo);
    pausar_console();
}

void sistema_alterar_produto(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("                  ALTERAR DADOS DE PRODUTO              \n");
    printf("========================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] O estoque está vazio. Nenhum produto para alterar.\n");
        pausar_console();
        return;
    }

    int cod = 0;
    if (!ler_inteiro_seguro("Digite o código do produto que deseja alterar: ", &cod, 1, 999999)) {
        return;
    }

    // Garantir ordenacao antes da busca binaria
    if (!verificar_ordenado_por_codigo(catalogo.itens, catalogo.total)) {
        ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);
    }

    int idx = busca_binaria_codigo(catalogo.itens, catalogo.total, cod);
    if (idx == -1) {
        printf("[ERRO] Produto com código %d não foi encontrado.\n", cod);
        pausar_console();
        return;
    }

    Produto *p = &catalogo.itens[idx];
    printf("\n[DADOS ATUAIS]:\n");
    exibir_cabecalho_tabela();
    exibir_linha_produto(p);
    printf("--------------------------------------------------------------------------------------------\n");

    printf("\nInforme os novos dados (Código e Quantidade não são alterados diretamente aqui):\n");
    ler_string_segura("Novo Nome: ", p->nome, sizeof(p->nome));
    ler_string_segura("Nova Descrição: ", p->descricao, sizeof(p->descricao));
    ler_string_segura("Nova Categoria: ", p->categoria, sizeof(p->categoria));
    ler_double_seguro("Novo Preço Unitário (R$): ", &p->preco, 0.01, 1000000.0);
    ler_inteiro_seguro("Novo Estoque Mínimo: ", &p->estoque_minimo, 0, 1000000);

    if (banco_atualizar_produto(p)) {
        printf("\n[SUCESSO] Produto atualizado no SQLite e na memória RAM com sucesso!\n");
    } else {
        printf("\n[ERRO] Falha ao atualizar dados no banco SQLite.\n");
    }

    pausar_console();
}

void sistema_excluir_produto(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("                   EXCLUIR PRODUTO                      \n");
    printf("========================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Nenhum produto cadastrado.\n");
        pausar_console();
        return;
    }

    int cod = 0;
    if (!ler_inteiro_seguro("Digite o código do produto a ser excluído: ", &cod, 1, 999999)) {
        return;
    }

    if (!verificar_ordenado_por_codigo(catalogo.itens, catalogo.total)) {
        ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);
    }

    int idx = busca_binaria_codigo(catalogo.itens, catalogo.total, cod);
    if (idx == -1) {
        printf("[ERRO] Produto com código %d não encontrado.\n", cod);
        pausar_console();
        return;
    }

    printf("\nRegistro localizado:\n");
    exibir_cabecalho_tabela();
    exibir_linha_produto(&catalogo.itens[idx]);
    printf("--------------------------------------------------------------------------------------------\n");

    if (!confirmar_acao("Confirma a exclusão definitiva deste produto e suas movimentações?")) {
        printf("[CANCELADO] Operação abortada pelo usuário.\n");
        pausar_console();
        return;
    }

    // Exclusao no banco SQLite
    if (!banco_excluir_produto(cod)) {
        printf("[ERRO] Falha ao excluir produto no banco de dados.\n");
        pausar_console();
        return;
    }

    // Remocao no array de structs em RAM (deslocamento para esquerda)
    for (int i = idx; i < catalogo.total - 1; i++) {
        catalogo.itens[i] = catalogo.itens[i + 1];
    }
    catalogo.total--;

    printf("\n[SUCESSO] Produto %d excluído com sucesso do banco e da memória RAM!\n", cod);
    pausar_console();
}

void sistema_listar_produtos(void) {
    limpar_tela();
    printf("============================================================================================\n");
    printf("                            LISTAGEM GERAL DE PRODUTOS EM ESTOQUE                           \n");
    printf("============================================================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Nenhum produto cadastrado no catálogo.\n");
        pausar_console();
        return;
    }

    exibir_cabecalho_tabela();
    for (int i = 0; i < catalogo.total; i++) {
        exibir_linha_produto(&catalogo.itens[i]);
    }
    printf("--------------------------------------------------------------------------------------------\n");
    printf("Total de registros listados: %d produtos.\n", catalogo.total);

    pausar_console();
}

void sistema_pesquisar_por_codigo(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("      PESQUISAR PRODUTO POR CÓDIGO (BUSCA BINÁRIA)       \n");
    printf("========================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Nenhum produto cadastrado.\n");
        pausar_console();
        return;
    }

    int cod = 0;
    if (!ler_inteiro_seguro("Digite o código a ser pesquisado: ", &cod, 1, 999999)) {
        return;
    }

    // Pre-condicao academica obrigatoria para Busca Binaria: DADOS ORDENADOS POR CODIGO
    if (!verificar_ordenado_por_codigo(catalogo.itens, catalogo.total)) {
        printf("\n[ALGORITMO] Detectado vetor desordenado. Executando Insertion Sort por código...\n");
        ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);
        printf("[ALGORITMO] Vetor em RAM ordenado. Iniciando busca binária O(log n).\n");
    }

    int idx = busca_binaria_codigo(catalogo.itens, catalogo.total, cod);

    if (idx != -1) {
        printf("\n[SUCESSO] Produto localizado no índice [%d] da memória RAM:\n", idx);
        exibir_cabecalho_tabela();
        exibir_linha_produto(&catalogo.itens[idx]);
        printf("--------------------------------------------------------------------------------------------\n");
        printf("Descrição completa: %s\n", catalogo.itens[idx].descricao);
    } else {
        printf("\n[RESULTADO] Nenhum produto encontrado com o código %d.\n", cod);
    }

    pausar_console();
}

void sistema_pesquisar_por_nome(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("       PESQUISAR PRODUTO POR NOME (BUSCA LINEAR)        \n");
    printf("========================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Estoque vazio. Nenhum produto cadastrado.\n");
        pausar_console();
        return;
    }

    char termo[TAM_NOME];
    ler_string_segura("Digite o nome ou parte do nome para buscar: ", termo, sizeof(termo));

    int indices[MAX_PRODUTOS];
    int encontrados = busca_linear_nome(catalogo.itens, catalogo.total, termo, indices, MAX_PRODUTOS);

    if (encontrados > 0) {
        printf("\n[SUCESSO] %d produto(s) correspondente(s) encontrado(s) via Busca Linear:\n", encontrados);
        exibir_cabecalho_tabela();
        for (int i = 0; i < encontrados; i++) {
            exibir_linha_produto(&catalogo.itens[indices[i]]);
        }
        printf("--------------------------------------------------------------------------------------------\n");
    } else {
        printf("\n[RESULTADO] Nenhum produto contendo o termo '%s' foi encontrado.\n", termo);
    }

    pausar_console();
}

void sistema_ordenar_produtos(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("            ORDENAÇÃO MANUAL DE PRODUTOS EM RAM         \n");
    printf("========================================================\n");

    if (catalogo.total <= 1) {
        printf("[AVISO] Registros insuficientes para ordenação (%d produtos).\n", catalogo.total);
        pausar_console();
        return;
    }

    printf("Escolha o Algoritmo Manual de Ordenação em C:\n");
    printf("  1. Insertion Sort (Inserção Direta)\n");
    printf("  2. Bubble Sort    (Flutuação / Bolha)\n");
    printf("  3. Selection Sort (Seleção Direta)\n");
    int opc_algo = 1;
    ler_inteiro_seguro("Opção de Algoritmo (1-3): ", &opc_algo, 1, 3);

    printf("\nEscolha o Critério de Ordenação:\n");
    printf("  1. Código do Produto\n");
    printf("  2. Nome (Alfabético A-Z)\n");
    printf("  3. Preço Unitário (Crescente)\n");
    printf("  4. Quantidade em Estoque (Crescente)\n");
    int opc_crit = 1;
    ler_inteiro_seguro("Opção de Critério (1-4): ", &opc_crit, 1, 4);

    CriterioOrdenacao criterio = (CriterioOrdenacao)opc_crit;

    switch (opc_algo) {
        case 1:
            ordenar_insertion(catalogo.itens, catalogo.total, criterio);
            printf("\n[ALGORITMO] Insertion Sort executado manualmente com sucesso!\n");
            break;
        case 2:
            ordenar_bubble(catalogo.itens, catalogo.total, criterio);
            printf("\n[ALGORITMO] Bubble Sort executado manualmente com sucesso!\n");
            break;
        case 3:
            ordenar_selection(catalogo.itens, catalogo.total, criterio);
            printf("\n[ALGORITMO] Selection Sort executado manualmente com sucesso!\n");
            break;
    }

    // Exibe o resultado imediatamente
    exibir_cabecalho_tabela();
    for (int i = 0; i < catalogo.total; i++) {
        exibir_linha_produto(&catalogo.itens[i]);
    }
    printf("--------------------------------------------------------------------------------------------\n");

    pausar_console();
}

void sistema_registrar_movimentacao(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("         REGISTRAR MOVIMENTAÇÃO DE ESTOQUE              \n");
    printf("========================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Estoque vazio. Cadastre produtos antes de movimentar.\n");
        pausar_console();
        return;
    }

    int cod = 0;
    if (!ler_inteiro_seguro("Digite o código do produto a ser movimentado: ", &cod, 1, 999999)) {
        return;
    }

    if (!verificar_ordenado_por_codigo(catalogo.itens, catalogo.total)) {
        ordenar_insertion(catalogo.itens, catalogo.total, CRITERIO_CODIGO);
    }

    int idx = busca_binaria_codigo(catalogo.itens, catalogo.total, cod);
    if (idx == -1) {
        printf("[ERRO] Produto %d não encontrado no sistema.\n", cod);
        pausar_console();
        return;
    }

    Produto *p = &catalogo.itens[idx];
    printf("\nProduto Selecionado: %s | Estoque Atual: %d un | Estoque Mín: %d un\n",
           p->nome, p->quantidade, p->estoque_minimo);

    printf("\nSelecione o Tipo de Movimentação:\n");
    printf("  1. ENTRADA (Recebimento / Compra)\n");
    printf("  2. SAÍDA   (Venda / Baixa / Descarte)\n");
    int tipo_int = 1;
    ler_inteiro_seguro("Tipo (1 ou 2): ", &tipo_int, 1, 2);
    TipoMovimentacao tipo = (TipoMovimentacao)tipo_int;

    int qtd = 0;
    ler_inteiro_seguro("Quantidade a movimentar (> 0): ", &qtd, 1, 1000000);

    // Validacao de saldo previa para saidas
    if (tipo == MOV_SAIDA && qtd > p->quantidade) {
        printf("\n[BLOQUEIO] Saldo insuficiente! Estoque atual: %d, Tentativa de retirada: %d.\n",
               p->quantidade, qtd);
        pausar_console();
        return;
    }

    char observacao[TAM_OBS];
    ler_string_segura("Observação/Justificativa (Ex: NF 1045, Venda 201): ", observacao, sizeof(observacao));

    int novo_saldo = (tipo == MOV_ENTRADA) ? (p->quantidade + qtd) : (p->quantidade - qtd);

    // Persistencia Transacional Atomica no SQLite
    if (!banco_registrar_movimentacao_transacional(p->codigo, tipo, qtd, observacao, novo_saldo)) {
        printf("\n[ERRO TRANSACIONAL] Falha ao registrar movimentação no banco SQLite. Nenhuma alteração realizada.\n");
        pausar_console();
        return;
    }

    // Aplicacao por Referencia / Ponteiros na memoria RAM
    atualizar_estoque(p, qtd, tipo);

    printf("\n[SUCESSO TRANSACIONAL] Movimentação confirmada e gravada no SQLite!\n");
    printf("Novo saldo do produto '%s': %d unidades.\n", p->nome, p->quantidade);

    if (p->quantidade <= p->estoque_minimo) {
        printf("[ALERTA DE SEGURANÇA] ATENÇÃO: O produto atingiu ou está abaixo do Estoque Mínimo (%d un)!\n",
               p->estoque_minimo);
    }

    pausar_console();
}

void sistema_alerta_estoque_baixo(void) {
    limpar_tela();
    printf("============================================================================================\n");
    printf("                     ALERTA: PRODUTOS COM BAIXO ESTOQUE (<= ESTOQUE MÍNIMO)                 \n");
    printf("============================================================================================\n");

    int criticos = 0;
    exibir_cabecalho_tabela();

    for (int i = 0; i < catalogo.total; i++) {
        if (catalogo.itens[i].quantidade <= catalogo.itens[i].estoque_minimo) {
            exibir_linha_produto(&catalogo.itens[i]);
            criticos++;
        }
    }

    printf("--------------------------------------------------------------------------------------------\n");
    if (criticos == 0) {
        printf("[STATUS] Excelente! Nenhum produto está em nível crítico de estoque.\n");
    } else {
        printf("[ALERTA CRÍTICO] Foram identificados %d produto(s) demandando reposição imediata!\n", criticos);
    }

    pausar_console();
}

void sistema_calcular_valor_total(void) {
    limpar_tela();
    printf("========================================================\n");
    printf("          VALOR TOTAL E INDICADORES DO ESTOQUE          \n");
    printf("========================================================\n");

    double valor_total = 0.0;
    long total_unidades = 0;

    for (int i = 0; i < catalogo.total; i++) {
        valor_total += (catalogo.itens[i].preco * (double)catalogo.itens[i].quantidade);
        total_unidades += catalogo.itens[i].quantidade;
    }

    printf("Total de Tipos de Produtos Cadastrados : %d produtos\n", catalogo.total);
    printf("Total de Unidades Físicas em Estoque   : %ld unidades\n", total_unidades);
    printf("Valor Patrimonial Consolidado (R$)    : R$ %.2f\n", valor_total);
    if (total_unidades > 0) {
        printf("Preço Médio Ponderado por Item        : R$ %.2f\n", valor_total / (double)total_unidades);
    }
    printf("========================================================\n");

    pausar_console();
}

void sistema_matriz_movimentacoes_mensais(void) {
    limpar_tela();
    printf("====================================================================================================\n");
    printf("                  MATRIZ 2D DE MOVIMENTAÇÃO POR PERÍODO (PRODUTOS X 12 MESES)                      \n");
    printf("====================================================================================================\n");

    if (catalogo.total == 0) {
        printf("[AVISO] Nenhum produto em estoque para processamento matricial.\n");
        pausar_console();
        return;
    }

    int ano = 2026;
    ler_inteiro_seguro("Informe o ano de análise da movimentação (ex: 2026): ", &ano, 2000, 2099);

    // Matriz Bidimensional C11: Linhas = Produtos, Colunas = 12 Meses
    static int matriz_mov[MAX_PRODUTOS][MESES_ANO];

    if (!banco_carregar_matriz_movimentacoes(ano, matriz_mov, catalogo.itens, catalogo.total)) {
        printf("[ERRO] Falha ao processar matriz de movimentação.\n");
        pausar_console();
        return;
    }

    printf("\nAno Base: %d | Valores representam o Balanço Líquido Mensal (Entradas - Saídas em unidades)\n", ano);
    printf("\n%-18s | %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s | %6s\n",
           "PRODUTO", "JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ", "TOTAL");
    printf("----------------------------------------------------------------------------------------------------\n");

    int soma_meses[MESES_ANO] = {0};
    int balanco_geral = 0;

    for (int i = 0; i < catalogo.total; i++) {
        int saldo_anual_produto = 0;
        printf("%-18.18s | ", catalogo.itens[i].nome);
        for (int m = 0; m < MESES_ANO; m++) {
            int val = matriz_mov[i][m];
            printf("%4d ", val);
            saldo_anual_produto += val;
            soma_meses[m] += val;
        }
        printf("| %+6d\n", saldo_anual_produto);
        balanco_geral += saldo_anual_produto;
    }

    printf("----------------------------------------------------------------------------------------------------\n");
    printf("%-18s | ", "TOTAL LÍQ. DO MÊS");
    for (int m = 0; m < MESES_ANO; m++) {
        printf("%4d ", soma_meses[m]);
    }
    printf("| %+6d\n", balanco_geral);
    printf("====================================================================================================\n");

    pausar_console();
}

void sistema_historico_movimentacoes(void) {
    limpar_tela();
    printf("========================================================================================================\n");
    printf("                              CONSULTA DE HISTÓRICO DE MOVIMENTAÇÕES (AUDITORIA)                        \n");
    printf("========================================================================================================\n");

    printf("Opcoes de Consulta:\n");
    printf("  1. Ver histórico de um produto específico\n");
    printf("  2. Ver últimas movimentações de todos os produtos\n");
    int opc = 1;
    ler_inteiro_seguro("Opção (1 ou 2): ", &opc, 1, 2);

    int filtro_cod = 0;
    if (opc == 1) {
        ler_inteiro_seguro("Digite o código do produto para filtrar: ", &filtro_cod, 1, 999999);
    }

    banco_listar_historico_movimentacoes(filtro_cod);
    pausar_console();
}

void menu_principal(void) {
    int opcao = -1;

    while (opcao != 0) {
        limpar_tela();
        printf("===================================================================\n");
        printf("         SISTEMA DE CONTROLE DE ESTOQUE - ENGENHARIA DA COMPUTAÇÃO \n");
        printf("===================================================================\n");
        printf("  [CADASTRO & GERENCIAMENTO]\n");
        printf("    1. Cadastrar Novo Produto\n");
        printf("    2. Alterar Dados de Produto\n");
        printf("    3. Excluir Produto\n");
        printf("    4. Listar Todos os Produtos em Estoque\n\n");
        printf("  [ALGORITMOS EM MEMÓRIA RAM (DISCIPLINA)]\n");
        printf("    5. Pesquisar Produto por Código  [Busca Binária Manual O(log n)]\n");
        printf("    6. Pesquisar Produto por Nome    [Busca Linear Manual O(n)]\n");
        printf("    7. Ordenar Produtos Manualmente  [Insertion / Bubble / Selection]\n\n");
        printf("  [MOVIMENTAÇÕES & INDICADORES]\n");
        printf("    8. Registrar Entrada / Saída de Estoque (Transacional SQLite)\n");
        printf("    9. Relatório de Produtos com Baixo Estoque (Alerta)\n");
        printf("   10. Calcular Valor Total e Indicadores Financeiros\n");
        printf("   11. Matriz 2D de Movimentações Mensais (Produtos x 12 Meses)\n");
        printf("   12. Consultar Histórico de Movimentações / Auditoria\n\n");
        printf("    0. Sair do Sistema\n");
        printf("===================================================================\n");

        if (!ler_inteiro_seguro("Escolha uma opção (0-12): ", &opcao, 0, 12)) {
            continue;
        }

        switch (opcao) {
            case 1: sistema_cadastrar_produto(); break;
            case 2: sistema_alterar_produto(); break;
            case 3: sistema_excluir_produto(); break;
            case 4: sistema_listar_produtos(); break;
            case 5: sistema_pesquisar_por_codigo(); break;
            case 6: sistema_pesquisar_por_nome(); break;
            case 7: sistema_ordenar_produtos(); break;
            case 8: sistema_registrar_movimentacao(); break;
            case 9: sistema_alerta_estoque_baixo(); break;
            case 10: sistema_calcular_valor_total(); break;
            case 11: sistema_matriz_movimentacoes_mensais(); break;
            case 12: sistema_historico_movimentacoes(); break;
            case 0:
                printf("\nEncerrando a aplicação...\n");
                break;
            default:
                break;
        }
    }
}
