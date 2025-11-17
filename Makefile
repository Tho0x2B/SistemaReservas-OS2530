# ============================================================
# Makefile para el proyecto Sistema de Reservas 
# Compila: Controlador y Agente
# Lenguaje: C (POSIX), Linux
# ============================================================

# Compilador
CC = gcc

# Flags de compilación
CFLAGS = -Wall -Wextra -pthread

# Directorios
DIR_CONTROLADOR = controlador
DIR_AGENTE = agente

# Archivos del Controlador
CONTROLADOR_SRC = $(DIR_CONTROLADOR)/main.c \
                   $(DIR_CONTROLADOR)/controlador.c

CONTROLADOR_OUT = controlador

# Archivos del Agente
AGENTE_SRC = $(DIR_AGENTE)/main.c \
              $(DIR_AGENTE)/agente.c

AGENTE_OUT = agente

# ======================
#  Targets principales
# ======================

all: $(CONTROLADOR_OUT) $(AGENTE_OUT)

# ----------------------
#  Compilar Controlador
# ----------------------
$(CONTROLADOR_OUT): $(CONTROLADOR_SRC)
	$(CC) $(CFLAGS) -o $(CONTROLADOR_OUT) $(CONTROLADOR_SRC)

# -------------------
#  Compilar Agente
# -------------------
$(AGENTE_OUT): $(AGENTE_SRC)
	$(CC) $(CFLAGS) -o $(AGENTE_OUT) $(AGENTE_SRC)

# ======================
#  Limpieza
# ======================
clean:
	rm -f $(CONTROLADOR_OUT) $(AGENTE_OUT)

cleanall: clean
	rm -f pipeGeneral
	rm -f *.o

# ======================
# Ayuda
# ======================
help:
	@echo "Comandos disponibles:"
	@echo "  make            --> Compila Controlador y Agente"
	@echo "  make controlador --> Compila solo el Controlador"
	@echo "  make agente      --> Compila solo el Agente"
	@echo "  make clean       --> Borra ejecutables"
	@echo "  make cleanall    --> Borra ejecutables y pipes"
