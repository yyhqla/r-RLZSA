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
bool output_occurrences = false;
bool check_correctness = false;
bool compare_locate = false;
bool write_compare_detail = false;
bool block_hybrid_locate = false;
uint64_t block_hybrid_block_size = 128;
uint64_t block_hybrid_gap = 4;
uint64_t block_hybrid_occ_threshold = 1000;
uint64_t block_hybrid_rlz_occ_threshold = 0;
std::string input;
std::ofstream mf;
std::string path_index_file;
std::string path_patterns_file;
std::string path_output_file;
std::ifstream index_file;
std::ifstream patterns_file;
std::ifstream input_file;
std::ofstream output_file;
std::ofstream compare_detail_file;
std::string name_text_file;
std::string path_input_file;

void help(std::string msg)
{
    if (msg != "") std::cout << msg << std::endl;
    std::cout << "move-r-locate: 定位所有输入模式串的出现位置。" << std::endl << std::endl;
    std::cout << "用法: move-r-locate [...] <index_file> <patterns>" << std::endl;
    std::cout << "   -m <m_file> <text_name>    m_file is the file to write measurement data to," << std::endl;
    std::cout << "                              text_name should be the name of the original file" << std::endl;
    std::cout << "   -i <input_file>            input_file must be the file the index was built for" << std::endl;
    std::cout << "                              (required for locate_rlzsa_bin_search and the -c option)" << std::endl;
    std::cout << "   -c                         checks correctness of each pattern occurrence on <input_file>" << std::endl;
    std::cout << "   -compare                   for hybrid locate_rlzsa indexes, also time forced Phi and forced RLZSA" << std::endl;
    std::cout << "   -compare-detail <csv>      write per-pattern hybrid/forced timing details for occurrence grouping" << std::endl;
    std::cout << "   -block-hybrid <b> <g> <theta>" << std::endl;
    std::cout << "                              experimental partial-RLZSA routing: for occ>=theta, every g-th SA block" << std::endl;
    std::cout << "                              of size b is decoded with RLZSA and other blocks with Phi" << std::endl;
    std::cout << "   -block-hybrid-rlz-thr <theta_rlz>" << std::endl;
    std::cout << "                              only use block hybrid if the query has at least theta_rlz occurrences in enhanced blocks" << std::endl;
    std::cout << "   -o <output_file>           write pattern occurrences to this file (in ASCII format; one line per pattern)" << std::endl;
    std::cout << "   <index_file>               index file (with extension .move-r)" << std::endl;
    std::cout << "   <patterns_file>            file in pizza&chili format containing the patterns" << std::endl;
    exit(0);
}

void parse_args(char** argv, int argc)
{
    std::string s = argv[arg_idx];
    arg_idx++;

    if (s == "-c") {
        check_correctness = true;
    } else if (s == "-compare") {
        compare_locate = true;
    } else if (s == "-compare-detail") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -compare-detail option.");
        compare_locate = true;
        write_compare_detail = true;
        compare_detail_file.open(argv[arg_idx++]);
        if (!compare_detail_file.good()) help("error: cannot open nor create <csv>");
    } else if (s == "-block-hybrid") {
        if (arg_idx + 2 >= argc - 1) help("error: missing parameters after -block-hybrid option.");
        block_hybrid_locate = true;
        block_hybrid_block_size = std::stoull(argv[arg_idx++]);
        block_hybrid_gap = std::stoull(argv[arg_idx++]);
        block_hybrid_occ_threshold = std::stoull(argv[arg_idx++]);
        if (block_hybrid_block_size == 0) help("error: block size must be positive");
        if (block_hybrid_gap == 0) help("error: block gap must be positive");
    } else if (s == "-block-hybrid-rlz-thr") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -block-hybrid-rlz-thr option.");
        block_hybrid_rlz_occ_threshold = std::stoull(argv[arg_idx++]);
    } else if (s == "-m") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -o option.");
        std::string path_m_file = argv[arg_idx++];
        mf.open(path_m_file, std::filesystem::exists(path_m_file) ? std::ios::app : std::ios::out);
        if (!mf.good()) help("error: cannot open nor create <m_file>");
        name_text_file = argv[arg_idx++];
    } else if (s == "-i") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -i option.");
        path_input_file = argv[arg_idx++];
        input_file.open(path_input_file, std::ios::binary);
        if (!input_file.good()) help("error: cannot open <input_file>");
    } else if (s == "-o") {
        if (arg_idx >= argc - 1) help("error: missing parameter after -o option.");
        output_occurrences = true;
        path_output_file = argv[arg_idx++];
    } else {
        help("error: unrecognized '" + s + "' option");
    }
}

