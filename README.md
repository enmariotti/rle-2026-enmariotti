# Planar RLE — Compresor de imágenes BMP

Compresor y descompresor de imágenes BMP color de 24 bits, basado en
**Run-Length Encoding planar con tres hilos independientes**.

---

## Algoritmo

### Separación de canales (planar)

El formato BMP almacena los píxeles intercalados en orden BGR. El encoder
separa la imagen en tres planos independientes — R, G, B — antes de comprimir.
Esto permite que zonas de color uniforme generen runs muy largos dentro de
cada canal, mejorando el ratio respecto a operar sobre píxeles BGR completos.

### Compresión RLE por canal

Cada canal se comprime como un stream lineal de tokens. Hay cuatro tipos de
token, seleccionados por los dos bits más significativos del byte de control:

```
bits 7..6   tipo              formato
─────────────────────────────────────────────────────────────────────────
  00        Run corto         [00 cccccc][value]               count 1..63
  01        Run largo         [01 CCCCCC][cccccccc][value]     count 64..16447
  10        Literal corto     [10 cccccc][v0 .. vN]            count 1..63
  11        Literal largo     [11 CCCCCC][cccccccc][v0 .. vN]  count 64..16447
```

- **Run:** N píxeles consecutivos con el mismo valor. Se emite un único token.
- **Literal:** N píxeles consecutivos con valores distintos. Se emiten todos los valores después del byte de control.
- Los runs y literales largos usan 14 bits de count con un offset de +64, alcanzando hasta 16.447 píxeles por token.
- Runs mayores a 16.447 se emiten como runs consecutivos del mismo valor.
- `count = 0` nunca es emitido. Tener en cuenta que en casos de run largos o literales largos, existe un offset de +64.

### Decisiones de codificación

| Situación | Decisión |
|---|---|
| Run ≥ 3 píxeles | Emitir como run (siempre rentable) |
| Run de 2, sin literal abierto | Emitir como run (2 bytes vs 3 del literal) |
| Run de 2, con literal abierto | Absorber en el literal (evita cerrar y reabrir) |
| Píxel único | Acumular en buffer de literales |

### Concurrencia

Los tres canales no tienen dependencias entre sí. El encoder y el decoder
lanzan un hilo por canal y sincronizan con `join` al finalizar.

```
separar canales (1 hilo)
        │
   ┌────┴────┐
   │    │    │
 hilo R hilo G hilo B
   │    │    │
   └────┬────┘
        │
  join + ensamblar (1 hilo)
```

---

## Formato del archivo `.prle`

```
Offset  Tamaño  Campo
──────────────────────────────────────────
  0       4     Identificador  (0xCAFECAFE)
  4       1     Versión        (0x01)
  5       4     Ancho en píxeles
  9       4     Alto en píxeles
 13       8     Offset canal R en el archivo
 21       4     Tamaño canal R comprimido
 25       8     Offset canal G en el archivo
 33       4     Tamaño canal G comprimido
 37       8     Offset canal B en el archivo
 45       4     Tamaño canal B comprimido
 49       -     Datos R | Datos G | Datos B
```

Total del header: **49 bytes**. Todos los valores en little-endian.

---

## Estructura del proyecto

```
.
├── include/
│   ├── rle.hpp          # Declaración de la clase RLE
│   └── CLI11.hpp        # Parser de argumentos (header-only)
├── src/
│   ├── rle.cpp          # Implementación del encoder/decoder
│   └── main.cpp         # Punto de entrada y CLI
├── Makefile
└── README.md
```

---

## Compilación

### Requisitos

- `g++` con soporte C++17 o superior
- `make`

### Compilar

```bash
make
```

Genera el ejecutable `rle` en el directorio raíz.

### Recompilar desde cero

```bash
make rebuild
```

### Limpiar archivos generados

```bash
make clean
```

---

## Uso

```
rle --encode <entrada.bmp> <salida.prle>
rle --decode <entrada.prle> <salida.bmp>
```

### Comprimir una imagen BMP

```bash
./rle --encode imagen.bmp imagen.prle
```

Salida de ejemplo:

```
Leyendo BMP...
  2048×2048 pixeles (12582912 bytes)
Comprimiendo canales (3 hilos)...
  Canal R: 81125 bytes
  Canal G: 81128 bytes
  Canal B: 81144 bytes
Estadisticas...
  Total entrada:  12582912 bytes
  Total salida:   243446 bytes
  Ratio:          51.69:1
```

### Descomprimir un archivo PRLE

```bash
./rle --decode imagen.prle imagen_recuperada.bmp
```

Salida de ejemplo:

```
Leyendo PRLE...
  2048×2048 pixeles (12582912 bytes sin comprimir)
  Canal R: 81125 bytes comprimidos
  Canal G: 81128 bytes comprimidos
  Canal B: 81144 bytes comprimidos
Descomprimiendo canales (3 hilos)...
Estadisticas...
  Total comprimido:    243446 bytes
  Total descomprimido: 12582912 bytes
Listo.
```

### Limitaciones

- Solo se soportan imágenes BMP de 24 bits sin compresión (`BI_RGB`).
- El formato `.prle` es específico de este proyecto y no es compatible con otros programas.
