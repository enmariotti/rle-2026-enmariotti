# Nombre del ejecutable
TARGET = rle

# Directorios
SRC_DIR = src
INC_DIR = include
OBJ_DIR = build

# Archivos fuente
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# Archivos objeto correspondientes
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Compilador y flags 
CC = g++
CFLAGS = -I$(INC_DIR)

# Regla por defecto
all: $(TARGET)

# Regla para enlazar el ejecutable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

# Regla para compilar cada .cpp en .o (en obj/)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Crear el directorio obj si no existe
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Limpiar archivos generados
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Opción de recompilar todo desde cero
rebuild: clean all

# Evitar que "clean" y "rebuild" se interpreten como archivos
.PHONY: all clean rebuild
