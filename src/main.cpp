#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include "bencode.hpp"


std::string read_binary_file(const std::string& file_path) {

    std::ifstream file(file_path , std::ios::binary | std::ios::ate);


    if (!file.is_open()){
        throw std::runtime_error("File couldn't be opened at: " + file_path);
    }


    std::streamsize size = file.tellg();
    
    file.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');

    if (!file.read(&buffer[0], size)) {
        throw std::runtime_error("Failed to read complete file: " + file_path + "buffer data with size: " + std::to_string(size));
    }
}

int main(){
    return 0;
}