"""
generate_test_bmps.py — Genera BMPs sinteticos para testing.

Uso:
    python3 generate_test_bmps.py [directorio_salida]

    Si no se especifica directorio, los archivos se crean en el directorio actual.

Casos generados:
    01_flat_white.bmp         — Fondo completamente blanco.
    02_flat_color.bmp         — Fondo de un color solido no trivial.
    03_noise.bmp              — Ruido aleatorio puro.
    04_flat_with_noise.bmp    — Fondo plano con zona de ruido central.
    05_gradient.bmp           — Gradiente horizontal suave.
    06_stripes_h.bmp          — Franjas horizontales.
    07_stripes_v.bmp          — Franjas verticales.
    08_checkerboard.bmp       — Tablero de ajedrez.
    09_single_pixel_noise.bmp — Píxeles aislados intercalados con fondo.
    10_large_run_boundary.bmp — Fondo que fuerza runs exactamente en los limites 63/64/16447
"""

import os
import random
import struct
import sys
import argparse

def make_bmp(width: int, height: int, pixels_bgr: bytes) -> bytes:
    """_summary_ Construye un BMP 24-bit top-down (height negativo) a partir de un buffer
                 de píxeles en formato BGR, fila por fila, sin padding interno.
                 Agrega el padding de fila a 4 bytes.

    Args:
        width (int): _description_ es el ancho de la imagen.
        height (int): _description_ es la altura de la imagen.
        pixels_bgr (bytes): _description_ es la cadena de bytes representando el payload de la imagen.

    Returns:
        bytes: _description_ es la cadena de bytes de la imagen BMP.
    """

    row_raw    = width * 3
    pad        = (4 - row_raw % 4) % 4
    row_stride = row_raw + pad # ((row_raw + 3) //4) * 4 o alternativamente operacion bitwise

    #     <   → little-endian
    # I   → unsigned int  (4 bytes)
    # i   → signed int    (4 bytes)
    # i   → signed int    (4 bytes)
    # H   → unsigned short (2 bytes)
    # H   → unsigned short (2 bytes)
    # I   → unsigned int  (4 bytes)
    # I   → unsigned int  (4 bytes)
    # i   → signed int    (4 bytes)
    # i   → signed int    (4 bytes)
    # I   → unsigned int  (4 bytes)
    # I   → unsigned int  (4 bytes)
    dib = struct.pack('<IiiHHIIiiII',
        40,                  # tamaño DIB header
        width, -height,      # negativo = top-down
        1,                   # planes
        24,                  # bpp
        0,                   # BI_RGB, sin compresión
        row_stride * height, # tamaño datos
        2835, 2835,          # píxeles por metro (~72 dpi)
        0, 0)                # solo importa para imagenes de 8-bit / pixel

    pixel_offset = 14 + 40
    file_size    = pixel_offset + row_stride * height

    file_header  = b'BM'                        # Numero magico BMP
    #     <   → little-endian
    # I   → unsigned int  (4 bytes)
    # H   → unsigned short (2 bytes)
    # H   → unsigned short (2 bytes)
    # I   → unsigned int  (4 bytes)
    file_header += struct.pack('<IHHI',
                                file_size,      # tamaño en bytes del archivo de salida
                                0,              # Reservado
                                0,              # Reservado
                                pixel_offset)   # Data offset

    rows = bytearray()
    for row in range(height):
        row_data = pixels_bgr[row * row_raw : (row + 1) * row_raw] # Del comienzo de la fila (inclusive) hasta el comienzo de la siguiente fila (exlusive)
        rows += row_data
        rows += bytes(pad)

    return bytes(file_header) + bytes(dib) + bytes(rows)


def save_bmp(path: str, width: int, height: int, pixels_bgr: bytes):
    """_summary_ Guardar imagen con extension .bmp

    Args:
        path (str): _description_
        width (int): _description_
        height (int): _description_
        pixels_bgr (bytes): _description_
    """
    with open(path, 'wb') as f:
        f.write(make_bmp(width, height, pixels_bgr))
    size = os.path.getsize(path)
    print(f"  {os.path.basename(path):40s}  {width}x{height}  {size:>12,} bytes")


def solid_bgr(width: int, height: int, b: int, g: int, r: int) -> bytes:
    """_summary_ Generar background solido.

    Args:
        width (int): _description_ es el ancho de la imagen.
        height (int): _description_ es el alto de la imagen.
        b (int): _description_ es el canal azul.
        g (int): _description_ es el canal verde.
        r (int): _description_ es el canal rojo.

    Returns:
        bytes: _description_ es la cadena de bytes resultante (orden BGR).
    """
    return bytes([b, g, r] * (width * height))


