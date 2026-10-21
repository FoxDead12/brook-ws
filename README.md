# Brook WS

Servidor WebSocket escrito em C, construído sobre sockets POSIX e um event loop com `poll()`.

> **Estado do projeto: fase inicial.** Neste momento o servidor aceita ligações TCP e lê dados em bruto.
> O protocolo WebSocket (handshake, frames), os workers múltiplos e a gestão de buffers ainda não estão implementados. Veja [Estado atual](#estado-atual) e [Roadmap](#roadmap).

## Requisitos

- `gcc` e `make`
- macOS ou Linux (em macOS assume Homebrew em `/opt/homebrew`)
- Bibliotecas ligadas pelo `Makefile`:
  - `cjson`
  - `beanstalkclient`
  - `hiredis`

> Estas bibliotecas já são ligadas no build, mas ainda não são usadas pelo código.

## Compilar e executar

```sh
make            # build normal -> build/brook
make debug      # build com -g -O0 e logs de debug -> build-debug/brook
make clean      # remove build/ e build-debug/
make install    # instala em /usr/local/bin (PREFIX e DESTDIR configuráveis)
make uninstall
```

O binário lê o ficheiro `brook-config.conf` a partir do **diretório atual**, por isso deve ser executado a partir da raiz do projeto:

```sh
./build/brook
```

Para testar a ligação (o servidor escuta apenas em `127.0.0.1`):

```sh
nc 127.0.0.1 3001
```

O que for enviado é impresso no stdout do servidor.

## Configuração

Ficheiro `brook-config.conf`, uma chave por linha no formato `chave: valor`:

```
port: 3001
workers: 5
logger: /Users/utilizador/Library/Logs/BrookWs
```

| Chave     | Descrição                                                         | Estado                                   |
|-----------|-------------------------------------------------------------------|------------------------------------------|
| `port`    | Porta TCP onde o servidor escuta                                  | Em uso                                   |
| `workers` | Número de processos worker                                        | Lido, mas ainda não usado (1 processo)   |
| `logger`  | Diretório onde são criados os ficheiros de log                    | Em uso                                   |

Limitações do parser atual:

- Linhas inválidas ou chaves desconhecidas terminam o arranque com erro.
- Valores só podem conter letras, números, `_` e `/`.
- Não há suporte para comentários nem linhas em branco.
- O `brook-config.conf` está no `.gitignore` por ser específico de cada máquina (o `logger` é um caminho absoluto), mas já está versionado. Para o deixar de versionar: `git rm --cached brook-config.conf`.

## Logs

Os logs são escritos em `<logger>/brook-ws-AAAA-MM-DD.log`. O diretório é criado se não existir (apenas um nível) e o ficheiro muda automaticamente quando o dia muda.

Formato de cada linha:

```
[DD-MM-AAAATHH:MM:SS][pid][NIVEL] mensagem
```

Níveis: `DEBUG`, `VERBOSE`, `INFO`, `WARN`, `ERROR`.

Os logs `DEBUG` (macro `brook_debug(...)`) só existem quando compilado com `-DBROOK_DEBUG`. Isto acontece em `make debug` e sempre em macOS. Em builds normais a chamada, incluindo os argumentos, é removida pelo pré-processador.

## Arquitetura

### Estrutura de ficheiros

```
src/
  brook.c              # main: config -> logger -> socket -> processo
  core/
    config.c           # parser do brook-config.conf
    logger.c           # logger com rotação diária
    socket.c           # criação do socket de escuta e accept
    process.c          # event loop (poll) de um worker
    connection.c       # accept e leitura de ligações
    pool.c             # pool de memória em blocos
includes/core/         # headers correspondentes
                       #  + types.h (códigos de retorno) e globals.h (estado global)
```

### Fluxo de arranque

1. `brook_config()` lê o `brook-config.conf`.
2. `brook_logger()` abre o ficheiro de log do dia.
3. `brook_socket()` cria o socket de escuta: IPv4, `127.0.0.1`, não bloqueante, com `SO_REUSEPORT`, `SO_REUSEADDR` e `TCP_NODELAY`. Também ignora `SIGPIPE`.
4. `brook_process()` inicializa o pool de memória e entra no event loop.

### Event loop

Cada processo mantém três arrays paralelos de tamanho `POLL_FDS_MAX` (1024), indexados pelo mesmo slot:

- `fds`: os `struct pollfd` passados ao `poll()`
- `handlers`: callbacks `on_read`, `on_write`, `on_error` e um `ctx`
- `conns`: dados de cada ligação (`fd`, `ip`, `port`)

O slot 0 é sempre o socket de escuta, com `brook_connection_accept` como handler de leitura. Quando um evento chega, o loop chama o handler correspondente. Se o processo já tem `POLL_FDS_MAX` descritores, o `accept` é ignorado.

### Pool de memória

`brook_pool_init()` aloca de uma só vez `BROOK_POOL_BLOCKS_AMOUNT` (256) blocos de `BROOK_POOL_BLOCK_SIZE` (4096) bytes, ou seja, 1 MiB, e cria uma lista ligada (`free`) de `brook_buffer_t` que apontam para cada bloco. Ainda não existe função para obter ou devolver buffers.

### Códigos de retorno

Definidos em `types.h`: `BROOK_OK` (0), `BROOK_ERROR` (-1) e `BROOK_DONE` (-2).

## Estado atual

Implementado:

- [x] Parser do ficheiro de configuração
- [x] Logger com níveis e rotação diária
- [x] Socket TCP não bloqueante
- [x] Event loop com `poll()` e handlers por descritor
- [x] Aceitar ligações (até 1024 descritores por processo)
- [x] Estrutura do pool de memória (alocação inicial)

Limitações conhecidas:

- Corre um único processo; `workers` é ignorado.
- Só escuta em `127.0.0.1`.
- A leitura (`brook_connection_read`) lê no máximo 1024 bytes e faz `printf`. Não trata fecho de ligação nem remove o slot.
- Os handlers `on_write` e `on_error` não estão ligados. O tratamento de `POLLERR`/`POLLHUP`/`POLLNVAL` está comentado.
- O pool não tem funções de acquire/release e ainda não é usado pelas ligações.
- Não há handshake nem frames WebSocket.

## Roadmap

- [ ] Handshake HTTP/WebSocket (`Upgrade`, `Sec-WebSocket-Accept`)
- [ ] Parsing e escrita de frames WebSocket
- [ ] Fechar ligações e libertar slots
- [ ] Usar o pool de buffers nas ligações
- [ ] Múltiplos workers (`workers` + `SO_REUSEPORT`)
- [ ] Tratamento de erros e escrita assíncrona (`on_write`, `on_error`)

## Estilo de código

O formato é definido em `.clang-format`. Os warnings do compilador estão ativos e são exigentes (`-Wall -Wextra -pedantic -Wshadow -Wstrict-prototypes ...`). Mantenha o build sem warnings.
