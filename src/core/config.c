/*
 * config.c - parser do ficheiro de configuracao para uma arvore de nodes.
 *
 * Formato (um elemento por linha):
 *
 *   port: 3001                 chave + valor      -> N_SCALAR
 *   redis: {                   abre objeto        -> N_OBJECT
 *     host: localhost
 *   }                          fecha objeto
 *   gatekeeper: [              abre array         -> N_ARRAY
 *     {                        elemento (objeto sem chave)
 *     },
 *   ]                          fecha array
 *   # comentario               linha inteira
 *
 * Como esta organizado (de baixo para cima, cada camada so usa a anterior):
 *
 *   1. nodes        brook_config_node_new / add_child / free
 *   2. leitor       brook_config_read_lines   (ficheiro -> callback por linha)
 *   3. parser       parse_line             (linha -> altera a arvore)
 *   4. acesso       brook_config_get*         (arvore -> valores)
 */

#include "core/config.h"
#include "core/globals.h" /* logger, para o brook_debug */
#include "core/logger.h"
#include "core/types.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFG_MAX_DEPTH 32
#define CFG_LINE_MAX 1024

/* ========================================================================
 * 1. NODES
 * ======================================================================== */

/* Copia n bytes para memoria propria e acrescenta o '\0'. */
static char *dup_n(const char *src, size_t n) {
  char *out = malloc(n + 1); /* +1 para o '\0' */
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, src, n);
  out[n] = '\0';
  return out;
}

/*
 * Cria um node. key pode ser NULL (elemento de array ou raiz).
 * O node fica com copia propria da chave: nunca aponta para o buffer da linha.
 */
brook_config_node_t *brook_config_node_new(brook_config_node_type_t type, const char *key, size_t key_len) {
  brook_config_node_t *node = calloc(1, sizeof *node); /* tudo a zero/NULL */
  if (node == NULL) {
    return NULL;
  }

  node->type = type;

  if (key != NULL) {
    node->key = dup_n(key, key_len);
    if (node->key == NULL) {
      free(node);
      return NULL;
    }
  }

  return node;
}

/* Liga child a parent. O array de ponteiros cresce a dobrar (4, 8, 16...). */
int brook_config_node_add_child(brook_config_node_t *parent, brook_config_node_t *child) {
  if (parent->count == parent->cap) {
    size_t new_cap = parent->cap ? parent->cap * 2 : 4;
    brook_config_node_t **tmp = realloc(parent->children, new_cap * sizeof *tmp);
    if (tmp == NULL) {
      return BROOK_ERROR;
    }
    parent->children = tmp;
    parent->cap = new_cap;
  }

  parent->children[parent->count++] = child; /* guarda so o endereco */
  return BROOK_OK;
}

/* Liberta o node e, recursivamente, todos os filhos. */
void brook_config_node_free(brook_config_node_t *node) {
  if (node == NULL) {
    return;
  }

  for (size_t i = 0; i < node->count; i++) {
    brook_config_node_free(node->children[i]);
  }

  free(node->children);
  free(node->key);
  free(node->scalar);
  free(node);
}

/* ========================================================================
 * 2. LEITOR DE FICHEIRO
 * ======================================================================== */

/*
 * Le o ficheiro linha a linha e chama on_line para CADA linha, incluindo as
 * vazias (len == 0). O fim do ficheiro e o fgets devolver NULL, por isso uma
 * linha vazia nunca se confunde com EOF.
 *
 * `user` e passado ao callback sem ser tocado: e como o callback acede ao
 * estado do parser sem usar variaveis globais.
 */
int brook_config_read_lines(const char *path, brook_config_line_cb on_line, void *user) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    perror("config: fopen");
    return BROOK_ERROR;
  }

  char buf[CFG_LINE_MAX];
  int line_no = 0;
  int rc = BROOK_OK;

  while (fgets(buf, sizeof buf, fp) != NULL) {
    line_no++;

    size_t len = strcspn(buf, "\r\n"); /* posicao do fim de linha */

    /* Sem '\n' e sem ser o fim do ficheiro: a linha nao coube no buffer. */
    if (buf[len] == '\0' && len == sizeof buf - 1 && !feof(fp)) {
      fprintf(stderr, "config:%d: line too long (max %d)\n", line_no, CFG_LINE_MAX - 1);
      rc = BROOK_ERROR;
      break;
    }

    buf[len] = '\0';

    if (on_line(buf, len, line_no, user) == BROOK_ERROR) {
      rc = BROOK_ERROR;
      break;
    }
  }

  fclose(fp);
  return rc;
}

