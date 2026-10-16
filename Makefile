# 1. Identificar o Sistema Operativo
UNAME_S := $(shell uname -s)

# 2. Configurações base
TARGET = brook
CC = gcc
SRC_DIR = src
INC_DIR = includes
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Flags base
CFLAGS = -I$(INC_DIR) -Wall -Wextra -O2 -Wall -W -pedantic -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wredundant-decls -Wnested-externs -Winline -Wlong-long
LDFLAGS = -lcjson -lbeanstalkclient -lhiredis

# 3. Ajustes específicos por OS
ifeq ($(UNAME_S), Darwin)
    HOMEBREW_PREFIX = /opt/homebrew
    CFLAGS += -I$(HOMEBREW_PREFIX)/include
    LDFLAGS += -L$(HOMEBREW_PREFIX)/lib
else
    CFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
endif

# 4. Localizar ficheiros
SRC = $(shell find $(SRC_DIR) -name "*.c")
# Mudar o destino dos objetos para a pasta build/obj
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# 5. Regras de Build
all: $(TARGET)

# Linkagem final
$(TARGET): $(OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJ) -o $(BUILD_DIR)/$(TARGET) $(LDFLAGS)
	@echo "Build concluído: $(BUILD_DIR)/$(TARGET)"

# Compilação dos objetos (cria subpastas se necessário)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 6. Instalação
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

install: $(TARGET)
	@echo "A instalar em $(BINDIR)..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	@echo "A limpar ficheiros temporários..."
	rm -rf $(BUILD_DIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
