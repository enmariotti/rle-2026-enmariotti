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
 * @brief Estructura que representa una imagen BMP.
 * 
 */
struct BMPImage 
{
    uint32_t width;
    uint32_t height;
    
    std::vector<uint8_t> r, g, b; // Pixeles en orden R, G, B - fila 0: arriba de la imagen
    
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
 * @brief Clase que representa el encoder RLE.
 * 
 */
class EncoderRLE 
{
    private:
        BMPImage img; // Imagen leida

        /**
         * @brief Funcion de emision de runs segun el formato especificado.
         * 
         * @param out es una referencia a un vector donde agregar el run codificado.
         * @param count es el numero de conteo de pixeles.
         * @param value es el valor del pixel contabilizado.
         */
        void emit_run(std::vector<uint8_t>& out, uint32_t count, uint8_t value); 

    public:
        /**
         * @brief Construct a new Encoder R L E object
         * 
         */
        EncoderRLE(){};
        
        /**
         * @brief Destroy the Encoder R L E object
         * 
         */
        ~EncoderRLE(){};

        /**
         * @brief Funcion de lectura de un archivo BMP.
         * 
         * @param path es la direccion del archivo.
         * @param img es una estructura del tipo imagen.
         */
        void read_bmp(const std::filesystem::path& path);

};

#endif /**__RLE_HPP__ **/