def case_01_flat_white(out_dir: str, W: int, H: int):
    """_summary_ Generar background blanco.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    pixels = solid_bgr(W, H, 255, 255, 255)
    save_bmp(os.path.join(out_dir, '01_flat_white.bmp'), W, H, pixels)


def case_02_flat_color(out_dir: str, W: int, H: int):
    """_summary_ Generar background verde azulado.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    pixels = solid_bgr(W, H, 120, 200, 50)
    save_bmp(os.path.join(out_dir, '02_flat_color.bmp'), W, H, pixels)


def case_03_noise(out_dir: str, W: int, H: int):
    """_summary_ Generar ruido aleatorio puro.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    rng = random.Random(14) # seed
    pixels = bytes(rng.randint(0, 255) for _ in range(W * H * 3))
    save_bmp(os.path.join(out_dir, '03_noise.bmp'), W, H, pixels)


def case_04_flat_with_noise(out_dir: str, W: int, H: int):
    """_summary_ Fondo plano (120,200,50) con zona de ruido 200×200 en el centro.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    rng = random.Random(14) #seed
    cx, cy   = W // 2, H // 2
    nr_start = cy - 100;  nr_end = cy + 100
    nc_start = cx - 100;  nc_end = cx + 100

    pixels  = bytearray(W * H * 3)
    for row in range(H):
        for col in range(W):
            base = (row * W + col) * 3
            if nr_start <= row < nr_end and nc_start <= col < nc_end:
                pixels[base]   = rng.randint(0, 255)
                pixels[base+1] = rng.randint(0, 255)
                pixels[base+2] = rng.randint(0, 255)
            else:
                pixels[base]   = 120
                pixels[base+1] = 200
                pixels[base+2] = 50

    save_bmp(os.path.join(out_dir, '04_flat_with_noise.bmp'), W, H, bytes(pixels))


