/**
 * Dump SR/CPL frequency-rank data for r-RLZSA / partial r-RLZSA indexes.
 */

#include <fstream>
#include <iostream>
#include <move_r/move_r.hpp>

void help()
{
    std::cout << "move-r-dump-fields: dump SR/CPL rank-frequency CSV files.\n\n";
    std::cout << "usage: move-r-dump-fields <index.move-r-rlzsa> <output_dir>\n";
}

template <typename pos_t>
void dump(const std::string& input_path, const std::string& output_dir)
{
    std::ifstream input(input_path, std::ios::binary);
    if (!input.good()) {
        std::cerr << "error: cannot open input index: " << input_path << std::endl;
        std::exit(1);
    }

    move_r<_locate_rlzsa, char, pos_t> index;
    index.load(input);
    index.dump_vrrlzsa_field_frequencies(output_dir);
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        help();
        return 1;
    }

    std::ifstream probe(argv[1], std::ios::binary);
    if (!probe.good()) {
        std::cerr << "error: cannot open input index: " << argv[1] << std::endl;
        return 1;
    }
    bool is_64_bit = false;
    probe.read((char*) &is_64_bit, 1);
    probe.close();

    if (is_64_bit) {
        dump<uint64_t>(argv[1], argv[2]);
    } else {
        dump<uint32_t>(argv[1], argv[2]);
    }
    return 0;
}
