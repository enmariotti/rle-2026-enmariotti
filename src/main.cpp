#include "CLI11.hpp"
#include "rle.hpp"
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) 
{
    CLI::App app{"Codificador PRLE."};

    bool do_encode = false, do_decode = false;
    std::filesystem::path entrada, salida;

    app.add_flag("--encode", do_encode, "Codificar imagen BMP.");
    app.add_flag("--decode", do_decode, "Decodificar archivo PRLE.");
    app.add_option("entrada", entrada, "Archivo de entrada")->required();
    app.add_option("salida",  salida,  "Archivo de salida")->required();

    CLI11_PARSE(app, argc, argv);

    // Validación manual
    if (do_encode == do_decode) 
    {
        std::cerr << "Error: No es posible ejecutar --encode y --decode simultaneamente.\n";
        return EXIT_FAILURE;
    }
 
    RLE rle;
    try 
    {
        if (do_encode)
        {
            rle.encode(entrada);
            rle.write_prle(salida);
        }
        else if (do_decode)
        {
            rle.decode(entrada);
            rle.write_bmp(salida);
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
 
    return EXIT_SUCCESS;
}