#include "rle.hpp"
/**
 * Planar RLE Encoder - BMP color (24-bit, RGB 8-bit)
 *
 * Formato de canal:
 *   Run corto:     [00 cccccc][value]               count = 1..63
 *   Run largo:     [01 CCCCCC][cccccccc][value]     count = 64..16447  (offset + 64)
 *   Literal corto: [10 cccccc][n values]            count = 1..63
 *   Literal largo: [11 CCCCCC][cccccccc][n values]  count = 64..16447  (offset + 64)
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

static constexpr uint32_t RUN_SHORT_MAX     = 63;      // Valor representable con 6 bits
static constexpr uint32_t RUN_LONG_MAX      = 16447;   // 63 + 16384 (14 bits + offset 64)
static constexpr uint32_t LITERAL_SHORT_MAX = 63;
static constexpr uint32_t LITERAL_LONG_MAX  = 16447;

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

static constexpr uint32_t HEADER_SIZE = 49;
static constexpr uint32_t IDENTIFIER  = 0xCAFECAFE;
static constexpr uint8_t  VERSION     = 0x01;

static void write_u32_le(std::ostream& out, const uint32_t word)
{
    uint8_t buf[4] = { static_cast<uint8_t>(word),
                       static_cast<uint8_t>(word >> 8),
                       static_cast<uint8_t>(word >> 16),
                       static_cast<uint8_t>(word >> 24)
                    };
    out.write(reinterpret_cast<char*>(buf), 4);
}

static void write_u64_le(std::ostream& out, uint64_t word) 
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

void RLE::read_bmp(const std::filesystem::path& path) 
{
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
    this->img.width  = static_cast<uint32_t>(std::abs(width_s));
    this->img.height = static_cast<uint32_t>(std::abs(height_s));

    // Dimensionar los vectores de pixeles
    uint64_t npixels = static_cast<uint64_t>(this->img.width) * static_cast<uint64_t>(this->img.height);
    this->img.channel.r.resize(npixels);
    this->img.channel.g.resize(npixels);
    this->img.channel.b.resize(npixels);

    // Cada fila de píxeles está alineada a 4 bytes
    uint32_t row_bytes_raw = this->img.width * CHANNELS;  // Cantidad de bytes por pixel
    uint32_t row_stride    = (row_bytes_raw + 3) & ~3U;   // Redondear al proximo multiplo de 4 (i = (i + 3) / 4 * 4;): 
                                                          // https://stackoverflow.com/questions/2022179/c-quick-calculation-of-next-multiple-of-4
    std::vector<uint8_t> row_buf(row_stride);             // Buffer para almacenar las filas.

    file.seekg(pixel_offset, std::ios::beg); // pixel_offset bytes desde la posición inicial

    uint64_t base = 0;
    uint32_t dst_row = 0; 
    for (uint32_t row = 0; row < this->img.height; ++row) 
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
            dst_row = this->img.height - 1 - row; // Inversion
        }

        uint64_t base = static_cast<uint64_t>(dst_row) * static_cast<uint64_t>(this->img.width);

        for (uint32_t col = 0; col < this->img.width; ++col) 
        {
            // BMP almacena BGR
            this->img.channel.b[base + col] = row_buf[col * CHANNELS + CHANNEL_B];
            this->img.channel.g[base + col] = row_buf[col * CHANNELS + CHANNEL_G];
            this->img.channel.r[base + col] = row_buf[col * CHANNELS + CHANNEL_R];
        }
    }
}

void RLE::emit_run(std::vector<uint8_t>& out, uint32_t count, uint8_t value) 
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

void RLE::emit_literal(std::vector<uint8_t>& out, uint32_t count, const uint8_t* data) 
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

void RLE::compress_channel(std::vector<uint8_t>& out, const uint8_t* in, uint64_t len)
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
            if (run_len == 2) 
            {
                if (lit_buf.empty()) 
                {
                    // flush_literal(); // No realiza ninguna accion. 
                    emit_run(out, 2, value);
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

bool RLE::encode(const std::filesystem::path& path)
{
    try 
    {
        std::cout << "Leyendo BMP...\n";
        read_bmp(path);
        std::cout << "  " << this->img.width << "×" << this->img.height
                  << " pixeles (" << this->img.width * this->img.height * CHANNELS << " bytes)\n";

        const uint64_t npixels = static_cast<uint64_t>(this->img.width) * static_cast<uint64_t>(this->img.height);
        
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

        std::thread t_r(compress_safe, std::ref(this->out.r), this->img.channel.r.data(), npixels, std::ref(err_r));
        std::thread t_g(compress_safe, std::ref(this->out.g), this->img.channel.g.data(), npixels, std::ref(err_g));
        std::thread t_b(compress_safe, std::ref(this->out.b), this->img.channel.b.data(), npixels, std::ref(err_b));

        t_r.join(); t_g.join(); t_b.join();

        if (!err_r.empty()) throw std::runtime_error("Canal R: " + err_r);
        if (!err_g.empty()) throw std::runtime_error("Canal G: " + err_g);
        if (!err_b.empty()) throw std::runtime_error("Canal B: " + err_b);

        std::cout << "Estadisticas...\n";
        uint64_t total_in  = npixels * CHANNELS;
        uint64_t total_out = HEADER_SIZE + this->out.r.size() + this->out.g.size() + this->out.b.size();
        double ratio = static_cast<double>(total_in) / static_cast<double>(total_out);

        std::cout << "  Canal R: " << this->out.r.size() << " bytes\n";
        std::cout << "  Canal G: " << this->out.g.size() << " bytes\n";
        std::cout << "  Canal B: " << this->out.b.size() << " bytes\n";
        std::cout << "  Total entrada:  " << total_in  << " bytes\n";
        std::cout << "  Total salida:   " << total_out << " bytes\n";
        std::cout << "  Ratio:          " << ratio     << ":1\n";

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

bool RLE::write_prle(const std::filesystem::path& path)
{
    try
    {
        // No genera el archivo, si no se realizo un encode() antes.
        if ( this->out.r.empty() && this->out.g.empty() && this->out.b.empty() )
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
        write_u32_le(file, this->img.width);
        write_u32_le(file, this->img.height);

        uint64_t offset_r    = HEADER_SIZE;
        uint64_t offset_g    = offset_r + this->out.r.size();
        uint64_t offset_b    = offset_g + this->out.g.size();

        // R: Offset y tamaño
        write_u64_le(file, offset_r);
        write_u32_le(file, static_cast<uint32_t>(this->out.r.size()));
        
        // G: Offset y tamaño
        write_u64_le(file, offset_g);
        write_u32_le(file, static_cast<uint32_t>(this->out.g.size()));
        
        // B: Offset y tamaño    
        write_u64_le(file, offset_b);
        write_u32_le(file, static_cast<uint32_t>(this->out.b.size()));

        file.write(reinterpret_cast<const char*>(this->out.r.data()), this->out.r.size());
        file.write(reinterpret_cast<const char*>(this->out.g.data()), this->out.r.size());
        file.write(reinterpret_cast<const char*>(this->out.b.data()), this->out.r.size());

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// TODO: Solo debugging. Llevar al archivo main.cpp y parsear argumentos.
int main()
{
    RLE rle;
    const std::filesystem::path in = "05_gradient.bmp";
    const std::filesystem::path out = "05_gradient.prle";
    rle.encode(in);
    rle.write_prle(out);
    
    return 0;
}