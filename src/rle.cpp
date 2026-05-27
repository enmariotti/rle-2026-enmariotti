#include "rle.hpp"
/**
 * Planar RLE Encoder - BMP color (24-bit, RGB 8-bit)
 *
 * Formato de canal:
 *   Run corto:     [00 cccccc][value]            count = 1..63
 *   Run largo:     [01 CCCCCC][cccccccc][value]  count = 64..16447  (offset + 64)
 *   Literal corto: [10 cccccc][value]            count = 2..63
 *   Literal largo: [11 CCCCCC][cccccccc][value]  count = 64..16447  (offset + 64)
 *
 *   count = 0 en cualquier tipo -> reservado, nunca emitido.
 *   Pixel aislado               -> run de count = 1 o literal de count = 1.
 *   Runs > 16447                -> runs consecutivos del mismo value.
 *
 * Formato del archivo de salida:
 *   [identificador 4B][version 1B][width 4B][height 4B]
 *   [offset_R 8B][size_R 4B]
 *   [offset_G 8B][size_G 4B]
 *   [offset_B 8B][size_B 4B]
 *   [datos R][datos G][datos B]
 */

static constexpr uint32_t RUN_SHORT_MAX  = 63;      // 6 bits
static constexpr uint32_t RUN_LONG_MAX   = 16447;   // 63 + 16384 (14 bits + offset 64)

static constexpr uint8_t CHANNELS    = 3;
static constexpr uint8_t CHANNEL_B   = 0;
static constexpr uint8_t CHANNEL_G   = 1;
static constexpr uint8_t CHANNEL_R   = 2;

static constexpr uint8_t BI_RGB    = 0;
static constexpr uint8_t BI_RLE8   = 1;
static constexpr uint8_t BI_RLE4   = 2;

static constexpr uint8_t BPP_RGB_4   = 4;
static constexpr uint8_t BPP_RGB_8   = 8;
static constexpr uint8_t BPP_RGB_16  = 16;
static constexpr uint8_t BPP_RGB_24  = 24;

static constexpr uint8_t FORMAT_MASK        = 0x3F;
static constexpr uint8_t RUN_SHORT_CODE     = 0x00;
static constexpr uint8_t RUN_LONG_CODE      = 0x40;
static constexpr uint8_t LITERAL_SHORT_CODE = 0x80;
static constexpr uint8_t LITERAL_LINE_CODE  = 0xC0;

