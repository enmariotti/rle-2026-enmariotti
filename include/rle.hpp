#ifndef __RLE_HPP__
#define __RLE_HPP__

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>

/**
 * @brief Enum que refleja el estado de la salida de ciertas funciones.
 * 
 */
enum class Status 
{
    OK      = EXIT_SUCCESS,   // 0
    FAIL    = EXIT_FAILURE    // 1
};

/**
 * @brief Estructura que representa la informacion por canales.
 * 
 */
struct Channel 
{
    std::vector<uint8_t> r, g, b; // Pixeles en orden R, G, B. Pixel 0: Izquierda-Arriba
};

/**
 * @brief Estructura que representa una imagen.
 * 
 */
struct Image 
{
    uint32_t width;
    uint32_t height;
    
    Channel channel;
    
    /**
     * @brief Construct a new Image object
     * 
     */
    Image(): width(0), height(0) {};
    
    /**
     * @brief Destroy the Image object
     * 
     */
    ~Image() {};
};

/**
 * @brief Estructura que representa un archivo codificado.
 * 
 */
struct EncodedData 
{
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> metadata;

    Channel channel;
    
    /**
     * @brief Construct a new EncodedData object
     * 
     */
    EncodedData(): width(0), height(0) {};
    
    /**
     * @brief Destroy the EncodedData object
     * 
     */
    ~EncodedData() {};
};

/**
 * @brief Estructura generica que encapsula las funciones de manejo de imagenes.
 * 
 */
class ImageHandler
{
    public:
        /**
         * @brief Funcion de lectura de una imagen. 
         * 
         * @param path es el archivo de entrada. 
         * @return Image es la imagen retornada.
         */
        virtual Image read(const std::filesystem::path& path) = 0;

        /**
         * @brief Funcion de escritura de una imagen. 
         * 
         * @param path es el archivo de entrada.
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status write(const std::filesystem::path& path, const Image& image) = 0;

        /**
         * @brief Construct a new ImageHandler object
         * 
         */
        ImageHandler() {};

        /**
         * @brief Destroy the ImageHandler object
         * 
         */
        virtual ~ImageHandler() {};

};

/**
 * @brief Estructura que encapsula las funciones de manejo de archivos comprimidos.
 * 
 */
class EncodedHandler
{
    public:
        /**
         * @brief Funcion de lectura de un archivo codificado. 
         * 
         * @param path es el archivo de entrada. 
         * @return EncodedData son los datos retornados.
         */
        virtual EncodedData read(const std::filesystem::path& path) = 0;

        /**
         * @brief Funcion de escritura de un archivo codificado. 
         * 
         * @param path es el archivo de entrada.
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status write(const std::filesystem::path& path, const EncodedData& data) = 0;

        /**
         * @brief Construct a new EncodedHandler object
         * 
         */
        EncodedHandler() {};

        /**
         * @brief Destroy the EncodedHandler object
         * 
         */
        virtual ~EncodedHandler() {};

};

/**
 * @brief Estructura que representa un codificador generico.
 * 
 */
class Encoder
{
    protected:
        ImageHandler * img;
        EncodedHandler * enc;
        
    public:
        /**
         * @brief Funcion de codificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status encode(const std::filesystem::path& path) = 0;

        /**
         * @brief Funcion de decodificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status decode(const std::filesystem::path& path) = 0;

        /**
         * @brief Construct a new Encoder object
         * 
         */
        Encoder(ImageHandler* _img, EncodedHandler* _enc) : img(_img), enc(_enc) {};

        /**
         * @brief Destroy the Encoder object
         * 
         */
        virtual ~Encoder() {};
};

/**
 * @brief Estructura que encapsula las funciones de manejo de imagenes BMP.
 * 
 */
class BMPHandler : public ImageHandler
{
    public:
        /**
         * @brief Funcion de lectura de una imagen. 
         * 
         * @param path es el archivo de entrada. 
         * @return Image es la imagen retornada.
         */
        virtual Image read(const std::filesystem::path& path) override;

        /**
         * @brief Funcion de escritura de una imagen. 
         * 
         * @param path es el archivo de entrada.
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status write(const std::filesystem::path& path, const Image& image) override;

        /**
         * @brief Construct a new BMPHandler object
         * 
         */
        BMPHandler() {};

        /**
         * @brief Destroy the BMPHandler object
         * 
         */
        virtual ~BMPHandler() override {};
};

/**
 * @brief Estructura que encapsula las funciones de manejo de archivos comprimidos.
 * 
 */
class PRLEncodedHandler : public EncodedHandler
{
    public:
        /**
         * @brief Funcion de lectura de un archivo codificado. 
         * 
         * @param path es el archivo de entrada. 
         * @return EncodedData son los datos retornados.
         */
        virtual EncodedData read(const std::filesystem::path& path) override;

        /**
         * @brief Funcion de escritura de un archivo codificado. 
         * 
         * @param path es el archivo de entrada.
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status write(const std::filesystem::path& path, const EncodedData& data) override;

        /**
         * @brief Construct a new EncodedHandler object
         * 
         */
        PRLEncodedHandler() {};

        /**
         * @brief Destroy the EncodedHandler object
         * 
         */
        virtual ~PRLEncodedHandler() override {};

};

/**
 * @brief Clase que representa el encoder PLREncoder.
 * 
 */
class PLREncoder : public Encoder
{
    private:
        /**
         * @brief Funcion de emision de runs segun el formato especificado.
         * 
         * @param out es una referencia a un vector donde agregar el run codificado.
         * @param count es el numero de conteo de pixeles.
         */
        void emit_run(std::vector<uint8_t>& out, uint32_t count, uint8_t value);
        
        /**
         * @brief Funcion de emision de literales segun el formato especificado.
         * 
         * @param out es una referencia a un vector donde agregar el literal codificado.
         * @param count es el numero de conteo de pixeles. 
         * @param data es un puntero a la data de los pixeles.
         */
        void emit_literal(std::vector<uint8_t>& out, uint32_t count, const uint8_t* data);

        /**
         * @brief Tarea de compresion de un canal individual
         * 
         * @param out es una referencia a un vector donde se encuentra la codificacion.
         * @param in  es un puntero a la data de los pixeles.
         * @param len es la longitud del canal sin codificar.
         */
        void compress_channel(std::vector<uint8_t>& out, const uint8_t* in, uint64_t len);

        /**
         * @brief Tarea de descompresion de un canal individual
         * 
         * @param out es una referencia a un vector donde se encuentra la codificacion.
         * @param in  es un puntero a la data de los pixeles.
         * @param len es la longitud del canal codificado.
         * @param expected_pixels es la cantidad de pixeles esperados.
         */
        void decompress_channel(std::vector<uint8_t>& out, const uint8_t* in, const uint32_t len, const uint64_t expected_pixels);
        
    public:
        /**
         * @brief Construct a new Encoder R L E object
         * 
         */
        PLREncoder() : Encoder(new BMPHandler(), new PRLEncodedHandler()) {};
        
        /**
         * @brief Destroy the Encoder R L E object
         * 
         */
        virtual ~PLREncoder() override 
        {
            delete img;
            delete enc;
        };

        /**
         * @brief Funcion de codificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status encode(const std::filesystem::path& path) override;

        /**
         * @brief Funcion de decodificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        virtual Status decode(const std::filesystem::path& path) override;
        
};

#endif /**__RLE_HPP__ **/
