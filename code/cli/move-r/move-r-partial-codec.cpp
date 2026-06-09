/**
 * Helper for innovation-2 experiments: reload a move-r locate_rlzsa index and
 * serialize its partial-RLZSA layer with a different on-disk codec.
 */

#include <fstream>
#include <iostream>
#include <move_r/move_r.hpp>

uint8_t parse_codec(const std::string& codec)
{
    if (codec == "raw") return 0;
    if (codec == "v1" || codec == "varint" || codec == "varint-v1") return 1;
    if (codec == "v2" || codec == "varint-v2") return 2;
    std::cerr << "error: codec must be raw, v1/varint-v1, or v2/varint-v2" << std::endl;
    std::exit(1);
}

void help()
{
    std::cout << "move-r-partial-codec: rewrite a locate_rlzsa partial index using another partial codec.\n\n";
    std::cout << "usage: move-r-partial-codec <raw|v1|v2> <input.move-r-rlzsa> <output.move-r-rlzsa>\n";
}

template <typename pos_t>
void convert(const std::string& input_path, const std::string& output_path, uint8_t codec)
{
    std::ifstream input(input_path, std::ios::binary);
    if (!input.good()) {
        std::cerr << "error: cannot open input index: " << input_path << std::endl;
        std::exit(1);
    }

    move_r<_locate_rlzsa, char, pos_t> index;
    index.load(input);
    input.close();
    if (!index.has_partial_rlzsa()) {
        std::cerr << "error: input index has no partial RLZSA layer" << std::endl;
        std::exit(1);
    }
    index.set_partial_rlzsa_codec(codec);

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        std::cerr << "error: cannot create output index: " << output_path << std::endl;
        std::exit(1);
    }
    index.serialize(output);
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        help();
        return 1;
    }

    uint8_t codec = parse_codec(argv[1]);
    std::ifstream probe(argv[2], std::ios::binary);
    if (!probe.good()) {
        std::cerr << "error: cannot open input index: " << argv[2] << std::endl;
        return 1;
    }
    bool is_64_bit = false;
    probe.read((char*) &is_64_bit, 1);
    probe.close();

    if (is_64_bit) {
        convert<uint64_t>(argv[2], argv[3], codec);
    } else {
        convert<uint32_t>(argv[2], argv[3], codec);
    }
    return 0;
}
