#include "rle.hpp"
/**
 * Planar RLE Encoder - BMP color (24-bit, RGB 8-bit)
 *
 * Formato de canal:
 *   Run corto:     [00 cccccc][value]               count = 1..63
 *   Run largo:     [01 CCCCCC][cccccccc][value]     count = 64..16447  (offset 64)
 *   Literal corto: [10 cccccc][n values]            count = 1..63
 *   Literal largo: [11 CCCCCC][cccccccc][n values]  count = 64..16447  (offset 64)
 *
 *   count = 0 en tipos cortos   -> reservado, nunca emitido.
 *   Pixel aislado               -> literal de count = 1.
 *   Runs > 16447                -> runs consecutivos del mismo value.
 *
 * Formato del archivo de salida:
 *   [identificador 4B][version 1B][width 4B][height 4B]
 *   [offset_R 8B][size_R 4B]
 *   [offset_G 8B][size_G 4B]
 *   [offset_B 8B][size_B 4B]
 *   [datos R][datos G][datos B]
 */

static constexpr uint32_t RUN_SHORT_MAX     = 63;      // Max valor representable con 6 bits
static constexpr uint32_t RUN_LONG_MAX      = 16447;   // 64 + 16383 (14 bits + offset 64)
static constexpr uint32_t LITERAL_SHORT_MAX = 63;      // Max valor representable con 6 bits
static constexpr uint32_t LITERAL_LONG_MAX  = 16447;   // 64 + 16383 (14 bits + offset 64)

static constexpr uint32_t OFFSET_R   = 0;
static constexpr uint32_t SIZE_R     = 8;
static constexpr uint32_t OFFSET_G   = 12;
static constexpr uint32_t SIZE_G     = 20;
static constexpr uint32_t OFFSET_B   = 24;
static constexpr uint32_t SIZE_B     = 32;

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
static constexpr uint8_t LITERAL_LONG_CODE  = 0xC0;

static constexpr uint32_t RUN_THRESHOLD  = 3; // Minimo valor de conteo para que existe compresion no nula en un run.

static constexpr uint32_t HEADER_SIZE  = 49;
static constexpr uint32_t IDENTIFIER   = 0xCAFECAFE;
static constexpr uint8_t  VERSION      = 0x01;

static void write_u32_le(std::ofstream& out, const uint32_t& word)
{
    uint8_t buf[4] = { static_cast<uint8_t>(word),
                       static_cast<uint8_t>(word >> 8),
                       static_cast<uint8_t>(word >> 16),
                       static_cast<uint8_t>(word >> 24)
                    };
    out.write(reinterpret_cast<char*>(buf), 4);
}

static void read_u32_le(std::ifstream& in, uint32_t& word) 
{
    uint8_t buf[4];
    in.read(reinterpret_cast<char*>(buf), 4);
    word =  static_cast<uint32_t>(buf[0])         |
            static_cast<uint32_t>(buf[1]) << 8    |
            static_cast<uint32_t>(buf[2]) << 16   |
            static_cast<uint32_t>(buf[3]) << 24;
}

static void write_u64_le(std::ofstream& out, const uint64_t& word) 
{
    uint8_t buf[8] = { static_cast<uint8_t>(word),
                       static_cast<uint8_t>(word >> 8),
                       static_cast<uint8_t>(word >> 16),
                       static_cast<uint8_t>(word >> 24),
                       static_cast<uint8_t>(word >> 32),
                       static_cast<uint8_t>(word >> 40),
                       static_cast<uint8_t>(word >> 48),
                       static_cast<uint8_t>(word >> 56)
                    };
    out.write(reinterpret_cast<char*>(buf), 8);
}

static void read_u64_le(std::ifstream& in, uint64_t& word) 
{
    uint8_t buf[8];
    in.read(reinterpret_cast<char*>(buf), 8);
    word =  static_cast<uint64_t>(buf[0])         |
            static_cast<uint64_t>(buf[1]) << 8    |
            static_cast<uint64_t>(buf[2]) << 16   |
            static_cast<uint64_t>(buf[3]) << 24   |
            static_cast<uint64_t>(buf[4]) << 32   |
            static_cast<uint64_t>(buf[5]) << 40   |
            static_cast<uint64_t>(buf[6]) << 48   |
            static_cast<uint64_t>(buf[7]) << 56;
}

