#include <fstream>
#include <iterator>
#include <iostream>
#include "json.hpp"

int main(int argc, char *argv[])
{
    json::writer writer;
    auto read = [&writer](json::item item, const std::string &data, std::size_t row, std::size_t column)
    {
        writer.write(item, data, std::ostream_iterator<char>(std::cout));
    };

    try
    {
        std::ifstream input("sample.json");

        json::reader reader(read);
        reader.read(std::istream_iterator<char>(input), std::istream_iterator<char>());
    }
    catch(json::exception &e)
    {
        std::cerr << e.what() << " at " << e.row() << ":" << e.column() << std::endl;
    }

    return 0;
}
