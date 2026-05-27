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

    BMPImage(): width(0), height(0) {};
    ~BMPImage() {};
};

/**
 * @brief Clase que representa el encoder RLE.
 * 
 */
class RLE 
{
    private:
        // TODO: Definir
    public:
        // TODO: Constructor y destructor de la clase

        /**
         * @brief Funcion de lectura de un archivo BMP.
         * 
         * @param path es la direccion del archivo.
         * @param img es una estructura del tipo imagen.
         */
        void read_bmp(const std::filesystem::path& path, BMPImage& img);


};

#endif /**__RLE_HPP__ **/