static void write_u32_le(std::vector<uint8_t>& buf, const uint32_t& word)
{
    buf.push_back(static_cast<uint8_t>(word));
    buf.push_back(static_cast<uint8_t>(word >> 8));
    buf.push_back(static_cast<uint8_t>(word >> 16));
    buf.push_back(static_cast<uint8_t>(word >> 24));
}

static void read_u32_le(const std::vector<uint8_t>& buf, uint32_t& word, const size_t offset)
{

    word =  static_cast<uint32_t>(buf[offset + 0])        |
            static_cast<uint32_t>(buf[offset + 1]) << 8   |
            static_cast<uint32_t>(buf[offset + 2]) << 16  |
            static_cast<uint32_t>(buf[offset + 3]) << 24;
}

static void write_u64_le(std::vector<uint8_t>& buf, const uint64_t& word)
{
    buf.push_back(static_cast<uint8_t>(word));
    buf.push_back(static_cast<uint8_t>(word >> 8));
    buf.push_back(static_cast<uint8_t>(word >> 16));
    buf.push_back(static_cast<uint8_t>(word >> 24));
    buf.push_back(static_cast<uint8_t>(word >> 32));
    buf.push_back(static_cast<uint8_t>(word >> 40));
    buf.push_back(static_cast<uint8_t>(word >> 48));
    buf.push_back(static_cast<uint8_t>(word >> 56));
}

static void read_u64_le(const std::vector<uint8_t>& buf, uint64_t& word, size_t offset)
{
    word =  static_cast<uint64_t>(buf[offset + 0])        |
            static_cast<uint64_t>(buf[offset + 1]) << 8   |
            static_cast<uint64_t>(buf[offset + 2]) << 16  |
            static_cast<uint64_t>(buf[offset + 3]) << 24  |
            static_cast<uint64_t>(buf[offset + 4]) << 32  |
            static_cast<uint64_t>(buf[offset + 5]) << 40  |
            static_cast<uint64_t>(buf[offset + 6]) << 48  |
            static_cast<uint64_t>(buf[offset + 7]) << 56;
}

