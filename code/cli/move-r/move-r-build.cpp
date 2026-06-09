/**
 * part of LukasNalbach/Move-r
 *
 * MIT License
 *
 * Copyright (c) Lukas Nalbach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <filesystem>
#include <iostream>
#include <move_r/move_r.hpp>

int arg_idx = 1;
uint64_t n;
uint16_t a = 8;
uint16_t p = omp_get_max_threads();
bool hybrid_locate = false;
bool partial_rlzsa = false;
bool partial_rlzsa_adaptive = false;
uint64_t partial_rlzsa_block_size = 128;
uint64_t partial_rlzsa_gap = 4;
uint64_t partial_rlzsa_budget_blocks = 0;
uint64_t partial_rlzsa_train_occ_threshold = 100;
std::string path_partial_rlzsa_training_patterns;
uint8_t partial_rlzsa_codec = 0;
std::string path_partial_field_distribution;
std::string path_partial_field_saving;
std::string partial_field_stats_label;
std::string path_partial_varint_copy_prefix;
std::string path_partial_varint_v2_copy_prefix;
uint32_t hybrid_phi_threshold = 32;
uint32_t hybrid_phi_min_occ = 2;
uint32_t hybrid_phi_max_pattern = 64;
double hybrid_cost_phi = 7.0;
double hybrid_cost_rlz_init = 48.0;
double hybrid_cost_rlz_phrase = 4.0;
double hybrid_cost_rlz_decode = 1.25;
std::string path_adaptive_patterns_file;
uint64_t adaptive_sample_budget = 0;
bool adaptive_uniform_sampling = false;
uint32_t adaptive_sample_min_occ = 16;
uint32_t adaptive_sample_max_occ = 4096;
uint32_t adaptive_sample_max_distance = 0;
std::string path_prefix_index_file;
move_r_construction_mode mode = _suffix_array;
move_r_support support = _locate_move;
std::ofstream mf_idx;
std::ofstream mf_mds;
std::ofstream index_file;
std::string path_input_file;
std::string name_text_file;
std::string path_index_file;

void help(std::string msg)
{
    if (msg != "") std::cout << msg << std::endl;
    std::cout << "move-r-build: 构建 move-r / Move-r-rlz 索引。" << std::endl << std::endl;
    std::cout << "用法: move-r-build [...] <input_file>" << std::endl;
    std::cout << "   -c <mode>           construction mode: sa or bigbwt (default: sa)" << std::endl;
    std::cout << "   -o <base_name>      names the index file base_name.move-r(-rlzsa) (default: input_file)" << std::endl;
    std::cout << "   -s <support>        support: count, locate_move, locate_rlzsa, locate_rlzsa_bin_search or locate_lzendsa" << std::endl;
    std::cout << "                       (default: locate_move)" << std::endl;
    std::cout << "   -p <integer>        number of threads to use during the construction of the index" << std::endl;
    std::cout << "                       (default: all threads)" << std::endl;
    std::cout << "   -a <integer>        balancing parameter; a must be an integer number and a >= 2 (default: 8)" << std::endl;
    std::cout << "   -hybrid             only for locate_rlzsa: additionally build M_Phi^{-1} and enable hybrid locate" << std::endl;
    std::cout << "   -partial-rlzsa <b> <g> only serialize every g-th RLZSA block of size b; requires -hybrid" << std::endl;
    std::cout << "   -partial-rlzsa-adaptive <b> <budget_blocks> <training_patterns>" << std::endl;
    std::cout << "                       select partial LocalR blocks by high-occ training-query coverage; requires -hybrid" << std::endl;
    std::cout << "   -partial-train-thr <int> min occurrence count used by -partial-rlzsa-adaptive (default: 100)" << std::endl;
    std::cout << "   -partial-codec <raw|varint|varint-v1|varint-v2> choose partial RLZSA serialization codec (default: raw)" << std::endl;
    std::cout << "   -partial-varint    shorthand for -partial-codec varint" << std::endl;
    std::cout << "   -partial-varint-v2 shorthand for -partial-codec varint-v2" << std::endl;
    std::cout << "   -partial-field-stats <dist.csv> <saving.csv> <label>" << std::endl;
    std::cout << "                       append field distribution and varint-saving estimates after partial construction" << std::endl;
    std::cout << "   -partial-varint-copy <base_name>" << std::endl;
    std::cout << "                       after normal serialization, write the same partial index again with varint-v1 codec" << std::endl;
    std::cout << "   -partial-varint-v2-copy <base_name>" << std::endl;
    std::cout << "                       after normal serialization, write the same partial index again with varint-v2 codec" << std::endl;
    std::cout << "   -hybrid-thr <int>   max occurrence count where Phi can be considered (default: 32)" << std::endl;
    std::cout << "   -hybrid-min-occ <int> min occurrence count where Phi can be considered (default: 2)" << std::endl;
    std::cout << "   -hybrid-max-pattern <int> max pattern length where Phi can be considered; 0 disables this guard (default: 64)" << std::endl;
    std::cout << "   -hybrid-cost <phi> <rlz_init> <rlz_phrase> <rlz_decode>" << std::endl;
    std::cout << "   -adaptive-samples <patterns> <budget>" << std::endl;
    std::cout << "                       build query-aware adaptive RLZSA samples from training patterns" << std::endl;
    std::cout << "   -adaptive-strategy <score|uniform>" << std::endl;
    std::cout << "                       score is the proposed load-aware selection; uniform is a baseline" << std::endl;
    std::cout << "   -adaptive-min-occ <int>" << std::endl;
    std::cout << "                       only try adaptive samples for RLZSA intervals with at least this many occurrences" << std::endl;
    std::cout << "   -adaptive-max-occ <int>" << std::endl;
    std::cout << "                       only use adaptive samples for RLZSA intervals up to this occurrence count" << std::endl;
    std::cout << "   -adaptive-max-distance <int>" << std::endl;
    std::cout << "                       allow predecessor adaptive samples within this SA-distance from the interval start" << std::endl;
    std::cout << "   -m_idx <m_file_idx> m_file_idx is file to write measurement data of the index construction to" << std::endl;
    std::cout << "   -m_mds <m_file_mds> m_file_mds is file to write measurement data of the construction of the move" << std::endl;
    std::cout << "                       data structures to" << std::endl;
    std::cout << "   <input_file>        input file" << std::endl;
    exit(0);
}

void parse_args(char** argv, int argc)
{
    std::string s = argv[arg_idx];
    arg_idx++;

    if (s == "-o") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -o option");
        path_prefix_index_file = argv[arg_idx++];
    } else if (s == "-p") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -p option");
        p = atoi(argv[arg_idx++]);
        if (p < 1) help("error: p < 1");
        if (p > omp_get_max_threads()) help("error: p > maximum number of threads");
    } else if (s == "-c") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -p option");
        std::string construction_mode_str = argv[arg_idx++];
        if (construction_mode_str == "sa") mode = _suffix_array;
        else if (construction_mode_str == "bigbwt") mode = _bigbwt;
        else help("error: invalid option for -c");
    } else if (s == "-s") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -s option");
        std::string support_str = argv[arg_idx++];
        if (support_str == "count") {
            support = _count;
        } else if (support_str == "locate_one") {
            support = _locate_one;
        } else if (support_str == "locate_move") {
            support = _locate_move;
        } else if (support_str == "locate_rlzsa") {
            support = _locate_rlzsa;
        } else if (support_str == "locate_rlzsa_bin_search") {
            support = _locate_rlzsa_bin_search;
        } else if (support_str == "locate_lzendsa") {
            support = _locate_lzendsa;
        } else help("error: unknown mode provided with -s option");
    } else if (s == "-a") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -a option");
        a = atoi(argv[arg_idx++]);
        if (a < 2) help("error: a < 2");
    } else if (s == "-hybrid") {
        hybrid_locate = true;
    } else if (s == "-partial-rlzsa") {
        if (arg_idx + 1 >= argc - 1) help("error: missing parameters after -partial-rlzsa option");
        partial_rlzsa = true;
        partial_rlzsa_adaptive = false;
        partial_rlzsa_block_size = std::stoull(argv[arg_idx++]);
        partial_rlzsa_gap = std::stoull(argv[arg_idx++]);
        if (partial_rlzsa_block_size == 0) help("error: partial block size must be positive");
        if (partial_rlzsa_gap == 0) help("error: partial gap must be positive");
    } else if (s == "-partial-rlzsa-adaptive") {
        if (arg_idx + 2 >= argc - 1) help("error: missing parameters after -partial-rlzsa-adaptive option");
        partial_rlzsa = true;
        partial_rlzsa_adaptive = true;
        partial_rlzsa_block_size = std::stoull(argv[arg_idx++]);
        partial_rlzsa_budget_blocks = std::stoull(argv[arg_idx++]);
        path_partial_rlzsa_training_patterns = argv[arg_idx++];
        if (partial_rlzsa_block_size == 0) help("error: partial block size must be positive");
        if (partial_rlzsa_budget_blocks == 0) help("error: adaptive partial budget must be positive");
    } else if (s == "-partial-train-thr") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -partial-train-thr option");
        partial_rlzsa_train_occ_threshold = std::stoull(argv[arg_idx++]);
    } else if (s == "-partial-codec") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -partial-codec option");
        std::string codec = argv[arg_idx++];
        if (codec == "raw") {
            partial_rlzsa_codec = 0;
        } else if (codec == "varint" || codec == "varint-v1") {
            partial_rlzsa_codec = 1;
        } else if (codec == "varint-v2" || codec == "v2") {
            partial_rlzsa_codec = 2;
        } else {
            help("error: invalid partial codec, expected raw, varint, varint-v1 or varint-v2");
        }
    } else if (s == "-partial-varint") {
        partial_rlzsa_codec = 1;
    } else if (s == "-partial-varint-v2") {
        partial_rlzsa_codec = 2;
    } else if (s == "-partial-field-stats") {
        if (arg_idx + 2 >= argc - 1) help("error: missing parameters after -partial-field-stats option");
        path_partial_field_distribution = argv[arg_idx++];
        path_partial_field_saving = argv[arg_idx++];
        partial_field_stats_label = argv[arg_idx++];
    } else if (s == "-partial-varint-copy") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -partial-varint-copy option");
        path_partial_varint_copy_prefix = argv[arg_idx++];
    } else if (s == "-partial-varint-v2-copy") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -partial-varint-v2-copy option");
        path_partial_varint_v2_copy_prefix = argv[arg_idx++];
    } else if (s == "-hybrid-thr") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -hybrid-thr option");
        hybrid_phi_threshold = atoi(argv[arg_idx++]);
    } else if (s == "-hybrid-min-occ") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -hybrid-min-occ option");
        hybrid_phi_min_occ = std::stoul(argv[arg_idx++]);
    } else if (s == "-hybrid-max-pattern") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -hybrid-max-pattern option");
        hybrid_phi_max_pattern = std::stoul(argv[arg_idx++]);
    } else if (s == "-hybrid-cost") {
        if (arg_idx + 3 >= argc - 1) help("error: missing parameters after -hybrid-cost option");
        hybrid_cost_phi = atof(argv[arg_idx++]);
        hybrid_cost_rlz_init = atof(argv[arg_idx++]);
        hybrid_cost_rlz_phrase = atof(argv[arg_idx++]);
        hybrid_cost_rlz_decode = atof(argv[arg_idx++]);
    } else if (s == "-adaptive-samples") {
        if (arg_idx + 1 >= argc - 1) help("error: missing parameters after -adaptive-samples option");
        path_adaptive_patterns_file = argv[arg_idx++];
        adaptive_sample_budget = std::stoull(argv[arg_idx++]);
    } else if (s == "-adaptive-strategy") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -adaptive-strategy option");
        std::string strategy = argv[arg_idx++];
        if (strategy == "score") {
            adaptive_uniform_sampling = false;
        } else if (strategy == "uniform") {
            adaptive_uniform_sampling = true;
        } else {
            help("error: invalid adaptive strategy, expected score or uniform");
        }
    } else if (s == "-adaptive-uniform") {
        adaptive_uniform_sampling = true;
    } else if (s == "-adaptive-min-occ") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -adaptive-min-occ option");
        adaptive_sample_min_occ = std::stoul(argv[arg_idx++]);
    } else if (s == "-adaptive-max-occ") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -adaptive-max-occ option");
        adaptive_sample_max_occ = std::stoul(argv[arg_idx++]);
    } else if (s == "-adaptive-max-distance") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -adaptive-max-distance option");
        adaptive_sample_max_distance = std::stoul(argv[arg_idx++]);
    } else if (s == "-m_idx") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -m_idx option");
        std::string path_mf_idx = argv[arg_idx++];
        mf_idx.open(path_mf_idx, std::filesystem::exists(path_mf_idx) ? std::ios::app : std::ios::out);
        if (!mf_idx.good()) help("error: cannot open nor create <m_file_idx>");
    } else if (s == "-m_mds") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -m_mds option");
        std::string path_mf_mds = argv[arg_idx++];
        mf_mds.open(path_mf_mds, std::filesystem::exists(path_mf_mds) ? std::ios::app : std::ios::out);
        if (!mf_mds.good()) help("error: cannot open nor create <m_file_mds>");
    } else {
        help("error: unrecognized '" + s + "' option");
    }
}

template <typename pos_t, move_r_support support>
void build()
{
    move_r<support, char, pos_t> index(path_input_file, {
        .file_input = true,
        .mode = mode,
        .num_threads = p,
        .a = a,
        .hybrid_locate = hybrid_locate,
        .hybrid_phi_threshold = hybrid_phi_threshold,
        .hybrid_phi_min_occ = hybrid_phi_min_occ,
        .hybrid_phi_max_pattern = hybrid_phi_max_pattern,
        .hybrid_cost_phi = hybrid_cost_phi,
        .hybrid_cost_rlz_init = hybrid_cost_rlz_init,
        .hybrid_cost_rlz_phrase = hybrid_cost_rlz_phrase,
        .hybrid_cost_rlz_decode = hybrid_cost_rlz_decode,
        .log = true,
        .mf_idx = mf_idx.is_open() ? &mf_idx : nullptr,
        .mf_mds = mf_mds.is_open() ? &mf_mds : nullptr,
        .name_text_file = name_text_file
    });

    if constexpr (support == _locate_rlzsa) {
        if (partial_rlzsa) {
            if (!hybrid_locate) help("error: -partial-rlzsa requires -hybrid so Phi fallback is available");
            std::cout << "building partial RLZSA serialization layer: block "
                      << partial_rlzsa_block_size;
            if (partial_rlzsa_adaptive) {
                std::cout << ", adaptive budget " << partial_rlzsa_budget_blocks
                          << ", train_occ_threshold " << partial_rlzsa_train_occ_threshold << std::flush;
            } else {
                std::cout << ", gap " << partial_rlzsa_gap << std::flush;
            }
            auto partial_time = now();
            if (partial_rlzsa_adaptive) {
                std::ifstream training_patterns(path_partial_rlzsa_training_patterns, std::ios::binary);
                if (!training_patterns.good()) help("error: cannot open partial RLZSA training patterns");
                index.build_partial_rlzsa_adaptive(
                    static_cast<pos_t>(partial_rlzsa_block_size),
                    static_cast<pos_t>(partial_rlzsa_budget_blocks),
                    training_patterns,
                    static_cast<pos_t>(partial_rlzsa_train_occ_threshold));
                training_patterns.close();
            } else {
                index.build_partial_rlzsa(
                    static_cast<pos_t>(partial_rlzsa_block_size),
                    static_cast<pos_t>(partial_rlzsa_gap));
            }
            index.set_partial_rlzsa_codec(partial_rlzsa_codec);
            if (!path_partial_field_distribution.empty()) {
                index.write_partial_field_statistics(
                    path_partial_field_distribution,
                    path_partial_field_saving,
                    partial_field_stats_label.empty() ? name_text_file : partial_field_stats_label);
            }
            log_runtime(partial_time);
        }
        if (adaptive_sample_budget != 0) {
            if (path_adaptive_patterns_file.empty()) help("error: adaptive sample budget set without patterns");
            std::ifstream adaptive_patterns(path_adaptive_patterns_file, std::ios::binary);
            if (!adaptive_patterns.good()) help("error: cannot open adaptive samples <patterns>");

            std::cout << "building adaptive RLZSA samples from " << path_adaptive_patterns_file
                      << " with budget " << adaptive_sample_budget
                      << ", strategy " << (adaptive_uniform_sampling ? "uniform" : "score")
                      << ", min_occ " << adaptive_sample_min_occ
                      << ", max_occ " << adaptive_sample_max_occ
                      << ", max_distance " << adaptive_sample_max_distance << std::flush;
            auto adaptive_time = now();
            index.build_adaptive_samples(
                adaptive_patterns,
                adaptive_sample_budget,
                adaptive_uniform_sampling,
                adaptive_sample_min_occ,
                adaptive_sample_max_occ,
                adaptive_sample_max_distance);
            log_runtime(adaptive_time);
            std::cout << "adaptive samples: " << index.num_adaptive_samples()
                      << " (" << format_size(index.adaptive_sample_size_in_bytes()) << ")" << std::endl;
            adaptive_patterns.close();
        }
    }

    std::cout << "serializing the index" << std::flush;
    auto time = now();
    index.serialize(index_file);
    log_runtime(time);

    if constexpr (support == _locate_rlzsa) {
        if (partial_rlzsa && !path_partial_varint_copy_prefix.empty()) {
            std::string copy_path = path_partial_varint_copy_prefix + ".move-r-rlzsa";
            std::ofstream copy_file(copy_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!copy_file.good()) help("error: could not create varint copy index");
            index.set_partial_rlzsa_codec(1);
            std::cout << "serializing the varint copy to " << copy_path << std::flush;
            auto copy_time = now();
            index.serialize(copy_file);
            log_runtime(copy_time);
        }
        if (partial_rlzsa && !path_partial_varint_v2_copy_prefix.empty()) {
            std::string copy_path = path_partial_varint_v2_copy_prefix + ".move-r-rlzsa";
            std::ofstream copy_file(copy_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!copy_file.good()) help("error: could not create varint-v2 copy index");
            index.set_partial_rlzsa_codec(2);
            std::cout << "serializing the varint-v2 copy to " << copy_path << std::flush;
            auto copy_time = now();
            index.serialize(copy_file);
            log_runtime(copy_time);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) help("");
    while (arg_idx < argc - 1) parse_args(argv, argc);

    path_input_file = argv[arg_idx];
    if (path_prefix_index_file == "")
        path_prefix_index_file = path_input_file;

    std::cout << std::setprecision(4);
    name_text_file = path_input_file.substr(path_input_file.find_last_of("/\\") + 1);
    path_index_file = path_prefix_index_file.append(".move-r");
    if (support == _locate_rlzsa) path_index_file = path_index_file.append("-rlzsa");

    index_file.open(path_index_file, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!index_file.good()) help("error: invalid input, could not create <index_file>");

    n = std::filesystem::file_size(path_input_file) + 1;

    if (p > 1 && 1000 * p > n) {
        p = std::max<uint16_t>(1, n / 1000);
        std::cout << "n = " << n << ", warning: p > n/1000, setting p to n/1000 ~ " << std::to_string(p) << std::endl;
    } else {
        p = std::max<uint16_t>(1, std::min<uint64_t>({ uint64_t{omp_get_max_threads()}, n / 1000, p }));
    }

    std::cout << "正在为 " << path_input_file << " 构建 move-r";
    std::cout << "，线程数 " << format_threads(p) << "，a = " << a << std::endl;
    if (hybrid_locate && support == _locate_rlzsa) {
        std::cout << "已启用 Move-r-rlz 混合 locate：构建 RLZSA，同时保留 Phi 枚举结构用于对比和自适应选择。" << std::endl;
        if (partial_rlzsa) {
            std::cout << "已启用部分 RLZSA 序列化：块大小 "
                      << partial_rlzsa_block_size;
            if (partial_rlzsa_adaptive) {
                std::cout << "，自适应预算 " << partial_rlzsa_budget_blocks
                          << " 个增强块，训练阈值 " << partial_rlzsa_train_occ_threshold << "。" << std::endl;
            } else {
                std::cout << "，每 " << partial_rlzsa_gap << " 个块保留 1 个增强块。" << std::endl;
            }
        }
        std::cout << "混合代价模型: phi_threshold=" << hybrid_phi_threshold
                  << ", phi_min_occ=" << hybrid_phi_min_occ
                  << ", phi_max_pattern=" << hybrid_phi_max_pattern
                  << ", c_phi=" << hybrid_cost_phi
                  << ", c_rlz_init=" << hybrid_cost_rlz_init
                  << ", c_rlz_phrase=" << hybrid_cost_rlz_phrase
                  << ", c_rlz_decode=" << hybrid_cost_rlz_decode << std::endl;
    }
    std::cout << "索引将保存到 " << path_index_file << std::endl << std::endl;

    if (mf_idx.is_open()) {
        mf_idx << "RESULT"
            << " algo=build_move_r_" << move_r_support_suffix(support)
            << " text=" << name_text_file
            << " num_threads=" << p
            << " a=" << a
            << " hybrid_locate=" << (hybrid_locate && support == _locate_rlzsa ? 1 : 0)
            << " hybrid_phi_threshold=" << hybrid_phi_threshold
            << " hybrid_phi_min_occ=" << hybrid_phi_min_occ
            << " hybrid_phi_max_pattern=" << hybrid_phi_max_pattern
            << " partial_rlzsa=" << (partial_rlzsa && support == _locate_rlzsa ? 1 : 0)
            << " partial_rlzsa_adaptive=" << (partial_rlzsa_adaptive && support == _locate_rlzsa ? 1 : 0)
            << " partial_rlzsa_block_size=" << partial_rlzsa_block_size
            << " partial_rlzsa_gap=" << partial_rlzsa_gap
            << " partial_rlzsa_codec=" << static_cast<uint32_t>(partial_rlzsa_codec)
            << " partial_rlzsa_budget_blocks=" << partial_rlzsa_budget_blocks
            << " partial_rlzsa_train_occ_threshold=" << partial_rlzsa_train_occ_threshold
            << " hybrid_cost_phi=" << hybrid_cost_phi
            << " hybrid_cost_rlz_init=" << hybrid_cost_rlz_init
            << " hybrid_cost_rlz_phrase=" << hybrid_cost_rlz_phrase
            << " hybrid_cost_rlz_decode=" << hybrid_cost_rlz_decode
            << " adaptive_strategy=" << (adaptive_uniform_sampling ? "uniform" : "score")
            << " adaptive_min_occ=" << adaptive_sample_min_occ
            << " adaptive_max_occ=" << adaptive_sample_max_occ
            << " adaptive_max_distance=" << adaptive_sample_max_distance;
    }

    const bool use_32_bit_positions = n <= static_cast<uint64_t>(INT_MAX);

    if (support == _count) {
        if (use_32_bit_positions) {
            build<uint32_t, _count>();
        } else {
            build<uint64_t, _count>();
        }
    } else if (support == _locate_move) {
        if (use_32_bit_positions) {
            build<uint32_t, _locate_move>();
        } else {
            build<uint64_t, _locate_move>();
        }
    } else if (support == _locate_rlzsa) {
        if (use_32_bit_positions) {
            build<uint32_t, _locate_rlzsa>();
        } else {
            build<uint64_t, _locate_rlzsa>();
        }
    } else if (support == _locate_rlzsa_bin_search) {
        if (use_32_bit_positions) {
            build<uint32_t, _locate_rlzsa_bin_search>();
        } else {
            build<uint64_t, _locate_rlzsa_bin_search>();
        }
    } else if (support == _locate_lzendsa) {
        if (use_32_bit_positions) {
            build<uint32_t, _locate_lzendsa>();
        } else {
            build<uint64_t, _locate_lzendsa>();
        }
    }

    if (mf_idx.is_open()) mf_idx.close();
    if (mf_mds.is_open()) mf_mds.close();
    index_file.close();
}
