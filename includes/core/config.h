#ifndef BROOK_CONFIG_H
#define BROOK_CONFIG_H

#include "core/types.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  N_SCALAR,
  N_OBJECT,
  N_ARRAY
} brook_config_node_type_t;

typedef struct brook_config_node_s {
  brook_config_node_type_t type;
  char *key;                             // NULL se for elemento de array
  char *scalar;                          // N_SCALAR
  struct brook_config_node_s **children; // N_OBJECT / N_ARRAY
  size_t count, cap;
} brook_config_node_t;

/*
 * Camada 1 - nodes (a estrutura de dados)
 */
brook_config_node_t *brook_config_node_new(brook_config_node_type_t type, const char *key, size_t key_len);
int brook_config_node_add_child(brook_config_node_t *parent, brook_config_node_t *child);
void brook_config_node_free(brook_config_node_t *node);

/*
 * Camada 2 - leitura do ficheiro, linha a linha
 */
typedef int (*brook_config_line_cb)(char *line, size_t len, int line_no, void *user);
int brook_config_read_lines(const char *path, brook_config_line_cb on_line, void *user);

/*
 * Camada 3 - parser (linhas -> arvore). Ponto de entrada.
 * Devolve a raiz (um N_OBJECT sem chave) ou NULL em caso de erro.
 * Quem chama fica dono da arvore e liberta com brook_config_node_free.
 */
brook_config_node_t *brook_config_load(const char *path);

/*
 * Camada 4 - acesso aos valores
 */
const brook_config_node_t *brook_config_get(const brook_config_node_t *obj, const char *key);
const brook_config_node_t *brook_config_find_next(const brook_config_node_t *obj, const char *key, size_t *cursor);
const char *brook_config_get_str(const brook_config_node_t *obj, const char *key, const char *def);
int brook_config_get_int(const brook_config_node_t *obj, const char *key, int def);

/* Depuracao: imprime a arvore com indentacao */
void brook_config_print(const brook_config_node_t *node, int indent);

/* Um brook_debug por chave/valor. Chamar so depois de brook_logger(). */
void brook_config_debug(const brook_config_node_t *root);
#endif