EncodedData PRLEncodedHandler::read(const std::filesystem::path& path) 
{

    // RVO
    EncodedData result;
    uint64_t offset_r;  uint32_t size_r;
    uint64_t offset_g;  uint32_t size_g;
    uint64_t offset_b;  uint32_t size_b;

    // Abrir el archivo. Manejar posibles errores.
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error( "No se pudo abrir: " + path.filename().string() );  
    } 

    // Cantidad de bytes del header completo.
    uint8_t buf[HEADER_SIZE] = {0};
    file.read(reinterpret_cast<char*>(buf), HEADER_SIZE);
    if (file.gcount() != HEADER_SIZE)
    {
        throw std::runtime_error("Archivo demasiado corto para contener un header valido");
    }

    file.seekg(0, std::ios::beg); // 0 bytes desde la posición inicial

    // Identificador
    uint32_t identifier = 0;
    read_u32_le(file, identifier);
    if (identifier != IDENTIFIER)
    {
        throw std::runtime_error("Identificador invalido, no es un archivo .prle");
    }

    // Version
    uint8_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 1);
    if (version != VERSION)
    {
        throw std::runtime_error("Versión de formato no soportada: " + std::to_string(version));
    }

    // Ancho y alto
    read_u32_le(file, result.width);
    read_u32_le(file, result.height);
    
    // Validar dimensiones
    if (result.width == 0 || result.height == 0)
    {
        throw std::runtime_error("Dimensiones nulas en el header");
    }

    uint64_t npix_check = static_cast<uint64_t>(result.width) * static_cast<uint64_t>(result.height);
    if (npix_check > 0x0FFFFFFFFFFFull)
    {
        throw std::runtime_error("Dimensiones excesivas en el header");
    }

    // Offsets y tamaños de cada canal
    read_u64_le(file, offset_r);
    read_u32_le(file, size_r);

    read_u64_le(file, offset_g);
    read_u32_le(file, size_g);

    read_u64_le(file, offset_b);
    read_u32_le(file, size_b);

    // Validar que los offsets sean coherentes con el header
    if (offset_r < HEADER_SIZE)
    {
        throw std::runtime_error("offset_R inválido: solapa con el header");
    }
    if (offset_g < offset_r + size_r)
    {
        throw std::runtime_error("offset_G inválido: solapa con datos R");
    }
    if (offset_b < offset_g + size_g)
    {
        throw std::runtime_error("offset_B inválido: solapa con datos G");
    }

    file.seekg(offset_r, std::ios::beg); // offset_r bytes desde la posición inicial
    result.channel.r.resize(size_r);
    file.read(reinterpret_cast<char*>(result.channel.r.data()), size_r);
    if (file.gcount() != size_r)
    {
        throw std::runtime_error("No se pudo leer el canal R completo.");
    }

    file.seekg(offset_g, std::ios::beg); // offset_g bytes desde la posición inicial
    result.channel.g.resize(size_g);
    file.read(reinterpret_cast<char*>(result.channel.g.data()), size_g);
    if (file.gcount() != size_g)
    {
        throw std::runtime_error("No se pudo leer el canal G completo.");
    }
    
    file.seekg(offset_b, std::ios::beg); // offset_b bytes desde la posición inicial
    result.channel.b.resize(size_b);
    file.read(reinterpret_cast<char*>(result.channel.b.data()), size_b);
    if (file.gcount() != size_b)
    {
        throw std::runtime_error("No se pudo leer el canal B completo.");
    }
    
    file.close();

    write_u64_le(result.metadata, offset_r); write_u32_le(result.metadata, size_r);
    write_u64_le(result.metadata, offset_g); write_u32_le(result.metadata, size_g);
    write_u64_le(result.metadata, offset_b); write_u32_le(result.metadata, size_b);
    
    return result;

}

Status PRLEncodedHandler::write(const std::filesystem::path& path, const EncodedData& data)
{
    try
    {
        // No genera el archivo, si no se realizo un encode() antes.
        if ( data.channel.r.empty() &&
             data.channel.g.empty() &&
             data.channel.b.empty() )
        {
            throw std::runtime_error("Todos los canales vacios.");
        }

        // Abrir el archivo. Manejar posibles errores.
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error( "No se pudo abrir: " + path.filename().string() );  
        }
        
        // Header: 4 + 1 + 4 + 4 + (8 + 4) * 3 = 49 bytes
        file.write(reinterpret_cast<const char*>(&IDENTIFIER), 4);
        file.write(reinterpret_cast<const char*>(&VERSION), 1);

        // Ancho y alto originales
        write_u32_le(file, data.width);
        write_u32_le(file, data.height);

        uint64_t offset_r    = HEADER_SIZE;
        uint64_t offset_g    = offset_r + data.channel.r.size();
        uint64_t offset_b    = offset_g + data.channel.g.size();

        // R: Offset y tamaño
        write_u64_le(file, offset_r);
        write_u32_le(file, static_cast<uint32_t>(data.channel.r.size()));
        
        // G: Offset y tamaño
        write_u64_le(file, offset_g);
        write_u32_le(file, static_cast<uint32_t>(data.channel.g.size()));
        
        // B: Offset y tamaño    
        write_u64_le(file, offset_b);
        write_u32_le(file, static_cast<uint32_t>(data.channel.b.size()));

        file.write(reinterpret_cast<const char*>(data.channel.r.data()), data.channel.r.size());
        file.write(reinterpret_cast<const char*>(data.channel.g.data()), data.channel.g.size());
        file.write(reinterpret_cast<const char*>(data.channel.b.data()), data.channel.b.size());

        file.close();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return Status::FAIL;
    }
    return Status::OK;
}