void RLE::read_bmp(const std::filesystem::path& path, BMPImage& img) 
{
    // Abrir el archivo. Manejar posibles errores.
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error( "ERROR. No se pudo abrir: " + path.filename().string() );  
    } 

    // Manejar header de imagen BMP (14 bytes)
    // Header 	14 bytes 	  	Windows Structure: BITMAPFILEHEADER
    // Signature 	2 bytes 	0000h 	'BM'
    // FileSize 	4 bytes 	0002h 	File size in bytes
    // reserved 	4 bytes 	0006h 	unused (=0)
    // DataOffset 	4 bytes 	000Ah 	Offset from beginning of file to the beginning of the bitmap data
    uint8_t header[14] = {0};
    file.read(reinterpret_cast<char*>(header), 14);

    // Signature 	2 bytes 	0000h 	'BM' 
    if (header[0] != 'B' || header[1] != 'M')
    {
        throw std::runtime_error( "ERROR. No es un archivo BMP: " + path.filename().string() );
    }

    // DataOffset 	4 bytes 	000Ah 	Offset from beginning of file to the beginning of the bitmap data
    uint32_t pixel_offset = 0;
    std::memcpy(&pixel_offset, header + 10, 4);

    // InfoHeader 	    40 bytes 	  	    Windows Structure: BITMAPINFOHEADER
    // Size 	        4 bytes 	000Eh 	Size of InfoHeader = 40 
    // Width 	        4 bytes 	0012h 	Horizontal width of bitmap in pixels
    // Height 	        4 bytes 	0016h 	Vertical height of bitmap in pixels
    // Planes 	        2 bytes 	001Ah 	Number of Planes (=1)
    // Bits Per Pixel 	2 bytes 	001Ch 	Bits per Pixel used to store palette entry information. This also identifies in an indirect way the number of possible colors. Possible values are:
    //                                       1 = monochrome palette. NumColors = 1  
    //                                       4 = 4bit palletized. NumColors = 16  
    //                                       8 = 8bit palletized. NumColors = 256 
    //                                      16 = 16bit RGB. NumColors = 65536
    //                                      24 = 24bit RGB. NumColors = 16M
    // Compression 	    4 bytes 	001Eh 	Type of Compression  
    //                                      0 = BI_RGB   no compression  
    //                                      1 = BI_RLE8 8bit RLE encoding  
    //                                      2 = BI_RLE4 4bit RLE encoding
    // ImageSize 	    4 bytes 	0022h 	(compressed) Size of Image 
    //                                      It is valid to set this = 0 if Compression = 0
    // XpixelsPerM 	    4 bytes 	0026h 	horizontal resolution: Pixels/meter
    // YpixelsPerM 	    4 bytes 	002Ah 	vertical resolution: Pixels/meter
    // Colors Used 	    4 bytes 	002Eh 	Number of actually used colors. For a 8-bit / pixel bitmap this will be 100h or 256.
    // Important Colors 4 bytes 	0032h 	Number of important colors 0 = all
    
    
    int32_t width_s = 0;
    int32_t height_s = 0;
    file.seekg( 4, std::ios::cur);  // 4 bytes desde la posición actual
    file.read(reinterpret_cast<char*>(&width_s),  4);
    file.read(reinterpret_cast<char*>(&height_s), 4);

    uint16_t bpp = 0;
    file.seekg( 2, std::ios::cur);  // 2 bytes desde la posición actual
    file.read(reinterpret_cast<char*>(&bpp),    2);

    uint32_t compression = 0;
    file.read(reinterpret_cast<char*>(&compression), 4);

    if (bpp != BPP_RGB_24)
    {
        throw std::runtime_error("ERROR. Solo se soportan BMP de 24 bits por pixel.");
    }
    if (compression != BI_RGB)
    {
        throw std::runtime_error("ERROR. Solo se soportan BMP sin compresión (BI_RGB).");
    }

    // Dimensiones de la imagen leida
    img.width  = static_cast<uint32_t>(std::abs(width_s));
    img.height = static_cast<uint32_t>(std::abs(height_s));

    // Dimensionar los vectores de pixeles
    uint64_t npixels = static_cast<uint64_t>(img.width) * static_cast<uint64_t>(img.height);
    img.r.resize(npixels);
    img.g.resize(npixels);
    img.b.resize(npixels);

    // Cada fila de píxeles está alineada a 4 bytes
    uint32_t row_bytes_raw = img.width * CHANNELS;        // Cantidad de bytes por pixel
    uint32_t row_stride    = (row_bytes_raw + 3) & ~3U;   // Redondear al proximo multiplo de 4 (i = (i + 3) / 4 * 4;): 
                                                          // https://stackoverflow.com/questions/2022179/c-quick-calculation-of-next-multiple-of-4
    std::vector<uint8_t> row_buf(row_stride);             // Buffer para almacenar las filas.

    file.seekg(pixel_offset, std::ios::beg); // pixel_offset bytes desde la posición inicial

    uint64_t base = 0;
    uint32_t dst_row = 0; 
    for (uint32_t row = 0; row < img.height; ++row) 
    {
        file.read(reinterpret_cast<char*>(row_buf.data()), row_stride);

        if (!file)
        {
            throw std::runtime_error("ERROR. Error leyendo datos de pixeles");
        } 

        // Fila destino: si esta invertida, mapear de abajo hacia arriba
        if (height_s < 0)
        {
            dst_row = row;                  // Nominal
        }
        else
        {
            dst_row = img.height - 1 - row; // Inversion
        }

        uint64_t base = static_cast<uint64_t>(dst_row) * static_cast<uint64_t>(img.width);

        for (uint32_t col = 0; col < img.width; ++col) 
        {
            // BMP almacena BGR
            img.b[base + col] = row_buf[col * CHANNELS + CHANNEL_B];
            img.g[base + col] = row_buf[col * CHANNELS + CHANNEL_G];
            img.r[base + col] = row_buf[col * CHANNELS + CHANNEL_R];
        }
    }
}

void emit_run(std::vector<uint8_t>& out, uint32_t count, uint8_t value) 
{
    // Emitir en bloques de RUN_LONG_MAX si count > RUN_LONG_MAX
    while (count > 0) 
    {
        uint32_t chunk = std::min(count, RUN_LONG_MAX); // Largo de cout o RUN_LONG_MAX
        count -= chunk;

        if (chunk <= RUN_SHORT_MAX) 
        {
            // Run corto: [00 cccccc][value]
            uint32_t encoded = RUN_SHORT_CODE | (static_cast<uint8_t>(chunk) & FORMAT_MASK); // b7=0, b6=0
            out.push_back(encoded);  
            out.push_back(value);
        }
        else 
        {
            // Run largo: [01 CCCCCC][cccccccc][value]
            uint32_t encoded = chunk - (RUN_SHORT_MAX + 1); // offset + 64
            uint8_t  high = RUN_LONG_CODE | (static_cast<uint8_t>(encoded >> 8) & FORMAT_MASK);
            uint8_t  low = static_cast<uint8_t>(encoded & 0xFF);
            out.push_back(high);
            out.push_back(low);
            out.push_back(value);
        }
    }
}