/* ========================================================================
 * 3. PARSER
 *
 * O callback e chamado linha a linha e nao se lembra de nada. O estado vive
 * aqui: uma PILHA com os nodes que estao "abertos". stack[depth] e o node onde
 * as proximas linhas vao ser penduradas.
 *
 *   linha            stack depois
 *   -----------      ------------------------
 *   (inicio)         [root]
 *   redis: {         [root, redis]
 *     host: x        [root, redis]          host pendurado em redis
 *   }                [root]
 * ======================================================================== */

typedef struct {
  brook_config_node_t *stack[CFG_MAX_DEPTH];
  int depth;
} cfg_parser_t;

static brook_config_node_t *current(cfg_parser_t *p) {
  return p->stack[p->depth];
}

static int cfg_error(int line_no, size_t col, const char *msg) {
  fprintf(stderr, "config:%d:%zu: %s\n", line_no, col + 1, msg);
  return BROOK_ERROR;
}

/* ---- helpers byte a byte: todos avancam o indice *i, nunca passam de len -- */

static int is_ws(char c) {
  return c == ' ' || c == '\t';
}

static int is_key_char(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static void skip_ws(const char *s, size_t len, size_t *i) {
  while (*i < len && is_ws(s[*i])) {
    (*i)++;
  }
}

/* Depois de um delimitador so podem restar espacos ate ao fim da linha. */
static int only_ws_left(const char *s, size_t len, size_t i) {
  skip_ws(s, len, &i);
  return i == len;
}

/* Abre um novo node filho do atual e passa a ser o atual. */
static int push(cfg_parser_t *p, brook_config_node_type_t type, const char *key, size_t key_len, int line_no) {
  if (p->depth + 1 >= CFG_MAX_DEPTH) {
    return cfg_error(line_no, 0, "nesting too deep");
  }

  brook_config_node_t *node = brook_config_node_new(type, key, key_len);
  if (node == NULL) {
    return cfg_error(line_no, 0, "out of memory");
  }

  if (brook_config_node_add_child(current(p), node) == BROOK_ERROR) {
    brook_config_node_free(node);
    return cfg_error(line_no, 0, "out of memory");
  }

  p->stack[++p->depth] = node;
  return BROOK_OK;
}

/* Cria um escalar (folha) no node atual. A pilha nao muda. */
static int add_scalar(cfg_parser_t *p, const char *key, size_t key_len, const char *val, size_t val_len, int line_no) {
  brook_config_node_t *node = brook_config_node_new(N_SCALAR, key, key_len);
  if (node == NULL) {
    return cfg_error(line_no, 0, "out of memory");
  }

  node->scalar = dup_n(val, val_len);
  if (node->scalar == NULL || brook_config_node_add_child(current(p), node) == BROOK_ERROR) {
    brook_config_node_free(node);
    return cfg_error(line_no, 0, "out of memory");
  }

  return BROOK_OK;
}

/*
 * Interpreta UMA linha. E aqui que esta toda a gramatica.
 * A regra: o tipo do node atual diz o que e permitido na linha.
 *
 *   dentro de N_OBJECT:  "chave: valor", "chave: {", "chave: [", "}"
 *   dentro de N_ARRAY:   "{", "]"
 */
static int parse_line(cfg_parser_t *p, const char *s, size_t len, int line_no) {
  size_t i = 0;
  skip_ws(s, len, &i);

  /* linha vazia ou comentario */
  if (i == len || s[i] == '#') {
    return BROOK_OK;
  }

  brook_config_node_t *cur = current(p);
  char c = s[i];

  /* ---- fecho: "}" ou "]" (com virgula opcional: "},") ---- */
  if (c == '}' || c == ']') {
    if (p->depth == 0) {
      return cfg_error(line_no, i, "unexpected closing delimiter");
    }

    char expected = (cur->type == N_ARRAY) ? ']' : '}';
    if (c != expected) {
      return cfg_error(line_no, i, c == '}' ? "'}' does not close an array" : "']' does not close an object");
    }

    i++;
    skip_ws(s, len, &i);
    if (i < len && s[i] == ',') {
      i++;
    }
    if (!only_ws_left(s, len, i)) {
      return cfg_error(line_no, i, "unexpected text after closing delimiter");
    }

    p->depth--;
    return BROOK_OK;
  }

  /* ---- "{" sozinho: elemento de array (objeto sem chave) ---- */
  if (c == '{') {
    if (cur->type != N_ARRAY) {
      return cfg_error(line_no, i, "'{' without key is only allowed inside an array");
    }
    if (!only_ws_left(s, len, i + 1)) {
      return cfg_error(line_no, i + 1, "unexpected text after '{'");
    }
    return push(p, N_OBJECT, NULL, 0, line_no);
  }

  /* ---- a partir daqui a linha e "chave: ..." ---- */
  if (cur->type != N_OBJECT) {
    return cfg_error(line_no, i, "inside an array only '{' and ']' are allowed");
  }

  /* 1. chave */
  size_t key_start = i;
  while (i < len && is_key_char(s[i])) {
    i++;
  }
  size_t key_len = i - key_start;
  if (key_len == 0) {
    return cfg_error(line_no, i, "expected a key");
  }

  /* 2. ':' */
  skip_ws(s, len, &i);
  if (i == len || s[i] != ':') {
    return cfg_error(line_no, i, "expected ':' after key");
  }
  i++;

  /* 3. o que vem depois decide o tipo */
  skip_ws(s, len, &i);
  if (i == len) {
    return cfg_error(line_no, i, "missing value after ':'");
  }

  if (s[i] == '{' || s[i] == '[') {
    brook_config_node_type_t type = (s[i] == '{') ? N_OBJECT : N_ARRAY;
    if (!only_ws_left(s, len, i + 1)) {
      return cfg_error(line_no, i + 1, "unexpected text after opening delimiter");
    }
    return push(p, type, s + key_start, key_len, line_no);
  }

  /* 4. valor simples: do indice atual ate ao fim, sem espacos finais */
  size_t val_start = i;
  size_t val_end = len;
  while (val_end > val_start && is_ws(s[val_end - 1])) {
    val_end--;
  }

  /* Delimitadores no meio do valor => o user tentou por varias coisas numa linha */
  for (size_t k = val_start; k < val_end; k++) {
    if (s[k] == '{' || s[k] == '}' || s[k] == '[' || s[k] == ']') {
      return cfg_error(line_no, k, "only one element per line");
    }
  }

  return add_scalar(p, s + key_start, key_len, s + val_start, val_end - val_start, line_no);
}

/* Adaptador: assinatura exigida por brook_config_read_lines -> parse_line. */
static int on_line(char *line, size_t len, int line_no, void *user) {
  return parse_line((cfg_parser_t *)user, line, len, line_no);
}

/*
 * Ponto de entrada: ficheiro -> arvore.
 * A raiz e um N_OBJECT sem chave, como se o ficheiro estivesse dentro de "{ }".
 */
brook_config_node_t *brook_config_load(const char *path) {
  brook_config_node_t *root = brook_config_node_new(N_OBJECT, NULL, 0);
  if (root == NULL) {
    return NULL;
  }

  cfg_parser_t parser = {.depth = 0};
  parser.stack[0] = root;

  if (brook_config_read_lines(path, on_line, &parser) == BROOK_ERROR) {
    brook_config_node_free(root);
    return NULL;
  }

  /* No fim tudo tem de estar fechado: depth volta a 0. */
  if (parser.depth != 0) {
    fprintf(stderr, "config: unexpected end of file, %d block(s) not closed\n", parser.depth);
    brook_config_node_free(root);
    return NULL;
  }

  return root;
}

/* ========================================================================
 * 4. ACESSO AOS VALORES
 * ======================================================================== */

/*
 * Primeiro filho com essa chave. Se a chave se repetir (ex: dois "server"),
 * devolve o primeiro; para os percorrer todos usa brook_config_find_next.
 */
const brook_config_node_t *brook_config_get(const brook_config_node_t *obj, const char *key) {
  size_t cursor = 0;
  return brook_config_find_next(obj, key, &cursor);
}

/*
 * Iterador para chaves repetidas:
 *
 *   size_t cur = 0;
 *   const brook_config_node_t *srv;
 *   while ((srv = brook_config_find_next(root, "server", &cur)) != NULL) { ... }
 */
const brook_config_node_t *brook_config_find_next(const brook_config_node_t *obj, const char *key, size_t *cursor) {
  if (obj == NULL) {
    return NULL;
  }

  for (; *cursor < obj->count; (*cursor)++) {
    const brook_config_node_t *child = obj->children[*cursor];
    if (child->key != NULL && strcmp(child->key, key) == 0) {
      (*cursor)++; /* proxima chamada continua depois deste */
      return child;
    }
  }

  return NULL;
}

const char *brook_config_get_str(const brook_config_node_t *obj, const char *key, const char *def) {
  const brook_config_node_t *n = brook_config_get(obj, key);
  if (n == NULL || n->type != N_SCALAR) {
    return def;
  }
  return n->scalar;
}

int brook_config_get_int(const brook_config_node_t *obj, const char *key, int def) {
  const char *s = brook_config_get_str(obj, key, NULL);
  if (s == NULL) {
    return def;
  }

  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0') { /* nao e um numero inteiro valido */
    return def;
  }
  return (int)v;
}