Image BMPHandler::read(const std::filesystem::path& path) 
{
    // RVO
    Image result;

    // Abrir el archivo. Manejar posibles errores.
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error( "No se pudo abrir: " + path.filename().string() );  
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
        throw std::runtime_error( "No es un archivo BMP: " + path.filename().string() );
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
        throw std::runtime_error("Solo se soportan BMP de 24 bits por pixel.");
    }
    if (compression != BI_RGB)
    {
        throw std::runtime_error("Solo se soportan BMP sin compresión (BI_RGB).");
    }

    // Dimensiones de la imagen leida
    result.width  = static_cast<uint32_t>(std::abs(width_s));
    result.height = static_cast<uint32_t>(std::abs(height_s));

    // Dimensionar los vectores de pixeles
    uint64_t npixels = static_cast<uint64_t>(result.width) * static_cast<uint64_t>(result.height);
    result.channel.r.resize(npixels);
    result.channel.g.resize(npixels);
    result.channel.b.resize(npixels);

    // Cada fila de píxeles está alineada a 4 bytes
    uint32_t row_bytes_raw = result.width * CHANNELS;  // Cantidad de bytes por pixel
    uint32_t row_stride    = (row_bytes_raw + 3) & ~3U;   // Redondear al proximo multiplo de 4 (i = (i + 3) / 4 * 4;): 
                                                          // https://stackoverflow.com/questions/2022179/c-quick-calculation-of-next-multiple-of-4
    std::vector<uint8_t> row_buf(row_stride);             // Buffer para almacenar las filas.

    file.seekg(pixel_offset, std::ios::beg); // pixel_offset bytes desde la posición inicial

    uint64_t base = 0;
    uint32_t dst_row = 0; 
    for (uint32_t row = 0; row < result.height; ++row) 
    {
        file.read(reinterpret_cast<char*>(row_buf.data()), row_stride);

        if (!file)
        {
            throw std::runtime_error("Error leyendo datos de pixeles");
        } 

        // Fila destino: si esta invertida, mapear de abajo hacia arriba
        if (height_s < 0)
        {
            dst_row = row;                  // Nominal
        }
        else
        {
            dst_row = result.height - 1 - row; // Inversion
        }

        uint64_t base = static_cast<uint64_t>(dst_row) * static_cast<uint64_t>(result.width);

        for (uint32_t col = 0; col < result.width; ++col) 
        {
            // BMP almacena BGR
            result.channel.b[base + col] = row_buf[col * CHANNELS + CHANNEL_B];
            result.channel.g[base + col] = row_buf[col * CHANNELS + CHANNEL_G];
            result.channel.r[base + col] = row_buf[col * CHANNELS + CHANNEL_R];
        }
    }

    file.close();

    return result;
}