template <typename pos_t, move_r_support support>
void measure_locate()
{
    std::cout << std::setprecision(4);
    std::cout << "正在加载索引" << std::flush;
    auto time = now();
    using idx_t = move_r<support, char, pos_t>;
    idx_t index;
    index.load(index_file);
    time = log_runtime(time);
    index_file.close();
    index.log_data_structure_sizes();

    if (support == _locate_rlzsa_bin_search || check_correctness) {
        if (path_input_file == "") help("error: <input_file> not provided");
        std::cout << std::endl << "正在加载原始文本" << std::flush;
        no_init_resize(input, index.input_size());
        read_from_file(input_file, input.data(), input.size());
        if constexpr (support == _locate_rlzsa_bin_search) index.set_input(input);
        input_file.close();
        time = log_runtime(time);
    }

    if constexpr (idx_t::has_rlzsa) {
        index.reset_hybrid_query_counters();
        if (block_hybrid_locate) {
            if (!index.has_hybrid_locate()) help("error: -block-hybrid requires a hybrid locate_rlzsa index with Phi support");
            std::cout << std::endl << "已启用实验性分块 Hybrid locate：block="
                      << block_hybrid_block_size
                      << ", gap=" << block_hybrid_gap
                      << ", theta=" << block_hybrid_occ_threshold
                      << ", theta_rlz=" << block_hybrid_rlz_occ_threshold << std::endl;
        } else if (index.has_hybrid_locate()) {
            std::cout << std::endl << "已启用混合 locate：按代价模型在 Phi 枚举和 RLZSA 解码之间选择。" << std::endl;
        } else if (compare_locate) {
            std::cout << std::endl << "提示: 当前索引没有 hybrid Phi 结构，-compare 将只执行默认 locate。" << std::endl;
        }
    }

    std::cout << std::endl << "正在查询模式串 ... " << std::endl;
    std::string header;
    std::getline(patterns_file, header);
    uint64_t num_patterns = number_of_patterns(header);
    uint64_t pattern_length = patterns_length(header);
    uint64_t perc;
    uint64_t last_perc = 0;
    uint64_t num_occurrences = 0;
    uint64_t time_locate = 0;
    uint64_t time_phi = 0;
    uint64_t time_rlzsa = 0;
    uint64_t num_occurrences_phi = 0;
    uint64_t num_occurrences_rlzsa = 0;
    uint64_t compare_mismatches = 0;
    uint64_t occurrence_checksum = 0;
    uint64_t occurrence_checksum_phi = 0;
    uint64_t occurrence_checksum_rlzsa = 0;
    std::chrono::steady_clock::time_point t2, t3;
    std::string pattern;
    no_init_resize(pattern, pattern_length);
    std::vector<pos_t> occurrences;
    bool equal;
    pos_t count;

    if (write_compare_detail) {
        compare_detail_file
            << "pattern_id,occ,chosen_strategy,time_hybrid_ns,time_phi_forced_ns,"
            << "time_rlzsa_forced_ns,occ_phi_forced,occ_rlzsa_forced,"
            << "checksum_hybrid,checksum_phi_forced,checksum_rlzsa_forced,mismatch\n";
    }

    for (uint64_t i = 0; i < num_patterns; i++) {
        perc = (100 * i) / num_patterns;

        if (perc > last_perc) {
            std::cout << "已完成 " << perc << "% ..." << std::endl;
            last_perc = perc;
        }

        patterns_file.read(pattern.data(), pattern_length);

        bool needs_occurrence_vector = output_occurrences || check_correctness;
        uint64_t pattern_occurrences = 0;
        uint64_t pattern_checksum = 0;
        uint64_t pattern_time_locate = 0;
        uint64_t pattern_time_phi = 0;
        uint64_t pattern_time_rlzsa = 0;
        uint64_t pattern_occ_phi = 0;
        uint64_t pattern_occ_rlzsa = 0;
        uint64_t pattern_checksum_phi = 0;
        uint64_t pattern_checksum_rlzsa = 0;
        bool pattern_mismatch = false;
        uint64_t phi_queries_before = 0;
        uint64_t rlzsa_queries_before = 0;
        if constexpr (idx_t::has_rlzsa) {
            phi_queries_before = index.hybrid_phi_query_count();
            rlzsa_queries_before = index.hybrid_rlzsa_query_count();
        }

        if (needs_occurrence_vector) {
            t2 = now();
            occurrences = index.locate(pattern);
            t3 = now();
            pattern_time_locate = time_diff_ns(t2, t3);
            time_locate += pattern_time_locate;
            pattern_occurrences = occurrences.size();
            num_occurrences += pattern_occurrences;
            for (pos_t occ : occurrences) pattern_checksum += static_cast<uint64_t>(occ);
            occurrence_checksum += pattern_checksum;
        } else {
            t2 = now();
            if constexpr (idx_t::has_rlzsa && idx_t::supports_bwsearch) {
                if (block_hybrid_locate) {
                    index.locate_block_hybrid(
                        pattern,
                        static_cast<pos_t>(block_hybrid_block_size),
                        static_cast<pos_t>(block_hybrid_gap),
                        static_cast<pos_t>(block_hybrid_occ_threshold),
                        static_cast<pos_t>(block_hybrid_rlz_occ_threshold),
                        [&](pos_t occ){
                            pattern_occurrences++;
                            pattern_checksum += static_cast<uint64_t>(occ);
                        });
                } else {
                    index.locate(pattern, [&](pos_t occ){
                        pattern_occurrences++;
                        pattern_checksum += static_cast<uint64_t>(occ);
                    });
                }
            } else {
                index.locate(pattern, [&](pos_t occ){
                    pattern_occurrences++;
                    pattern_checksum += static_cast<uint64_t>(occ);
                });
            }
            t3 = now();
            pattern_time_locate = time_diff_ns(t2, t3);
            time_locate += pattern_time_locate;
            num_occurrences += pattern_occurrences;
            occurrence_checksum += pattern_checksum;
        }

        if constexpr (idx_t::has_rlzsa && idx_t::supports_bwsearch) {
            if (compare_locate && index.has_hybrid_locate()) {
                if (needs_occurrence_vector) {
                    std::vector<pos_t> occ_phi;
                    std::vector<pos_t> occ_rlzsa;

                    t2 = now();
                    occ_phi = index.locate_phi(pattern);
                    t3 = now();
                    pattern_time_phi = time_diff_ns(t2, t3);
                    time_phi += pattern_time_phi;
                    pattern_occ_phi = occ_phi.size();
                    num_occurrences_phi += pattern_occ_phi;
                    for (pos_t occ : occ_phi) pattern_checksum_phi += static_cast<uint64_t>(occ);
                    occurrence_checksum_phi += pattern_checksum_phi;

                    t2 = now();
                    occ_rlzsa = index.locate_rlzsa(pattern);
                    t3 = now();
                    pattern_time_rlzsa = time_diff_ns(t2, t3);
                    time_rlzsa += pattern_time_rlzsa;
                    pattern_occ_rlzsa = occ_rlzsa.size();
                    num_occurrences_rlzsa += pattern_occ_rlzsa;
                    for (pos_t occ : occ_rlzsa) pattern_checksum_rlzsa += static_cast<uint64_t>(occ);
                    occurrence_checksum_rlzsa += pattern_checksum_rlzsa;

                    if (occ_phi != occurrences || occ_rlzsa != occurrences) {
                        pattern_mismatch = true;
                        compare_mismatches++;
                    }
                } else {
                    t2 = now();
                    index.locate_phi(pattern, [&](pos_t occ) {
                        pattern_occ_phi++;
                        pattern_checksum_phi += static_cast<uint64_t>(occ);
                    });
                    t3 = now();
                    pattern_time_phi = time_diff_ns(t2, t3);
                    time_phi += pattern_time_phi;
                    num_occurrences_phi += pattern_occ_phi;
                    occurrence_checksum_phi += pattern_checksum_phi;

                    t2 = now();
                    index.locate_rlzsa(pattern, [&](pos_t occ) {
                        pattern_occ_rlzsa++;
                        pattern_checksum_rlzsa += static_cast<uint64_t>(occ);
                    });
                    t3 = now();
                    pattern_time_rlzsa = time_diff_ns(t2, t3);
                    time_rlzsa += pattern_time_rlzsa;
                    num_occurrences_rlzsa += pattern_occ_rlzsa;
                    occurrence_checksum_rlzsa += pattern_checksum_rlzsa;

                    if (pattern_occ_phi != pattern_occ_rlzsa ||
                        pattern_checksum_phi != pattern_checksum_rlzsa) {
                        pattern_mismatch = true;
                        compare_mismatches++;
                    }
                }
            }
        }

        if (write_compare_detail) {
            std::string chosen_strategy = "none";
        if constexpr (idx_t::has_rlzsa) {
            uint64_t phi_delta = index.hybrid_phi_query_count() - phi_queries_before;
            uint64_t rlzsa_delta = index.hybrid_rlzsa_query_count() - rlzsa_queries_before;
            if (block_hybrid_locate) chosen_strategy = "block";
            else if (phi_delta > 0) chosen_strategy = "phi";
            else if (rlzsa_delta > 0) chosen_strategy = "rlzsa";
        }
            compare_detail_file
                << i << ','
                << pattern_occurrences << ','
                << chosen_strategy << ','
                << pattern_time_locate << ','
                << pattern_time_phi << ','
                << pattern_time_rlzsa << ','
                << pattern_occ_phi << ','
                << pattern_occ_rlzsa << ','
                << pattern_checksum << ','
                << pattern_checksum_phi << ','
                << pattern_checksum_rlzsa << ','
                << (pattern_mismatch ? 1 : 0) << '\n';
        }

        if (check_correctness) {
            for (pos_t occ : occurrences) {
                if (input.substr(occ, pattern_length) != pattern) {
                    std::cout << "error: wrong occurrence: " << occ << ", '" <<
                        input.substr(occ, pattern_length) <<  "' of pattern '" <<
                        pattern << "'" << std::endl;
                    exit(-1);
                }
            }
        }

        if (output_occurrences) {
            ips4o::sort(occurrences.begin(), occurrences.end());
            for (pos_t occ : occurrences) output_file << occ << " ";
            output_file << std::endl;
        }

        occurrences.clear();
    }

    std::cout << "平均每个模式串出现次数: " << (num_occurrences / num_patterns) << std::endl;
    std::cout << "模式串数量: " << num_patterns << std::endl;
    std::cout << "模式串长度: " << pattern_length << std::endl;
    std::cout << "总出现次数: " << num_occurrences << std::endl;
    if (!output_occurrences && !check_correctness && occurrence_checksum != 0)
        std::cout << "occurrence checksum: " << occurrence_checksum << std::endl;
    std::cout << "locate 总耗时: " << format_time(time_locate) << std::endl;
    std::cout << "             " << format_time(time_locate / num_patterns) << "/模式串" << std::endl;
    if (num_occurrences != 0)
      std::cout << "             " << format_time(time_locate / num_occurrences) << "/出现位置" << std::endl;

    if constexpr (idx_t::has_rlzsa) {
        if (block_hybrid_locate) {
            std::cout << "分块 Hybrid 统计: 查询 "
                      << index.block_hybrid_query_count()
                      << " 次，低 occ 回退 "
                      << index.block_hybrid_low_occ_query_count()
                      << " 次，Phi 块 "
                      << index.block_hybrid_phi_block_count()
                      << " 个，RLZSA 块 "
                      << index.block_hybrid_rlzsa_block_count()
                      << " 个" << std::endl;
            std::cout << "分块 Hybrid occurrence 统计: Phi 处理 "
                      << index.block_hybrid_phi_occurrence_count()
                      << " 个，RLZSA 处理 "
                      << index.block_hybrid_rlzsa_occurrence_count()
                      << " 个" << std::endl;
        } else if (index.has_hybrid_locate()) {
            std::cout << "混合策略统计: Phi 枚举查询 "
                      << index.hybrid_phi_query_count()
                      << " 次，RLZSA 解码查询 "
                      << index.hybrid_rlzsa_query_count()
                      << " 次" << std::endl;
            std::cout << "混合代价模型统计: 决策 "
                      << index.hybrid_model_query_count()
                      << " 次，实际估计 phrase "
                      << index.hybrid_phrase_estimate_query_count()
                      << " 次" << std::endl;
            std::cout << "混合 occurrence 统计: Phi 处理 "
                      << index.hybrid_phi_occurrence_count()
                      << " 个，RLZSA 处理 "
                      << index.hybrid_rlzsa_occurrence_count()
                      << " 个" << std::endl;
            std::cout << "自适应采样统计: 样本 "
                      << index.num_adaptive_samples()
                      << " 个，min_occ "
                      << index.adaptive_sample_min_occurrence_threshold()
                      << " 个，max_occ "
                      << index.adaptive_sample_max_occurrence_threshold()
                      << "，max_distance "
                      << index.adaptive_sample_max_distance_threshold()
                      << " 个，查询 "
                      << index.adaptive_sample_query_count()
                      << " 次，命中 "
                      << index.adaptive_sample_hit_count()
                      << " 次，精确命中 "
                      << index.adaptive_sample_exact_hit_count()
                      << " 次，前驱命中 "
                      << index.adaptive_sample_predecessor_hit_count()
                      << " 次，miss "
                      << index.adaptive_sample_miss_count()
                      << " 次，按 occ 跳过 "
                      << index.adaptive_sample_skipped_by_occ_count()
                      << " 次，处理 occurrence "
                      << index.adaptive_sample_occurrence_count()
                      << " 个，前驱距离总和 "
                      << index.adaptive_sample_distance_total()
                      << " 个" << std::endl;
            if constexpr (idx_t::supports_bwsearch) {
                if (compare_locate) {
                    std::cout << "强制 Phi 对比耗时: " << format_time(time_phi)
                              << "，出现次数 " << num_occurrences_phi
                              << "，checksum " << occurrence_checksum_phi << std::endl;
                    std::cout << "强制 RLZSA 对比耗时: " << format_time(time_rlzsa)
                              << "，出现次数 " << num_occurrences_rlzsa
                              << "，checksum " << occurrence_checksum_rlzsa << std::endl;
                    std::cout << "对比结果不一致的模式串数量: " << compare_mismatches << std::endl;
                }
            }
        } else {
            std::cout << "混合策略统计: 当前索引未启用 hybrid locate，全部使用 RLZSA 解码。" << std::endl;
        }
    }

    if (mf.is_open()) {
        mf << "RESULT";
        mf << " algo=locate_move_r_" << move_r_support_suffix(support);
        mf << " text=" << name_text_file;
        mf << " a=" << index.balancing_parameter();
        mf << " n=" << index.input_size();
        mf << " sigma=" << std::to_string(index.alphabet_size());
        mf << " r=" << index.num_bwt_runs();
        mf << " r_=" << index.M_LF().num_intervals();

        if constexpr (idx_t::supports_multiple_locate) {
            if constexpr (idx_t::has_locate_move) {
                mf << " r__=" << index.M_Phi_m1().num_intervals();
            } else if constexpr (idx_t::has_rlzsa) {
                mf << " z=" << index.num_phrases_rlzsa();
                mf << " z_l=" << index.num_literal_phrases_rlzsa();
                mf << " z_c=" << index.num_copy_phrases_rlzsa();
                mf << " hybrid_locate=" << (index.has_hybrid_locate() ? 1 : 0);
                mf << " hybrid_phi_queries=" << index.hybrid_phi_query_count();
                mf << " hybrid_rlzsa_queries=" << index.hybrid_rlzsa_query_count();
                mf << " hybrid_phi_occurrences=" << index.hybrid_phi_occurrence_count();
                mf << " hybrid_rlzsa_occurrences=" << index.hybrid_rlzsa_occurrence_count();
                mf << " hybrid_model_queries=" << index.hybrid_model_query_count();
                mf << " hybrid_phrase_estimate_queries=" << index.hybrid_phrase_estimate_query_count();
                mf << " adaptive_samples=" << index.num_adaptive_samples();
                mf << " adaptive_sample_bytes=" << index.adaptive_sample_size_in_bytes();
                mf << " adaptive_sample_min_occ=" << index.adaptive_sample_min_occurrence_threshold();
                mf << " adaptive_sample_max_occ=" << index.adaptive_sample_max_occurrence_threshold();
                mf << " adaptive_sample_max_distance=" << index.adaptive_sample_max_distance_threshold();
                mf << " adaptive_sample_queries=" << index.adaptive_sample_query_count();
                mf << " adaptive_sample_hits=" << index.adaptive_sample_hit_count();
                mf << " adaptive_sample_exact_hits=" << index.adaptive_sample_exact_hit_count();
                mf << " adaptive_sample_predecessor_hits=" << index.adaptive_sample_predecessor_hit_count();
                mf << " adaptive_sample_misses=" << index.adaptive_sample_miss_count();
                mf << " adaptive_sample_skipped_by_occ=" << index.adaptive_sample_skipped_by_occ_count();
                mf << " adaptive_sample_occurrences=" << index.adaptive_sample_occurrence_count();
                mf << " adaptive_sample_distance_sum=" << index.adaptive_sample_distance_total();
                mf << " block_hybrid_locate=" << (block_hybrid_locate ? 1 : 0);
                mf << " block_hybrid_block_size=" << block_hybrid_block_size;
                mf << " block_hybrid_gap=" << block_hybrid_gap;
                mf << " block_hybrid_occ_threshold=" << block_hybrid_occ_threshold;
                mf << " block_hybrid_rlz_occ_threshold=" << block_hybrid_rlz_occ_threshold;
                mf << " block_hybrid_queries=" << index.block_hybrid_query_count();
                mf << " block_hybrid_low_occ_queries=" << index.block_hybrid_low_occ_query_count();
                mf << " block_hybrid_phi_blocks=" << index.block_hybrid_phi_block_count();
                mf << " block_hybrid_rlzsa_blocks=" << index.block_hybrid_rlzsa_block_count();
                mf << " block_hybrid_phi_occurrences=" << index.block_hybrid_phi_occurrence_count();
                mf << " block_hybrid_rlzsa_occurrences=" << index.block_hybrid_rlzsa_occurrence_count();
                if constexpr (idx_t::supports_bwsearch) {
                    mf << " compare_locate=" << (compare_locate ? 1 : 0);
                    mf << " time_phi_forced=" << time_phi;
                    mf << " time_rlzsa_forced=" << time_rlzsa;
                    mf << " num_occurrences_phi_forced=" << num_occurrences_phi;
                    mf << " num_occurrences_rlzsa_forced=" << num_occurrences_rlzsa;
                    mf << " occurrence_checksum_phi_forced=" << occurrence_checksum_phi;
                    mf << " occurrence_checksum_rlzsa_forced=" << occurrence_checksum_rlzsa;
                    mf << " compare_mismatches=" << compare_mismatches;
                }
            } if constexpr (idx_t::has_lzendsa) {
                mf << " z=" << index.num_phrases_lzendsa();
            }
        }

        mf << " pattern_length=" << pattern_length;
        index.log_data_structure_sizes(mf);
        mf << " num_patterns=" << num_patterns;
        mf << " num_occurrences=" << num_occurrences;
        mf << " occurrence_checksum=" << occurrence_checksum;
        mf << " time_locate=" << time_locate;
        mf << std::endl;
        mf.close();
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) help("");
    while (arg_idx < argc - 2) parse_args(argv, argc);

    path_index_file = argv[arg_idx];
    path_patterns_file = argv[arg_idx + 1];

    index_file.open(path_index_file, std::ios::binary);
    patterns_file.open(path_patterns_file, std::ios::binary);

    if (!index_file.good()) help("error: could not read <index_file>");
    if (!patterns_file.good()) help("error: could not read <patterns_file>");

    if (output_occurrences) {
        output_file.open(path_output_file);
        if (!output_file.good()) help("error: could not create <output_file>");
    }

    bool is_64_bit;
    index_file.read((char*) &is_64_bit, 1);
    move_r_support _support;
    index_file.read((char*) &_support, sizeof(move_r_support));
    index_file.seekg(0, std::ios::beg);

    if (_support == _count || _support == _locate_one) {
        help("error: this index does not support locate");
    } else if (_support == _locate_move) {
        if (is_64_bit) {
            measure_locate<uint64_t, _locate_move>();
        } else {
            measure_locate<uint32_t, _locate_move>();
        }
    } else if (_support == _locate_rlzsa) {
        if (is_64_bit) {
            measure_locate<uint64_t, _locate_rlzsa>();
        } else {
            measure_locate<uint32_t, _locate_rlzsa>();
        }
    } else if (_support == _locate_rlzsa_bin_search) {
        if (is_64_bit) {
            measure_locate<uint64_t, _locate_rlzsa_bin_search>();
        } else {
            measure_locate<uint32_t, _locate_rlzsa_bin_search>();
        }
    } else if (_support == _locate_lzendsa) {
        if (is_64_bit) {
            measure_locate<uint64_t, _locate_lzendsa>();
        } else {
            measure_locate<uint32_t, _locate_lzendsa>();
        }
    }
}