def case_05_gradient(out_dir: str, W: int, H: int):
    """_summary_ Gradiente horizontal lineal: R va de 0 a 255 de izquierda a derecha. G y B constantes.
    
    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    pixels = bytearray(W * H * 3)
    for row in range(H):
        for col in range(W):
            base          = (row * W + col) * 3
            r_val         = int(col * 255 / (W - 1)) # variacion lineal
            pixels[base]  = 50       # B constante
            pixels[base+1]= 100      # G constante
            pixels[base+2]= r_val    # R gradiente
    save_bmp(os.path.join(out_dir, '05_gradient.bmp'), W, H, bytes(pixels))


def case_06_stripes_h(out_dir: str, W: int, H: int):
    """_summary_ Franjas horizontales de 64 píxeles de alto, alternando blanco y negro.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    pixels = bytearray(W * H * 3)
    for row in range(H):
        val  = 255 if (row // 64) % 2 == 0 else 0
        base = row * W * 3
        for col in range(W):
            pixels[base + col*3]   = val
            pixels[base + col*3+1] = val
            pixels[base + col*3+2] = val
    save_bmp(os.path.join(out_dir, '06_stripes_h.bmp'), W, H, bytes(pixels))


def case_07_stripes_v(out_dir: str, W: int, H: int):
    """_summary_ Franjas verticales de 64 píxeles de alto, alternando blanco y negro.

    Args:
        out_dir (str): _description_ es el directorio de salida
        W (int): _description_ es el ancho de la imagen.
        H (int): _description_ es el alto de la imagen.
    """
    pixels = bytearray(W * H * 3)
    for row in range(H):
        for col in range(W):
            val  = 255 if (col // 64) % 2 == 0 else 0
            base = (row * W + col) * 3
            pixels[base]   = val
            pixels[base+1] = val
            pixels[base+2] = val
    save_bmp(os.path.join(out_dir, '07_stripes_v.bmp'), W, H, bytes(pixels))

def case_08_checkerboard(out_dir: str, W: int, H: int):
    """_summary_ Tablero de ajedrez de 1×1 pixel. Maxima alternancia.

    Args:
        out_dir (str): _description_
        W (int): _description_
        H (int): _description_
    """
    pixels = bytearray(W * H * 3)
    for row in range(H):
        for col in range(W):
            val  = 255 if (row + col) % 2 == 0 else 0
            base = (row * W + col) * 3
            pixels[base]   = val
            pixels[base+1] = val
            pixels[base+2] = val
    save_bmp(os.path.join(out_dir, '08_checkerboard.bmp'), W, H, bytes(pixels))


def case_09_single_pixel_noise(out_dir: str, W: int, H: int):
    """_summary_ Fondo plano con pixeles aislados de color distinto cada 10 posiciones.

    Args:
        out_dir (str): _description_
        W (int): _description_
        H (int): _description_
    """
    rng    = random.Random(7)
    pixels = bytearray(solid_bgr(W, H, 180, 180, 180))
    total  = W * H
    for i in range(0, total, 10):
        base           = i * 3
        pixels[base]   = rng.randint(0, 100)
        pixels[base+1] = rng.randint(0, 100)
        pixels[base+2] = rng.randint(0, 100)
    save_bmp(os.path.join(out_dir, '09_single_pixel_noise.bmp'), W, H, bytes(pixels))

def case_10_large_run_boundary(out_dir: str, W: int, H: int):
    """_summary_     Imagen diseñada para forzar runs exactamente en los limites del formato:
                    - Segmento A: 63 pixeles    → debe usar run corto
                    - Segmento B: 64 pixeles    → debe usar run largo
                    - Segmento C: 16447 pixeles → run largo maximo
                    - Segmento D: 16448 pixeles → debe partirse en dos runs

        Args:
        out_dir (str): _description_
        W (int): _description_
        H (int): _description_
    """
    pixels  = bytearray(solid_bgr(W, H, 200, 200, 200))
    # Escribir la secuencia en el canal R (byte offset +2 en BGR)
    offsets = [
        (0,      63,    0xFF),   # run corto max
        (63,     127,   0x01),   # run largo min (64 pixeles)
        (127,    16574, 0xAA),   # run largo max (16447 pixeles)
        (16574,  33022, 0x55),   # run que debe partirse (16448 pixeles)
    ]
    for start, end, val in offsets:
        for i in range(start, min(end, W * H)):
            pixels[i * 3 + 2] = val    # canal R en BGR

    save_bmp(os.path.join(out_dir, '10_large_run_boundary.bmp'), W, H, bytes(pixels))

def case_11_debugging(out_dir: str, W: int, H: int):
    """_summary_     Imagen diseñada para testear a mano el formato de codificacion:
                    - Segmento A: 63 pixeles    → literal corto
                    - Segmento B: 63 pixeles    → run corto
                    - Segmento B: 64 pixeles    → run largo
                    - Segmento D: 64 pixeles    → literal largo

        Args:
        out_dir (str): _description_
        W (int): _description_
        H (int): _description_
    """
    pixels  = bytearray(solid_bgr(W, H, 0, 0, 0))
    # Escribir la secuencia en el canal B (byte offset +0 en BGR)
    offsets = [
        (0,      63,    0x00),   # literal corto max (63 pixeles)
        (63,     126,   0xDD),   # run corto max (63 pixeles)
        (126,    190,   0x00),   # literal largo min (64 pixeles)
        (190,    315,   0xAA),   # run largo min (64 pixeles)
    ]
    for start, end, val in offsets:
        for i in range(start, min(end, W * H)):
            if start == 63 or start == 190:
                pixels[i * 3 + 0] = val    # canal B en BGR
            elif start == 0 or start == 126:
                value  = 255 if i % 2 == 0 else 0
                pixels[i * 3 + 0] = value  # canal B en BGR
    save_bmp(os.path.join(out_dir, '11_small_debugging.bmp'), W, H, bytes(pixels))    


def main():

    # Parser
    parser = argparse.ArgumentParser()
    parser.add_argument(
        dest = 'out_dir', 
        default = '.', 
        type = str, 
        help = 'Directorio de salida.'
        )

    # Diccionario de argumentos
    args = parser.parse_args()
    out_dir = args.out_dir
    os.makedirs(out_dir, exist_ok=True)

    W, H = 2048, 2048
    w, h = 63, 5

    print(f"Generando BMPs {W}×{H} en '{out_dir}/':\n")
    print(f"  {'archivo':40s}  {'dimensión':10s}  {'tamaño':>12s}")
    print(f"  {'-'*40}  {'-'*10}  {'-'*12}")

    # Generar casos de prueba
    case_01_flat_white(out_dir, W, H)
    case_02_flat_color(out_dir, W, H)
    case_03_noise(out_dir, W, H)
    case_04_flat_with_noise(out_dir, W, H)
    case_05_gradient(out_dir, W, H)
    case_06_stripes_h(out_dir, W, H)
    case_07_stripes_v(out_dir, W, H)
    case_08_checkerboard(out_dir, W, H)
    case_09_single_pixel_noise(out_dir, W, H)
    case_10_large_run_boundary(out_dir, W, H)
    case_11_debugging(out_dir, w, h)

    save_bmp(os.path.join(out_dir, '11_sin_pixeles.bmp'), 0, 0, bytearray())

    print(f"\nTotal: 10 archivos BMP generados.")

if __name__ == '__main__':
    main()