/* ========================================================================
 * DEPURACAO
 * ======================================================================== */

void brook_config_print(const brook_config_node_t *node, int indent) {
  printf("%*s", indent * 2, "");

  if (node->key != NULL) {
    printf("%s: ", node->key);
  }

  switch (node->type) {
  case N_SCALAR:
    printf("%s\n", node->scalar);
    break;
  case N_OBJECT:
    printf("{\n");
    break;
  case N_ARRAY:
    printf("[\n");
    break;
  }

  if (node->type != N_SCALAR) {
    for (size_t i = 0; i < node->count; i++) {
      brook_config_print(node->children[i], indent + 1);
    }
    printf("%*s%c\n", indent * 2, "", node->type == N_OBJECT ? '}' : ']');
  }
}

#ifdef BROOK_DEBUG

#define CFG_PATH_MAX 256

/*
 * Um brook_debug por node. O caminho junta as chaves com '.', e os elementos
 * de array levam o indice:  server.gatekeeper[1]
 */
static void debug_node(const brook_config_node_t *node, const char *path) {
  if (node->type == N_SCALAR) {
    brook_debug("config %s = %s\n", path, node->scalar);
    return;
  }

  brook_debug("config %s %s with %zu item(s)\n", path, node->type == N_OBJECT ? "object" : "array", node->count);

  for (size_t i = 0; i < node->count; i++) {
    const brook_config_node_t *child = node->children[i];
    char child_path[CFG_PATH_MAX];

    if (node->type == N_ARRAY) {
      snprintf(child_path, sizeof child_path, "%s[%zu]", path, i);
    } else {
      snprintf(child_path, sizeof child_path, "%s.%s", path, child->key);
    }

    debug_node(child, child_path);
  }
}

#endif

/*
 * Escreve no log cada chave e valor da arvore.
 * Fora do load: so pode ser chamada depois de brook_logger() ter aberto o log.
 * Sem -DBROOK_DEBUG nao faz nada.
 */
void brook_config_debug(const brook_config_node_t *root) {
#ifdef BROOK_DEBUG
  /* A raiz nao tem chave: o caminho comeca nas chaves dos filhos */
  for (size_t i = 0; i < root->count; i++) {
    debug_node(root->children[i], root->children[i]->key);
  }
#else
  (void)root;
#endif
}