Status BMPHandler::write(const std::filesystem::path& path, const Image& data)
{
    try
    {
        // No genera el archivo, si no se realizo un decode() antes.
        if ( data.channel.r.empty() &&
             data.channel.g.empty() &&
             data.channel.b.empty() )
        {
            throw std::runtime_error("Todos los canales vacios.");
        }

        // Abrir el archivo. Manejar posibles errores.
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error( "No se pudo abrir: " + path.filename().string() );  
        }
        
        uint32_t row_bytes_raw = data.width * 3;
        uint32_t row_stride    = (row_bytes_raw + 3) & ~3U;
        uint32_t pad           = row_stride - row_bytes_raw;
        uint32_t pixel_data_sz = row_stride * data.height;
        uint32_t pixel_offset  = 14 + 40;
        uint32_t file_size     = pixel_offset + pixel_data_sz;

        // Manejar header de imagen BMP (14 bytes)
        // Header 	14 bytes 	  	Windows Structure: BITMAPFILEHEADER
        // Signature 	2 bytes 	0000h 	'BM'
        // FileSize 	4 bytes 	0002h 	File size in bytes
        // reserved 	4 bytes 	0006h 	unused (=0)
        // DataOffset 	4 bytes 	000Ah 	Offset from beginning of file to the beginning of the bitmap data
        // File header (14 bytes)
        file.write("BM", 2);
        write_u32_le(file, file_size);
        write_u32_le(file, 0);
        write_u32_le(file, pixel_offset);

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
        write_u32_le(file, 40);
        write_u32_le(file, static_cast<int32_t>(data.width));
        write_u32_le(file, static_cast<uint32_t>(-static_cast<int32_t>(data.height)));
        
        uint8_t planes_bpp[4] = {1, 0, 24, 0};
        file.write(reinterpret_cast<char*>(planes_bpp), 4);
        write_u32_le(file, 0);           // BI_RGB
        write_u32_le(file, pixel_data_sz);
        write_u32_le(file, 2835);        // X pixels per meter
        write_u32_le(file, 2835);        // Y pixels per meter
        write_u32_le(file, 0);           // colores en tabla
        write_u32_le(file, 0);           // colores importantes

        // Datos de píxeles: intercalar R, G, B → BGR (formato BMP)
        const uint8_t padding[3] = {0, 0, 0};
        for (uint32_t row = 0; row < data.height; ++row) 
        {
            const uint64_t base = static_cast<uint64_t>(row) * static_cast<uint64_t>(data.width);
            for (uint32_t col = 0; col < data.width; ++col) 
            {
                uint64_t idx = base + col;
                uint8_t pixel[3] = { data.channel.b[idx], 
                                     data.channel.g[idx],
                                     data.channel.r[idx] };  // BGR
                file.write(reinterpret_cast<char*>(pixel), 3);
            }
            if (pad > 0)
            {
                file.write(reinterpret_cast<const char*>(padding), pad);
            }
                
        }

        file.close();

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return Status::FAIL;
    }
    return Status::OK;
}

