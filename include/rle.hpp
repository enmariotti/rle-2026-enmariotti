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
 * @brief Enum que refleja la salida de ciertas funciones.
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
    std::vector<uint8_t> r, g, b; // Pixeles en orden R, G, B. Fila 0: arriba de la imagen
};

/**
 * @brief Estructura que representa una imagen BMP.
 * 
 */
struct BMPImage 
{
    uint32_t width;
    uint32_t height;
    
    Channel channel;
    
    /**
     * @brief Construct a new BMPImage object
     * 
     */
    BMPImage(): width(0), height(0) {};
    
    /**
     * @brief Destroy the BMPImage object
     * 
     */
    ~BMPImage() {};
};

/**
 * @brief Estructura que representa un archivo PRLE.
 * 
 */
struct PRLEFile 
{
    uint32_t width;
    uint32_t height;
    uint64_t offset_r;  uint32_t size_r;
    uint64_t offset_g;  uint32_t size_g;
    uint64_t offset_b;  uint32_t size_b;

    Channel channel;
    
    /**
     * @brief Construct a new PRLEFile object
     * 
     */
    PRLEFile(): width(0), height(0),
                offset_r(0),  size_r(0),
                offset_g(0),  size_g(0),
                offset_b(0),  size_b(0) {};
    
    /**
     * @brief Destroy the PRLEFile object
     * 
     */
    ~PRLEFile() {};
};

/**
 * @brief Clase que representa el encoder RLE.
 * 
 */
class RLE 
{
    private:
        BMPImage img_in;     // Imagen de entrada
        PRLEFile enc_out;    // Codificacion de salida
        
        PRLEFile enc_in;     // Codificacion de entrada
        BMPImage img_out;    // Imagen de salida

        /**
         * @brief Funcion de lectura de un archivo BMP.
         * 
         */
        void read_bmp(const std::filesystem::path& path);

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
         * @brief Funcion de lectura de un archivo PRLE.
         * 
         */
        void read_prle(const std::filesystem::path& path);
        
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
        RLE(){};
        
        /**
         * @brief Destroy the Encoder R L E object
         * 
         */
        ~RLE(){};

        /**
         * @brief Funcion de codificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        Status encode(const std::filesystem::path& path);

        /**
         * @brief Funcion de escritura del archivo comprimido. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        Status write_prle(const std::filesystem::path& path);

        /**
         * @brief Funcion de decodificacion de la imagen. 
         * 
         * @param path es el archivo de entrada-
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        Status decode(const std::filesystem::path& path);

        /**
         * @brief Funcion de escritura de la imagen reconstruida. 
         * 
         * @param path es el archivo de entrada.
         * @return Status es el estado de ejecucion de la tarea (OK o FAIL).
         */
        Status write_bmp(const std::filesystem::path& path);
        
};

#endif /**__RLE_HPP__ **/
