#include "CLI11.hpp"
#include "rle.hpp"
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) 
{
    CLI::App app{"Codificador PRLE. "};
    app.require_option(1);              // exactamente una operación obligatoria
 
    std::filesystem::path entrada, salida;
 
    // --encode
    auto* encode = app.add_option_group("encode");
    encode->add_flag("--encode", "Codificar imagen BMP.");
    encode->add_option("entrada", entrada, "Archivo de entrada")->required();
    encode->add_option("salida",  salida,  "Archivo de salida")->required();
 
    // --decode
    auto* decode = app.add_option_group("decode");
    decode->add_flag("--decode", "Decodificar archivo PRLE.");
    decode->add_option("entrada", entrada, "Archivo de entrada")->required();
    decode->add_option("salida",  salida,  "Archivo de salida")->required();

    CLI11_PARSE(app, argc, argv);
 
    RLE rle;
    try 
    {
        if (app.got_subcommand(encode))
        {
            rle.encode(entrada);
            rle.write_prle(salida);
        }
        else if (app.got_subcommand(decode))
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