void PLREncoder::emit_run(std::vector<uint8_t>& out, uint32_t count, uint8_t value) 
{
    // Emitir en bloques de RUN_LONG_MAX si count > RUN_LONG_MAX
    while (count > 0) 
    {
        uint32_t chunk = std::min(count, RUN_LONG_MAX); // Largo de cout o RUN_LONG_MAX
        count -= chunk;

        if (chunk <= RUN_SHORT_MAX) 
        {
            // Run corto: [00 cccccc][value]
            uint8_t encoded = RUN_SHORT_CODE | (static_cast<uint8_t>(chunk) & FORMAT_MASK);
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

void PLREncoder::emit_literal(std::vector<uint8_t>& out, uint32_t count, const uint8_t* data) 
{
    // Emitir en bloques de LITERAL_LONG_MAX si count > LITERAL_LONG_MAX
    uint32_t offset = 0;
    while (offset < count)
    {
        uint32_t chunk = std::min(count - offset, LITERAL_LONG_MAX); // Largo de cout o LITERAL_LONG_MAX

        if (chunk <= LITERAL_SHORT_MAX) 
        {
            // Literal corto: [10 cccccc][n values]
            uint8_t encoded = LITERAL_SHORT_CODE | (static_cast<uint8_t>(chunk) & FORMAT_MASK);
            out.push_back(encoded);
        } 
        else 
        {
            // Literal largo: [11 CCCCCC][cccccccc][n values]
            uint32_t encoded = chunk - (RUN_SHORT_MAX + 1); // offset + 64
            uint8_t  high = LITERAL_LONG_CODE | (static_cast<uint8_t>(encoded >> 8) & FORMAT_MASK);
            uint8_t  low = static_cast<uint8_t>(encoded & 0xFF);
            out.push_back(high);
            out.push_back(low);
        }
        out.insert(out.end(), data + offset, data + offset + chunk);
        offset += chunk;
    }
}

void PLREncoder::compress_channel(std::vector<uint8_t>& out, const uint8_t* in, uint64_t len)
{
    out.clear();          // Limpiar estado
    out.reserve(len);     // Estimacion inicial de compromiso, el vector puede alocar mas elementos dinamicamente.

    std::vector<uint8_t> lit_buf;
    lit_buf.clear();
    lit_buf.reserve(LITERAL_LONG_MAX);

    // Guardar token de literales con lo almacenado en el buffer
    auto flush_literal = [&]() 
    {
        if (!lit_buf.empty())
        {
            emit_literal(out, static_cast<uint32_t>(lit_buf.size()), lit_buf.data());
            lit_buf.clear();
        }
    };

    uint64_t i = 0;
    while (i < len) 
    {
        // Medir el run que empieza en i
        uint8_t  value   = in[i];
        uint64_t run_end = i + 1;

        // Iteramos moviendo run_end sobre los datos
        while (run_end < len && in[run_end] == value)
        {
            ++run_end;
        }
        uint64_t run_len = run_end - i;

        if (run_len >= RUN_THRESHOLD) 
        {
            // Run rentable: mayor o igual a 3 pixeles
            flush_literal();
            emit_run(out, static_cast<uint32_t>(std::min(run_len, static_cast<uint64_t>(UINT32_MAX))), value);
        } 
        else 
        {
            // Run problematico: igual a 2 pixeles. 
            // A veces es mejor tomarlo como literal y a veces es mejor tomarlo como run.
            if (run_len == (RUN_THRESHOLD - 1)) 
            {
                if (lit_buf.empty()) 
                {
                    // flush_literal(); // No realiza ninguna accion. 
                    emit_run(out, (RUN_THRESHOLD - 1), value);
                } 
                else 
                {
                    // Hay literal abierto: Absorber en el literal evita cerrar y reabrir.
                    lit_buf.push_back(value);
                    if (lit_buf.size() == LITERAL_LONG_MAX)
                    {
                        flush_literal();
                    } 
                    
                    // Repite, para nos desbordar lit_buf y penalizar por re-dimensionamiento.
                    lit_buf.push_back(value);
                    if (lit_buf.size() == LITERAL_LONG_MAX)
                    {
                        flush_literal();
                    } 
                }
            }
            // Run con perdida: es igual a 1 pixel. El tamaño resultante es el doble. 
            // Se acumula en lit_buf para agruparse con otros pixeles.
            else
            {
                lit_buf.push_back(value);
                if (lit_buf.size() == LITERAL_LONG_MAX) 
                {
                    flush_literal();
                }
            }
        }
        i = run_end;
    }
    flush_literal();
}

void PLREncoder::decompress_channel(std::vector<uint8_t>& out, const uint8_t* in, const uint32_t len, const uint64_t expected_pixels)
{

    out.clear();                    // Limpiar estado
    out.reserve(expected_pixels);   // Pixeles esperados

    uint64_t written = 0;
    uint32_t i       = 0;

    while (i < len) 
    {
        uint8_t  ctrl = in[i++];
        uint8_t  type = (ctrl & ~FORMAT_MASK) >> 6;     // bits 7..6
        uint32_t cnt_field = ctrl & FORMAT_MASK;        // bits 5..0

        switch (type) 
        {
            case 0:
            // Run corto: [00 cccccc][value] count = 1..63 
            { 
                uint32_t count = cnt_field;
                if (count == 0)
                {
                    throw std::runtime_error("Stream corrupto: count = 0 en run corto");
                }

                if (i >= len)
                {
                    throw std::runtime_error("Stream truncado: falta value en run corto");
                }
                
                uint8_t value = in[i++];
                if (written + count > expected_pixels)
                {
                    throw std::runtime_error("Stream excede el tamaño esperado del canal");
                }

                out.insert(out.end(), count, value);
                written += count;
                break;
            }

            case 1: 
            // Run largo: [01 CCCCCC][cccccccc][value] count = 64..16447  (offset + 64)
            { 
                if (i >= len)
                {
                    throw std::runtime_error("Stream truncado: falta segundo byte en run largo");
                }

                uint8_t low = in[i++];
                uint16_t count = (static_cast<uint16_t>(cnt_field) << 8 | low) + (RUN_SHORT_MAX + 1);
                if (count < (RUN_SHORT_MAX + 1))
                {
                    throw std::runtime_error("Stream corrupto: count < 64 en run largo");
                }
                
                if (i >= len)
                {
                    throw std::runtime_error("Stream truncado: falta value en run largo");
                }

                uint8_t value = in[i++];
                if (written + count > expected_pixels)
                {
                    throw std::runtime_error("Stream excede el tamaño esperado del canal");
                }
                out.insert(out.end(), count, value);
                written += count;
                break;
            }

            case 2:
            // Literal corto: [10 cccccc][n values] count = 1..63
            { 
                uint32_t count = cnt_field;
                if (count == 0)
                {
                    throw std::runtime_error("Stream corrupto: count = 0 en literal corto");
                }

                if (i + count > len)
                {
                    throw std::runtime_error("Stream truncado: datos insuficientes en literal corto");
                }

                if (written + count > expected_pixels)
                {
                    throw std::runtime_error("Stream excede el tamaño esperado del canal");
                }
                out.insert(out.end(), in + i, in + i + count);
                written += count;
                i       += count;
                break;
            }

            case 3:
            // Literal largo: [11 CCCCCC][cccccccc][n values]  count = 64..16447  (offset + 64) 
            { 
                if (i >= len)
                {
                    throw std::runtime_error("Stream truncado: falta segundo byte en literal largo");
                }

                uint8_t low = in[i++];
                uint16_t count = (static_cast<uint16_t>(cnt_field) << 8 | low) + (RUN_SHORT_MAX + 1);
                if (count < (RUN_SHORT_MAX + 1))
                {
                    throw std::runtime_error("Stream corrupto: count < 64 en literal largo");
                }

                if (i + count > len)
                {
                    throw std::runtime_error("Stream truncado: datos insuficientes en literal largo");
                }

                if (written + count > expected_pixels)
                {
                    throw std::runtime_error("Stream excede el tamaño esperado del canal");
                }
                out.insert(out.end(),  in + i, in + i + count);
                written += count;
                i       += count;
                break;
            }
        }
    }

    if (written != expected_pixels)
    {
        throw std::runtime_error("Canal incompleto: se esperaban "
                                 + std::to_string(expected_pixels)
                                 + " pixeles, se obtuvieron "
                                 + std::to_string(written));

    }
}

Status PLREncoder::encode(const std::filesystem::path& in, const std::filesystem::path& out)
{
    try 
    { 
        // Allocar memoria
        Image       * img_input  = new Image(); 
        EncodedData * enc_output = new EncodedData(); 
        
        std::cout << "Leyendo BMP...\n";
        *img_input = this->img->read(in);
        
        std::cout << "  " << img_input->width << "×" << img_input->height
                  << " pixeles (" << img_input->width * img_input->height * CHANNELS << " bytes)\n";

        const uint64_t npixels = static_cast<uint64_t>(img_input->width) * static_cast<uint64_t>(img_input->height);
        
        std::cout << "Comprimiendo canales (3 hilos)...\n";
        std::string err_r, err_g, err_b;

        auto compress_safe = [&](std::vector<uint8_t>& out,
                                 const uint8_t* in,
                                 const uint64_t len,
                                 std::string& err) 
        {
            try 
            {
                compress_channel(out, in, len);
            } 
            catch (const std::exception& e)
            {
                err = e.what();
            }
            catch (...) 
            {
                err = "Error desconocido.";
            }
        };

        std::thread t_r(compress_safe, std::ref(enc_output->channel.r), img_input->channel.r.data(), npixels, std::ref(err_r));
        std::thread t_g(compress_safe, std::ref(enc_output->channel.g), img_input->channel.g.data(), npixels, std::ref(err_g));
        std::thread t_b(compress_safe, std::ref(enc_output->channel.b), img_input->channel.b.data(), npixels, std::ref(err_b));

        t_r.join(); t_g.join(); t_b.join();

        enc_output->width  = img_input->width;
        enc_output->height = img_input->height;

        if (!err_r.empty()) throw std::runtime_error("Canal R: " + err_r);
        if (!err_g.empty()) throw std::runtime_error("Canal G: " + err_g);
        if (!err_b.empty()) throw std::runtime_error("Canal B: " + err_b);

        std::cout << "Estadisticas...\n";
        uint64_t total_in  = npixels * CHANNELS;
        uint64_t total_out = HEADER_SIZE + enc_output->channel.r.size() + enc_output->channel.g.size() + enc_output->channel.b.size();
        double ratio = static_cast<double>(total_in) / static_cast<double>(total_out);

        std::cout << "  Canal R: " << enc_output->channel.r.size() << " bytes\n";
        std::cout << "  Canal G: " << enc_output->channel.g.size() << " bytes\n";
        std::cout << "  Canal B: " << enc_output->channel.b.size() << " bytes\n";
        std::cout << "  Total entrada:  " << total_in  << " bytes\n";
        std::cout << "  Total salida:   " << total_out << " bytes\n";
        std::cout << "  Ratio:          " << ratio     << ":1\n";

        // Guardar los datos
        this->enc->write(out, *enc_output);

        // Liberar memoria
        delete img_input;
        delete enc_output;

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return Status::FAIL;
    }

    return Status::OK;
}

Status PLREncoder::decode(const std::filesystem::path& in, const std::filesystem::path& out)
{
    try 
    {
        // Allocar memoria
        EncodedData * enc_input  = new EncodedData(); 
        Image       * img_output = new Image(); 
        
        std::cout << "Leyendo PRLE...\n";
        *enc_input = this->enc->read(in);
        
        std::cout << "  " << enc_input->width << "×" << enc_input->height
        << " pixeles (" << enc_input->width * enc_input->height * CHANNELS << " bytes)\n";
                
        const uint64_t npixels = static_cast<uint64_t>(enc_input->width) * static_cast<uint64_t>(enc_input->height);

        std::cout << "Descomprimiendo canales (3 hilos)...\n";
        std::string err_r, err_g, err_b;

        auto decompress_safe = [&](std::vector<uint8_t>& out,
                                   const uint8_t* in,
                                   const uint32_t len,
                                   const uint64_t expected_pixels,
                                   std::string& err) 
        {
            try 
            {
                decompress_channel(out, in, len, expected_pixels);
            } 
            catch (const std::exception& e)
            {
                err = e.what();
            }
            catch (...) 
            {
                err = "Error desconocido.";
            }
        };

        uint64_t offset_r;  uint32_t size_r;
        uint64_t offset_g;  uint32_t size_g;
        uint64_t offset_b;  uint32_t size_b;

        read_u64_le(enc_input->metadata, offset_r, OFFSET_R);
        read_u32_le(enc_input->metadata, size_r, SIZE_R);

        read_u64_le(enc_input->metadata, offset_g, OFFSET_G);
        read_u32_le(enc_input->metadata, size_g, SIZE_G);

        read_u64_le(enc_input->metadata, offset_b, OFFSET_B);
        read_u32_le(enc_input->metadata, size_b, SIZE_B);

        std::thread t_r(decompress_safe, std::ref(img_output->channel.r), 
                        enc_input->channel.r.data(), size_r, npixels, std::ref(err_r));
        std::thread t_g(decompress_safe, std::ref(img_output->channel.g),
                        enc_input->channel.g.data(), size_g, npixels, std::ref(err_g));
        std::thread t_b(decompress_safe, std::ref(img_output->channel.b),
                        enc_input->channel.b.data(), size_b, npixels, std::ref(err_b));

        t_r.join(); t_g.join(); t_b.join();

        img_output->width = enc_input->width;
        img_output->height = enc_input->height;

        if (!err_r.empty()) throw std::runtime_error("Canal R: " + err_r);
        if (!err_g.empty()) throw std::runtime_error("Canal G: " + err_g);
        if (!err_b.empty()) throw std::runtime_error("Canal B: " + err_b);

        std::cout << "Estadisticas...\n";
        uint64_t total_in  = size_r + size_g + size_b + HEADER_SIZE;
        uint64_t total_out = npixels * 3;
        std::cout << "  Total comprimido:    " << total_in  << " bytes\n";
        std::cout << "  Total descomprimido: " << total_out << " bytes\n";
        std::cout << "Listo.\n";

        // Guardar los datos
        this->img->write(out, *img_output);

        // Liberar memoria
        delete enc_input;
        delete img_output;

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return Status::FAIL;
    }
    return Status::OK;

}
