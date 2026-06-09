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

#pragma once

#include <iostream>
#include <data_structures/interleaved_byte_aligned_vectors.hpp>
#include <data_structures/interleaved_bit_aligned_vectors.hpp>
#include <move_data_structure/move_data_structure.hpp>
#include <move_data_structure/move_data_structure_l_.hpp>
#include <data_structures/rank_select_support.hpp>
#include <lzendsa/lzendsa_encoding.hpp>
#include <misc/utils.hpp>
#include <misc/files.hpp>
#include <misc/search.hpp>
#include <misc/log.hpp>
#include <misc/strings.hpp>
#include <omp.h>
#include <tsl/sparse_map.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <limits>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <type_traits>

/**
 * @brief type of locate support
 */
enum move_r_support {
    _count, // only count support (no locate support)
    _locate_one, // support for computing exaclty one occurrence per pattern
    _locate_move, // locate support is implemented using a move data structure to answer Phi^{-1}-queries
    _locate_rlzsa, // locate support is implemented by relative lepel-ziv encoding the differential suffix array
    _locate_rlzsa_bin_search, // count and locate is implemented by relative lepel-ziv encoding the differential
                              // suffix array and performing binary search over the suffix array
    _locate_lzendsa, // locate support is implemented by LZ-end encoding the differential suffix array

    // ############################# INTERNAL (DON'T USE) #############################
    _count_bi, // internal mode (don't use)
    _locate_move_bi_fwd, // internal mode (don't use)
    _locate_rlzsa_bi_fwd, // internal mode (don't use)
    _locate_bi_bwd // internal mode (don't use)
};

/**
 * @brief move-r construction mode
 */
enum move_r_construction_mode {
    _bigbwt, // builds the bwt with Big-BWT and stores many data structures on disk to reduce peak memory usage
    _suffix_array, // builds the suffix array in-memory and stores no data structures on disk
    _suffix_array_space // builds the suffix array and stores some data structures on disk
};

static std::string move_r_support_suffix(move_r_support support)
{
    std::string support_suffix;
        
    switch (support) {
        case _count: support_suffix = ""; break;
        case _locate_move: support_suffix = "move"; break;
        case _locate_rlzsa: support_suffix = "rlzsa"; break;
        case _locate_rlzsa_bin_search: support_suffix = "rlzsa_bin_search"; break;
        case _locate_lzendsa: support_suffix = "lzendsa"; break;
        default: break;
    }

    return support_suffix;
}

/**
 * @brief move-r construction parameters
 */
struct move_r_params {
    bool file_input = false; // true <=> the input is a file, and input must be the path to the file
    move_r_construction_mode mode = _suffix_array; // cosntruction mode to use
    uint16_t num_threads = omp_get_max_threads(); // maximum number of threads to use during the construction
    uint16_t a = 8; // balancing parameter, 2 <= a
    /* alphabet size of the input; if set to 0, the symbols in the input are maped to a compact alphabet; else
       (alphabet_size != 0), the input must already have a compact alphabet, and the symbols are not remapped */
    uint64_t alphabet_size = 0;
    uint16_t delta = 0; // SA-sampling rate (only for _locate_rlzsa_bin_search); if set to 0, the SA-sampling will be ~10% of the index size
    bool hybrid_locate = false; // true <=> locate_rlzsa also stores M_Phi^{-1} and chooses between Phi and RLZSA at query time
    uint32_t hybrid_phi_threshold = 32; // maximum occurrence count where Phi can be considered
    uint32_t hybrid_phi_min_occ = 2; // occurrence count below which RLZSA is preferred
    uint32_t hybrid_phi_max_pattern = 64; // maximum pattern length where Phi can be considered; 0 disables this guard
    double hybrid_cost_phi = 7.0; // estimated cost of one Phi step
    double hybrid_cost_rlz_init = 48.0; // estimated fixed cost of initializing RLZSA decoding
    double hybrid_cost_rlz_phrase = 4.0; // estimated cost of crossing one RLZSA phrase
    double hybrid_cost_rlz_decode = 1.25; // estimated cost of decoding one occurrence from RLZSA
    bool log = false; // controls, whether to print log messages
    std::ostream* mf_idx = nullptr; // measurement file for the index construciton
    std::ostream* mf_mds = nullptr; // measurement file for the move data structure construction
    std::string name_text_file = ""; // name of the input file (used only for measurement output)
    
    // ############################# INTERNAL (DON'T USE) #############################
    void* sa_vector = nullptr; // pointer to the suffix array (used only for bidirectional forward indexes)
    std::string* sa_file_name = nullptr; // name of the suffix array file (used only for bidirectional forward indexes)
    uint64_t* peak_memory_usage = nullptr; // variable to store the peak memory usage in (only for bidirectional indexes)
};

template <move_r_support support, typename sym_t, typename pos_t> class move_rb;

/**
 * @brief move-r index, size O(r*(a/(a-1)))
 * @tparam support type of locate support
 * @tparam sym_t value type (default: char for strings)
 * @tparam pos_t index integer type (use uint32_t if input size < UINT_MAX, else uint64_t)
 */
template <move_r_support support = _locate_move, typename sym_t = char, typename pos_t = uint32_t>
class move_r {
    template <move_r_support _support, typename _sym_t, typename _pos_t> friend class move_rb;

public:
    // check if the position type is supported
    static_assert(
        std::is_same_v<pos_t, uint32_t> ||
        std::is_same_v<pos_t, uint64_t>);

    // check if the type of input is supported
    static_assert(
        std::is_same_v<sym_t,char> ||
        std::is_same_v<sym_t,uint8_t> ||
        std::is_same_v<sym_t,uint16_t> ||
        std::is_same_v<sym_t,uint32_t> ||
        std::is_same_v<sym_t,uint64_t> ||
        std::is_same_v<sym_t,int8_t> ||
        std::is_same_v<sym_t,int16_t> ||
        std::is_same_v<sym_t,int32_t> ||
        std::is_same_v<sym_t,int64_t>
    );

    // internal (unsigned) symbol type
    using i_sym_t = constexpr_switch_t<
        constexpr_case<sizeof(sym_t) == 1,    uint8_t>,
        constexpr_case<sizeof(sym_t) == 2,    uint16_t>,
        constexpr_case<sizeof(sym_t) == 4,    uint32_t>,
     /* constexpr_case<sizeof(sym_t) == 8, */ uint64_t>;

    static constexpr bool is_bidirectional =
        support == _count_bi ||
        support == _locate_move_bi_fwd ||
        support == _locate_rlzsa_bi_fwd ||
        support == _locate_bi_bwd;
        
    static constexpr bool supports_locate = support != _count && support != _count_bi; // true <=> the index supports locate
    static constexpr bool supports_multiple_locate = supports_locate && support != _locate_one && support != _locate_bi_bwd; // true <=> the index supports locating multiple occurrences
    static constexpr bool supports_bwsearch = support != _locate_rlzsa_bin_search; // true <=> the index uses backward search for answering count queries
    static constexpr bool has_rlzsa = support == _locate_rlzsa || support == _locate_rlzsa_bi_fwd || support == _locate_rlzsa_bin_search; // true <=> the index has an rlzsa
    static constexpr bool has_lzendsa = support == _locate_lzendsa; // true <=> the index has an lzendsa
    static constexpr bool has_locate_move = support == _locate_move || support == _locate_move_bi_fwd; // true the index uses move data structures for Phi/Phi^{-1}
    static constexpr bool str_input = std::is_same_v<sym_t, char>; // true <=> the input is a string
    static constexpr bool int_input = !str_input; // true <=> the input is an iteger vector
    static constexpr bool byte_alphabet = sizeof(sym_t) == 1; // true <=> the input uses a byte alphabet
    static constexpr bool int_alphabet = !byte_alphabet; // true <=> the input uses an integer alphabet

    using map_int_t = std::conditional_t<byte_alphabet, std::vector<uint8_t>, tsl::sparse_map<sym_t, i_sym_t>>; // type of map_int
    using map_ext_t = std::vector<sym_t>; // type of map_ext
    using inp_t = std::conditional_t<str_input, std::string, std::vector<sym_t>>; // input container type
    using rsl_t = rank_select_support<i_sym_t, pos_t, true, true>; // type of RS_L'

    // sample rate of the copy phrases in the rlzsa
    static constexpr pos_t sample_rate_scp = 4;

    // maximum distance to scan over L' to find the first and last occurrences of sym in L'[\hat{b},\hat{e}]
    static constexpr pos_t max_scan_l_ = 128;

    // space-time tradeoff parameter for the size of blocks in L' (for L_prev and L_next)
    static constexpr pos_t _l_blk_size_factor = 4;

    // default overall size of the SA-delta-samples relative to the index size
    static constexpr double default_relative_sampling_size = 0.1;

    // ############################# INDEX VARIABLES #############################

protected:
    pos_t n = 0; // the length of the input
    pos_t sigma = 0; // the number of distinct characters (including the terminator symbol 1) of T
    pos_t r = 0; // r, the number of runs in L
    pos_t r_ = 0; // r', the number of input/output intervals in M_LF
    pos_t r__ = 0; // r'', the number of input/output intervals in M_Phi^{-1}
    pos_t r___ = 0; // r''', the number of input/output intervals in M_Phi
    pos_t z = 0; // z, the number of phrases in the rlzsa
    pos_t z_l = 0; // z_l, the number of literal phrases in the rlzsa
    pos_t z_c = 0; // z_c, the number of copy-phrases in the rlzsa
    pos_t z_end = 0; // z_end, the number of phrases in the lzendsa
    pos_t delta = 0; // SA-sample rate (for _locate_rlzsa_bin_search)
    uint16_t a = 0; // balancing parameter, restricts size to O(r*(a/(a-1))+z), 2 <= a
    uint16_t p_r = 1; // maximum possible number of threads to use while reverting the index
    pos_t _l_blk_size = 0; // size of the blocks in L' (for L_prev and L_next)
    pos_t _num_blks_l_ = 0; // number of blocks (of size _l_blk_size) in L' (for L_prev and L_next)
    uint8_t omega_idx = 0; // word width of SA_Phi^{-1}
    pos_t last_sa = 0; // SA[n - 1]
    bool hybrid_locate = false; // true <=> this RLZSA index can use Phi/RLZSA hybrid locate
    bool partial_rlzsa = false; // true <=> only selected RLZSA blocks were serialized
    pos_t partial_rlzsa_block_size = 0;
    pos_t partial_rlzsa_gap = 0;
    uint8_t partial_rlzsa_codec = 0; // 0 = legacy/raw, 1 = varint-v1 for selected high-variance fields
    uint32_t hybrid_phi_threshold = 32;
    uint32_t hybrid_phi_min_occ = 2;
    uint32_t hybrid_phi_max_pattern = 64;
    double hybrid_cost_phi = 7.0;
    double hybrid_cost_rlz_init = 48.0;
    double hybrid_cost_rlz_phrase = 4.0;
    double hybrid_cost_rlz_decode = 1.25;
    mutable uint64_t hybrid_phi_queries = 0;
    mutable uint64_t hybrid_rlzsa_queries = 0;
    mutable uint64_t hybrid_phi_occurrences = 0;
    mutable uint64_t hybrid_rlzsa_occurrences = 0;
    mutable uint64_t hybrid_model_queries = 0;
    mutable uint64_t hybrid_phrase_estimate_queries = 0;
    mutable uint64_t block_hybrid_queries = 0;
    mutable uint64_t block_hybrid_low_occ_queries = 0;
    mutable uint64_t block_hybrid_phi_blocks = 0;
    mutable uint64_t block_hybrid_rlzsa_blocks = 0;
    mutable uint64_t block_hybrid_phi_occurrences = 0;
    mutable uint64_t block_hybrid_rlzsa_occurrences = 0;
    mutable uint64_t adaptive_sample_queries = 0;
    mutable uint64_t adaptive_sample_hits = 0;
    mutable uint64_t adaptive_sample_exact_hits = 0;
    mutable uint64_t adaptive_sample_predecessor_hits = 0;
    mutable uint64_t adaptive_sample_misses = 0;
    mutable uint64_t adaptive_sample_skipped_by_occ = 0;
    mutable uint64_t adaptive_sample_occurrences = 0;
    mutable uint64_t adaptive_sample_distance_sum = 0;
    uint32_t adaptive_sample_min_occ = 16;
    uint32_t adaptive_sample_max_occ = 4096;
    uint32_t adaptive_sample_max_distance = 0;

    std::vector<pos_t> _adaptive_sample_pos;
    std::vector<pos_t> _adaptive_sample_sa;
    std::vector<pos_t> _adaptive_sample_x_p;
    std::vector<pos_t> _adaptive_sample_x_lp;
    std::vector<pos_t> _adaptive_sample_x_cp;
    std::vector<pos_t> _adaptive_sample_x_r;
    std::vector<pos_t> _adaptive_sample_s_np;
    tsl::sparse_map<pos_t, pos_t> _adaptive_sample_lookup;

    std::vector<pos_t> _partial_rlzsa_offsets;
    std::vector<pos_t> _partial_rlzsa_copy_offsets;
    std::vector<pos_t> _partial_rlzsa_literal_offsets;
    std::vector<pos_t> _partial_rlzsa_block_ids;
    std::vector<uint8_t> _partial_rlzsa_pt;
    std::vector<uint16_t> _partial_rlzsa_cpl;
    std::vector<pos_t> _partial_rlzsa_sr;
    std::vector<pos_t> _partial_rlzsa_lp;
    std::vector<pos_t> _partial_local_r_interval_starts;
    std::vector<pos_t> _partial_local_r_interval_lengths;

    /* true <=> the characters of the input have been remapped internally */
    bool symbols_remapped = false;
    uint64_t size_map_int = 0; // size of _map_int (for byte_alphabet = false)

    // ############################# INDEX DATA STRUCTURES #############################

    // mapping function from the alphabet of the input to the internal effective alphabet
    map_int_t _map_int;
    // mapping function from the internal effective alphabet to the alphabet of the input
    map_ext_t _map_ext;

    /* The Move Data Structure for LF. It also stores L', which can be accessed at
    position i with M_LF.L_(i). */
    move_data_structure_l_<pos_t, i_sym_t> _M_LF;
    // rank-select data structure for L'
    rsl_t _RS_L_;

    /* [0..num_blocks_L_() * L_block_size() - 1], stores at position blk * sigma + sym
    the position of the last occurrence of sym before block blk in L' */
    interleaved_byte_aligned_vectors<pos_t, pos_t> _L_prev;
    /* [0..num_blocks_L_() * L_block_size() - 1], stores at position blk * sigma + sym
    the position of the first occurrence of sym after or in block blk in L' */
    interleaved_byte_aligned_vectors<pos_t, pos_t> _L_next;

    // The Move Data Structure for Phi^{-1}.
    move_data_structure<pos_t> _M_Phi_m1;
    // The Move Data Structure for Phi.
    move_data_structure<pos_t> _M_Phi;
    // [0..r'-1] stores at position x the index of the output interval of M_Phi^{-1} that starts with SA_s[x] = SA[M_LF.p[x]]
    interleaved_byte_aligned_vectors<pos_t, pos_t> _SA_Phi_m1;

    /* [0..p_r-1], where D_e[i] = <x,j>, where x in [0,r'-1] and j is minimal, s.t. SA_s[x]=j > i* lfloor (n-1)/p rfloor;
    see the parallel revert algorithm to understand why this is useful. */
    std::vector<std::pair<pos_t, pos_t>> _D_e;

    // stores the suffix array values at the starting positions of the input intervals of M_LF, i.e, SA_s[i] = SA[M_LF.p[i]]
    interleaved_byte_aligned_vectors<pos_t, pos_t> _SA_s;
    // stores the suffix array values at the end positions of the input intervals of M_LF, i.e, SA_s'[i] = SA[M_LF.p[i + 1] - 1]
    interleaved_byte_aligned_vectors<pos_t, pos_t> _SA_s_;
    // reference for SA^d (differential suffix array)
    interleaved_byte_aligned_vectors<uint64_t, pos_t> _R;
    // bit vector storing the phrase types of the rlzsa, i.e, PT[i] = 1 <=> phrase i is literal
    plain_bit_vector<pos_t, true, true, true> _PT;
    // compressed bit vector marking the sampled starting positions in SA^d of the copy phrases of the rlzsa
    sd_array<pos_t> _SCP_S;
    // lengths of the copy phrases of the rlzsa
    std::vector<uint16_t> _CPL;
    // starting positions in R of the copy phrases of the rlzsa
    interleaved_byte_aligned_vectors<pos_t, pos_t> _SR;
    // literal phrases of the rlzsa
    interleaved_byte_aligned_vectors<pos_t, pos_t> _LP;

    // LZ-End encoding of the differential suffix array
    lzendsa_encoding _lzendsa;

    // [0..n / delta - 1] stores evenly-spaced SA-samples, i.e, SA_delta[i] = SA[i * delta] (for _locate_rlzsa_bin_search)
    interleaved_byte_aligned_vectors<pos_t, pos_t> _SA_delta;

    const inp_t* input = nullptr;

    // ############################# INTERNAL METHODS #############################

    /**
     * @brief sets SA_Phi^{-1}[x] to idx
     * @param x [0..r-1]
     * @param idx [0..r''-1]
     */
    inline void set_SA_Phi_m1(pos_t x, pos_t idx)
    {
        _SA_Phi_m1.template set_parallel<0, pos_t>(x, idx);
    }

    class construction;

    // ############################# CONSTRUCTORS #############################

public:
    move_r() = default;

    /**
     * @brief constructs a move_r index of the input
     * @param input the input
     * @param params construction parameters
     */
    move_r(inp_t& input, move_r_params params = {})
    {
        construction(*this, input, false, params);
    }

    /**
     * @brief constructs a move_r index of the input
     * @param input the input
     * @param params construction parameters
     */
    move_r(inp_t&& input, move_r_params params = {})
    {
        construction(*this, input, true, params);
    }

    /**
     * @brief constructs a move_r index from a suffix array and a bwt
     * @tparam sa_sint_t suffix array signed integer type
     * @param suffix_array vector containing the suffix array of the input
     * @param bwt string containing the bwt of the input, where $ = 1
     * @param params construction parameters
     */
    template <typename sa_sint_t>
    move_r(std::vector<sa_sint_t>& suffix_array, std::string& bwt, move_r_params params = {})
        requires(str_input)
    {
        construction(*this, suffix_array, bwt, params);
    }

    // ############################# MISC PUBLIC METHODS #############################

    void set_alphabet_maps(map_int_t& map_int, map_ext_t& map_ext) requires(is_bidirectional)
    {
        symbols_remapped = true;
        
        _map_int = map_int;
        _map_ext = map_ext;
    }

    /**
     * @brief returns the size of the input
     * @return size of the input
     */
    inline pos_t input_size() const
    {
        return n - 1;
    }

    /**
     * @brief returns the number of distinct characters in the input (alphabet size)
     * @return alphabet_size
     */
    inline uint32_t alphabet_size() const
    {
        return sigma - 1;
    }

    /**
     * @brief returns the number of runs in the bwt
     * @return number of runs in the bwt
     */
    inline pos_t num_bwt_runs() const
    {
        return r;
    }

    /**
     * @brief returns the size of the blocks in L' (for L_prev and L_next)
     * @return the size of the blocks in L'
     */
    inline pos_t L_block_size() const
    {
        return _l_blk_size;
    }

    /**
     * @brief returns the number of blocks in L' (for L_prev and L_next)
     * @return the number of blocks in L'
     */
    inline pos_t num_blocks_L_() const
    {
        return _num_blks_l_;
    }

    /**
     * @brief returns the number of phrases in the rlzsa
     * @return number of phrases in the rlzsa
     */
    inline pos_t num_phrases_rlzsa() const requires(has_rlzsa)
    {
        return z;
    }

    /**
     * @brief returns the number of literal phrases in the rlzsa
     * @return number of literal phrases in the rlzsa
     */
    inline pos_t num_literal_phrases_rlzsa() const requires(has_rlzsa)
    {
        return z_l;
    }

    /**
     * @brief returns the number of copy phrases in the rlzsa
     * @return number of copy phrases in the rlzsa
     */
    inline pos_t num_copy_phrases_rlzsa() const requires(has_rlzsa)
    {
        return z_c;
    }

    /**
     * @brief returns the number of phrases in the rlzsa
     * @return number of phrases in the rlzsa
     */
    inline pos_t num_phrases_lzendsa() const requires(has_lzendsa)
    {
        return z_end;
    }

    /**
     * @brief returns the balancing parameter the index has been built with
     * @return balancing parameter
     */
    inline uint16_t balancing_parameter() const
    {
        return a;
    }

    /**
     * @brief returns the number omega_idx of bits used by one entry in SA_Phi^{-1} (word width of SA_Phi^{-1})
     * @return omega_idx
     */
    inline uint8_t width_saphi() const requires(has_locate_move || has_rlzsa)
    {
        return omega_idx;
    }

    /**
     * @brief returns the maximum number of threads that can be used to revert the index
     * @return maximum number of threads that can be used to revert the index
     */
    inline uint16_t max_revert_threads() const
    {
        return p_r;
    }

    inline void set_input(inp_t& input) requires (support == _locate_rlzsa_bin_search)
    {
        this->input = &input;
    }

    /**
     * @brief returns the size of the data structure in bytes
     * @return size of the data structure in bytes
     */
    uint64_t size_in_bytes() const
    {
        return sizeof(this) +
            p_r * sizeof(pos_t) + // D_e
            _M_LF.size_in_bytes() +
            size_map_int +
            sizeof(sym_t) * sigma + // map_ext
            _RS_L_.size_in_bytes() +
            _M_Phi.size_in_bytes() +
            _M_Phi_m1.size_in_bytes() +
            _SA_Phi_m1.size_in_bytes() +
            _SA_s.size_in_bytes() +
            _SA_s_.size_in_bytes() +
            _SA_delta.size_in_bytes() +
            _R.size_in_bytes() +
            (z_c + 2) * sizeof(uint16_t) + // CPL
            _SCP_S.size_in_bytes() +
            _SR.size_in_bytes() +
            _LP.size_in_bytes() +
            _PT.size_in_bytes() +
            _partial_rlzsa_offsets.size() * sizeof(pos_t) +
            _partial_rlzsa_copy_offsets.size() * sizeof(pos_t) +
            _partial_rlzsa_literal_offsets.size() * sizeof(pos_t) +
            _partial_rlzsa_block_ids.size() * sizeof(pos_t) +
            _partial_rlzsa_pt.size() * sizeof(uint8_t) +
            _partial_rlzsa_cpl.size() * sizeof(uint16_t) +
            _partial_rlzsa_sr.size() * sizeof(pos_t) +
            _partial_rlzsa_lp.size() * sizeof(pos_t) +
            7 * sizeof(pos_t) * _adaptive_sample_pos.size() +
            _lzendsa.size_in_bytes();
    }

    /**
     * @brief logs all data structures
     */
    void log_data_structures() const
    {
        if (n > 50) {
            std::cout << "cannot log the contents for n > 50" << std::endl;
            return;
        }

        std::cout << "n = " << n;
        std::cout << ", sigma = " << sigma;
        if (M_LF().num_intervals() != 0) std::cout << ", a = " << a;
        std::cout << std::endl;

        std::cout << std::endl << "T:  ";
        auto T = revert_range();
        for (auto sym : T) std::cout << sym << ", ";
        std::cout << "$" << std::endl;

        for (pos_t i = 0; i < n; i++) {
            *reinterpret_cast<i_sym_t*>(&T[i]) = map_symbol(T[i]);
        }

        std::cout << "SA: ";
        log_contents(move_r<_locate_move, sym_t, pos_t>(T).SA_range());
            
        auto report_external_sym = [&](sym_t sym){
            std::cout << (sym == 0 ? '$' : sym);
        };
        
        std::cout << "L:  ";
        auto L = BWT_range();
        for (pos_t i = 0; i < n - 1; i++)
            { report_external_sym(L[i]); std::cout << ", "; }
        report_external_sym(L[n - 1]);
        std::cout << std::endl << std::endl;

        if (M_LF().num_intervals() != 0) {
            std::cout << "M_LF: " << "r' = " << r_ << std::endl;
            M_LF().log_contents();
            std::cout << std::endl;

            auto report_internal_sym = [&](i_sym_t sym){
                std::cout << (sym == 0 ? '$' : unmap_symbol(sym));
            };

            std::cout << "L': ";
            for (pos_t i = 0; i < r_ - 1; i++)
                { report_internal_sym(L_(i)); std::cout << ", "; }
            report_internal_sym(L_(r_ - 1));

            std::cout << std::endl;
            std::cout << "L' block size = " << _l_blk_size << std::endl;
            std::cout << "L'_prev: ";
            _L_prev.log_contents();
            std::cout << "L'_next: ";
            _L_next.log_contents();
            std::cout << std::endl;
        }

        if constexpr (has_locate_move) {
            std::cout << "M_Phi^{-1}: " << "r'' = " << r__ << std::endl;
            M_Phi_m1().log_contents();
            std::cout << std::endl;
        }

        if constexpr (support == _locate_move_bi_fwd) {
            std::cout << "M_Phi: " << "r''' = " << r___ << std::endl;
            M_Phi().log_contents();
            std::cout << std::endl;
        }

        if (!_SA_Phi_m1.empty()) {
            std::cout << "SA_Phi^{-1}: ";
            _SA_Phi_m1.log_contents();
        }

        if (!_SA_s.empty()) {
            std::cout << "SA_s:  ";
            _SA_s.log_contents();
        }

        if (!_SA_s_.empty()) {
            std::cout << "SA_s': ";
            _SA_s_.log_contents();
        }

        if (!_SA_delta.empty()) {
            std::cout << "delta = " << delta << std::endl;
            std::cout << "_SA_delta: ";
            _SA_delta.log_contents();
        }

        if (z != 0) {
            std::cout << std::endl;
            std::cout << "z = " << z << ", ";
            std::cout << "z_l = " << z_l << ", ";
            std::cout << "z_c = " << z_c << std::endl;
            std::cout << "SCP_S sampling rate = " << sample_rate_scp << std::endl;
            std::cout << std::endl;

            std::cout << "R: ";
            auto report_R = [&](pos_t i){ std::cout << int32_t{_R[i]} - int32_t{n}; };
            for (pos_t i = 0; i < _R.size() - 1; i++) { report_R(i); std::cout << ", "; }
            report_R(_R.size() - 1); std::cout << std::endl;
            
            std::cout << "PT: ";
            log_contents(_PT);
            std::cout << "SCP_S: ";
            log_contents(_SCP_S);
            std::cout << "CPL: ";
            log_contents(_CPL);
            std::cout << "SR: ";
            _SR.log_contents();

            std::cout << "LP: ";
            auto report_LP = [&](pos_t i){ std::cout << int32_t{_LP[i]} - int32_t{n}; };
            for (pos_t i = 0; i < _LP.size() - 1; i++) { report_LP(i); std::cout << ", "; }
            report_LP(_R.size() - 1); std::cout << std::endl;
        }

        if (z_end != 0) {
            _lzendsa.log_contents();
        }
    }

    /**
     * @brief logs the index data structure sizes to cout
     */
    void log_data_structure_sizes(bool print_index_size = true) const
    {
        if (print_index_size) std::cout << "index size: " << format_size(size_in_bytes()) << std::endl;

        if constexpr (supports_bwsearch) {
            uint64_t size_l_ = (_M_LF.width_l_() / 8) * (r_ + 1);
            std::cout << "M_LF: " << format_size(_M_LF.size_in_bytes() - size_l_) << std::endl;
            std::cout << "L': " << format_size(size_l_) << std::endl;

            if (byte_alphabet) {
                std::cout << "L'_prev & L'_next: " << format_size(
                    _L_prev.size_in_bytes() + _L_next.size_in_bytes()) << std::endl;
            } else {
                std::cout << "RS_L': " << format_size(_RS_L_.size_in_bytes()) << std::endl;
            }
        }

        if (int_alphabet && symbols_remapped) {
            std::cout << "map_int: " << format_size(size_map_int) << std::endl;
            std::cout << "map_ext: " << format_size(sizeof(sym_t) * sigma) << std::endl;
        }

        if (!_M_Phi.empty()) std::cout << "M_Phi: " << format_size(_M_Phi.size_in_bytes()) << std::endl;
        if (!_M_Phi_m1.empty()) std::cout << "M_Phi^{-1}: " << format_size(_M_Phi_m1.size_in_bytes()) << std::endl;
        if (!_SA_Phi_m1.empty()) std::cout << "SA_Phi^{-1}: " << format_size(_SA_Phi_m1.size_in_bytes()) << std::endl;

        if (z != 0) {
            std::cout << "R: " << format_size(_R.size_in_bytes()) << std::endl;
            if (partial_rlzsa) {
                std::cout << "partial RLZSA offsets: " << format_size(_partial_rlzsa_offsets.size() * sizeof(pos_t)) << std::endl;
                std::cout << "partial RLZSA copy offsets: " << format_size(_partial_rlzsa_copy_offsets.size() * sizeof(pos_t)) << std::endl;
                std::cout << "partial RLZSA literal offsets: " << format_size(_partial_rlzsa_literal_offsets.size() * sizeof(pos_t)) << std::endl;
                std::cout << "partial RLZSA block ids: " << format_size(_partial_rlzsa_block_ids.size() * sizeof(pos_t)) << std::endl;
                std::cout << "partial RLZSA PT: " << format_size(_partial_rlzsa_pt.size() * sizeof(uint8_t)) << std::endl;
                std::cout << "partial RLZSA CPL: " << format_size(_partial_rlzsa_cpl.size() * sizeof(uint16_t)) << std::endl;
                std::cout << "partial RLZSA SR: " << format_size(_partial_rlzsa_sr.size() * sizeof(pos_t)) << std::endl;
                std::cout << "partial RLZSA LP: " << format_size(_partial_rlzsa_lp.size() * sizeof(pos_t)) << std::endl;
            } else {
                std::cout << "CPL: " << format_size((z_c + 2) * sizeof(uint16_t)) << std::endl;
                std::cout << "SCP_S: " << format_size(_SCP_S.size_in_bytes()) << std::endl;
                std::cout << "SR: " << format_size(_SR.size_in_bytes()) << std::endl;
                std::cout << "LP: " << format_size(_LP.size_in_bytes()) << std::endl;
                std::cout << "PT: " << format_size(_PT.size_in_bytes()) << std::endl;
            }
            if (!_adaptive_sample_pos.empty())
                std::cout << "adaptive samples: " << format_size(7 * sizeof(pos_t) * _adaptive_sample_pos.size()) << std::endl;
        }

        if (!_SA_delta.empty()) std::cout << "SA_delta: " << format_size(_SA_delta.size_in_bytes()) << std::endl;
        if (!_SA_s.empty()) std::cout << "SA_s: " << format_size(_SA_s.size_in_bytes()) << std::endl;
        if (!_SA_s_.empty()) std::cout << "SA_s': " << format_size(_SA_s_.size_in_bytes()) << std::endl;

        if (z_end != 0) {
            std::cout << "lzendsa:" << std::endl;
            _lzendsa.log_data_structure_sizes();
        }
    }

    /**
     * @brief logs the index data structure sizes to the output stream out
     * @param out an output stream
     */
    void log_data_structure_sizes(std::ostream& out) const
    {
        out << " size_index=" << size_in_bytes();

        if constexpr (supports_bwsearch) {
            uint64_t size_l_ = (_M_LF.width_l_() / 8) * (r_ + 1);
            out << " size_m_lf=" << _M_LF.size_in_bytes() - size_l_;
            out << " size_l_=" << size_l_;
            
            if constexpr (byte_alphabet) {
                out << " size_l_prev=" << _L_prev.size_in_bytes();
                out << " size_l_next=" << _L_next.size_in_bytes();
            } else {
                out << " size_rs_l_=" << _RS_L_.size_in_bytes();
            }
        }

        if (int_alphabet && symbols_remapped) {
            out << " size_map_int=" << size_map_int;
            out << " size_map_ext=" << sizeof(sym_t) * sigma;
        }

        if constexpr (support == _locate_one || support == _locate_rlzsa ||
            support == _locate_rlzsa_bi_fwd || support == _locate_bi_bwd ||
            has_lzendsa
        ) {
            out << " size_sa_s=" << _SA_s.size_in_bytes();
        }

        if constexpr (has_locate_move) {
            if constexpr (support == _locate_move_bi_fwd) {
                out << " size_m_phi=" << _M_Phi.size_in_bytes();
            }

            out << " size_m_phim1=" << _M_Phi_m1.size_in_bytes();
            out << " size_sa_phim1=" << _SA_Phi_m1.size_in_bytes();
        } else if constexpr (has_rlzsa) {
            out << " size_r=" << _R.size_in_bytes();
            out << " partial_rlzsa=" << (partial_rlzsa ? 1 : 0);
            if (partial_rlzsa) {
                out << " partial_rlzsa_block_size=" << partial_rlzsa_block_size;
                out << " partial_rlzsa_gap=" << partial_rlzsa_gap;
                out << " partial_rlzsa_codec=" << static_cast<uint32_t>(partial_rlzsa_codec);
                out << " size_partial_offsets=" << _partial_rlzsa_offsets.size() * sizeof(pos_t);
                out << " size_partial_copy_offsets=" << _partial_rlzsa_copy_offsets.size() * sizeof(pos_t);
                out << " size_partial_literal_offsets=" << _partial_rlzsa_literal_offsets.size() * sizeof(pos_t);
                out << " size_partial_block_ids=" << _partial_rlzsa_block_ids.size() * sizeof(pos_t);
                out << " size_partial_pt=" << _partial_rlzsa_pt.size() * sizeof(uint8_t);
                out << " size_partial_cpl=" << _partial_rlzsa_cpl.size() * sizeof(uint16_t);
                out << " size_partial_sr=" << _partial_rlzsa_sr.size() * sizeof(pos_t);
                out << " size_partial_lp=" << _partial_rlzsa_lp.size() * sizeof(pos_t);
                out << " size_partial_block_ids_varint=" << partial_delta_varbyte_size(_partial_rlzsa_block_ids);
                out << " size_partial_offsets_varint=" << partial_delta_varbyte_size(_partial_rlzsa_offsets);
                out << " size_partial_copy_offsets_varint=" << partial_delta_varbyte_size(_partial_rlzsa_copy_offsets);
                out << " size_partial_literal_offsets_varint=" << partial_delta_varbyte_size(_partial_rlzsa_literal_offsets);
                out << " size_partial_pt_bitvector=" << (_partial_rlzsa_pt.size() + 7) / 8;
                out << " size_partial_cpl_varint=" << partial_value_varbyte_size(_partial_rlzsa_cpl);
                out << " size_partial_sr_varint=" << partial_zigzag_delta_varbyte_size(_partial_rlzsa_sr);
            } else {
                out << " size_cpl=" << (z_c + 2) * sizeof(uint16_t);
                out << " size_scp=" << _SCP_S.size_in_bytes();
                out << " size_sr=" << _SR.size_in_bytes();
                out << " size_lp=" << _LP.size_in_bytes();
                out << " size_pt=" << _PT.size_in_bytes();
            }
            if (!_M_Phi_m1.empty()) out << " size_hybrid_m_phim1=" << _M_Phi_m1.size_in_bytes();
            if (!_adaptive_sample_pos.empty()) out << " size_adaptive_samples=" << 7 * sizeof(pos_t) * _adaptive_sample_pos.size();

            if constexpr (!supports_bwsearch) {
                out << " size_sa_delta=" << _SA_delta.size_in_bytes();
            }
        }

        if constexpr (support == _locate_rlzsa_bi_fwd || support == _locate_bi_bwd) {
            out << " size_sa_s_=" << _SA_s_.size_in_bytes();
        }
    }

    // ############################# PUBLIC ACCESS METHODS #############################

    /**
     * @brief returns a reference to M_LF
     * @return M_LF
     */
    inline const move_data_structure_l_<pos_t, i_sym_t>& M_LF() const
    {
        return _M_LF;
    }

    /**
     * @brief returns a reference to M_Phi^{-1}
     * @return M_Phi^{-1}
     */
    inline const move_data_structure<pos_t>& M_Phi_m1() const requires(has_locate_move || has_rlzsa)
    {
        return _M_Phi_m1;
    }

    /**
     * @brief returns a reference to M_Phi
     * @return M_Phi
     */
    inline const move_data_structure<pos_t>& M_Phi() const requires(support == _locate_move_bi_fwd)
    {
        return _M_Phi;
    }

    /**
     * @brief returns a reference to RS_L'
     * @return RS_L'
     */
    inline const rsl_t& RS_L_() const
    {
        return _RS_L_;
    }

    /**
     * @brief returns L_next[x]
     * @param x [0..num_blocks_L_() * sigma - 1] index in L_next
     * @return L_next[x]
     */
    inline pos_t L_next(pos_t x) const
        requires(byte_alphabet)
    {
        return _L_next[x];
    }

    /**
     * @brief returns the position of the first occurrence of i_sym
     *        after or in the x-th block (of size L_block_size()) in L'
     * @param blk [0..num_blocks_L_() - 1] block index
     * @param sym [0..alphabet_size() - 1] symbol
     * @return L_next[blk * sigma + sym]
     */
    inline pos_t L_next(pos_t blk, pos_t i_sym) const
        requires(byte_alphabet)
    {
        return _L_next[blk * sigma + i_sym];
    }

    /**
     * @brief returns L_prev[x]
     * @param x [0..num_blocks_L_() * sigma - 1] index in L_prev
     * @return L_prev[x]
     */
    inline pos_t L_prev(pos_t x) const
        requires(byte_alphabet)
    {
        return _L_prev[x];
    }

    /**
     * @brief returns the position of the last occurrence of i_sym
     *        before the x-th block (of size L_block_size()) in L'
     * @param blk [0..num_blocks_L_() - 1] block index
     * @param sym [0..alphabet_size() - 1] symbol
     * @return L_prev[blk * sigma + i_sym]
     */
    inline pos_t L_prev(pos_t blk, pos_t i_sym) const
        requires(byte_alphabet)
    {
        return _L_prev[blk * sigma + i_sym];
    }

    /**
     * @brief returns a reference to R
     * @return R
     */
    inline const interleaved_byte_aligned_vectors<uint64_t, pos_t>& R() const requires(has_rlzsa)
    {
        return _R;
    }

    /**
     * @brief returns a reference to PT
     * @return PT
     */
    inline const plain_bit_vector<pos_t, true, true, true>& PT() const requires(has_rlzsa)
    {
        return _PT;
    }

    /**
     * @brief returns a reference to CPL
     * @return CPL
     */
    inline const std::vector<uint16_t>& CPL() const requires(has_rlzsa)
    {
        return _CPL;
    }

    /**
     * @brief returns a reference to SCP_S
     * @return SCP_S
     */
    inline const sd_array<pos_t>& SCP_S() const requires(has_rlzsa)
    {
        return _SCP_S;
    }

    /**
     * @brief returns a reference to SR
     * @return SR
     */
    inline const interleaved_byte_aligned_vectors<pos_t, pos_t>& SR() const requires(has_rlzsa)
    {
        return _SR;
    }

    /**
     * @brief returns a reference to LP
     * @return LP
     */
    inline const interleaved_byte_aligned_vectors<pos_t, pos_t>& LP() const requires(has_rlzsa)
    {
        return _LP;
    }

    /**
     * @brief returns R[x]
     * @param x [0..|R|-1] index in R
     * @return R[x]
     */
    inline uint64_t R(pos_t x) const requires(has_rlzsa)
    {
        return _R[x];
    }

    /**
     * @brief returns PT[x]
     * @param x [0..z-1] index in PT
     * @return PT[x]
     */
    inline bool PT(pos_t x) const requires(has_rlzsa)
    {
        return _PT[x];
    }

    /**
     * @brief returns CPL[x]
     * @param x [0..z_c-1] index in CPL
     * @return CPL[x]
     */
    inline uint16_t CPL(pos_t x) const requires(has_rlzsa)
    {
        return _CPL[x];
    }

    /**
     * @brief returns SCP_S[x]
     * @param x [0..z_c/sample_rate_scp-1] index in SCP_S
     * @return SCP_S[x]
     */
    inline pos_t SCP_S(pos_t x) const requires(has_rlzsa)
    {
        return _SCP_S.select_1(x + 1);
    }

    /**
     * @brief returns SR[x]
     * @param x [0..z_r-1] index in SR
     * @return SR[x]
     */
    inline pos_t SR(pos_t x) const requires(has_rlzsa)
    {
        return _SR[x];
    }

    /**
     * @brief returns LP[x]
     * @param x [0..z_l-1] index in LP
     * @return LP[x]
     */
    inline pos_t LP(pos_t x) const requires(has_rlzsa)
    {
        return _LP[x];
    }

    /**
     * @brief returns LP[x]
     * @param x [0..z_l-1] index in LP
     * @return LP[x]
     */
    inline const lzendsa_encoding& lzend() const requires(has_lzendsa)
    {
        return _lzendsa;
    }

    /**
     * @brief returns SA_Phi^{-1}[x]
     * @param x [0..r''-1]
     * @return SA_Phi^{-1}[x]
     */
    inline pos_t SA_Phi_m1(pos_t x) const requires(support == _locate_move)
    {
        return _SA_Phi_m1[x];
    }

    /**
     * @brief returns SA_s[x]
     * @param x [0..r'-1] the starting position of the x-th input
     * interval in M_LF must be a starting position of a bwt run
     * @return SA_s[x]
     */
    inline pos_t SA_s(pos_t x) const
        requires(supports_locate)
    {
        if constexpr (support == _locate_move) {
            return M_Phi_m1().q(SA_Phi_m1(x));
        } else {
            return _SA_s[x];
        }
    }

    /**
     * @brief returns SA_s'[x]
     * @param x [0..r'-1] the end position of the x-th input
     * interval in M_LF must be an end position of a bwt run
     * @return SA_s'[x]
     */
    inline pos_t SA_s_(pos_t x) const
        requires(support != _count && support != _count_bi && support != _locate_rlzsa)
    {
        if constexpr (support == _locate_move) {
            if (x == r_ - 1) [[unlikely]] {
                return M_Phi_m1().p(SA_Phi_m1(0));
            } else {
                return M_Phi_m1().p(SA_Phi_m1(x + 1));
            }
        } else {
            return _SA_s_[x];
        }
    }

    /**
     * @brief returns L'[x]
     * @param x [0..r'-1]
     * @return L'[x]
     */
    inline i_sym_t L_(pos_t x) const
    {
        return _M_LF.L_(x);
    }

    /**
     * @brief reinterprets a symbol of the input symbol type as a symbol of the internal symbol type
     * @param sym symbol
     * @return sym reinterpreted as i_sym_t
     */
    i_sym_t symbol_idx(sym_t sym) const
    {
        return *reinterpret_cast<i_sym_t*>(&sym);
    }

    /**
     * @brief maps a symbol to its corresponding symbol in the internal effective alphabet
     * @param sym symbol
     * @return its corresponding symbol in the internal effective alphabet
     */
    inline i_sym_t map_symbol(sym_t sym) const
    {
        if constexpr (byte_alphabet) {
            return _map_int[symbol_idx(sym)];
        } else {
            if (symbols_remapped) {
                auto res = _map_int.find(sym);

                if (res == _map_int.end()) {
                    return 0;
                } else {
                    return (*res).second;
                }
            } else {
                return symbol_idx(sym);
            }
        }
    }

    /**
     * @brief maps a symbol that occurs in the internal effective alphabet to its corresponding
     *        symbol in the input
     * @param sym a symbol that occurs in the internal effective alphabet
     * @return its corresponding symbol in the input
     */
    inline sym_t unmap_symbol(i_sym_t sym) const
    {
        if constexpr (byte_alphabet) {
            return _map_ext[symbol_idx(sym)];
        } else {
            return symbols_remapped ? _map_ext[sym] : sym;
        }
    }

    /**
     * @brief returns D_e[i]
     * @param i [0..max_revert_threads()-2]
     * @return D_e[i]
     */
    inline std::pair<pos_t, pos_t> D_e(uint16_t i) const
    {
        return _D_e[i];
    }

    // ############################# QUERY METHODS #############################

    /**
     * @brief returns L[i], where $ = 0, so if the input contained 0, the output is not equal to the real bwt
     * @param x [0..input size]
     * @return L[i]
     */
    inline sym_t BWT(pos_t i) const;

    /**
     * @brief returns SA[i]
     * @param x [0..input size]
     * @return SA[i]
     */
    pos_t SA(pos_t i) const
        requires(supports_multiple_locate);

    /**
     * @brief stores the variables needed to perform count- and locate-queries
     */
    struct query_context_t {
    protected:
        pos_t l; // length of the currently matched pattern
        pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z; // variables for backward search
        int64_t y, z; // variables for backward search
        pos_t i; // current position in the suffix array interval
        pos_t s; // current suffix s = SA[i] in the suffix array interval
        pos_t s_; // index of the input inteval of M_Phi^{-1} containing s
        pos_t x_p, x_lp, x_cp, x_r, s_np; // variables for decoding the rlzsa

        const move_r<support, sym_t, pos_t>* idx; // index to query

    public:
        /**
         * @brief constructs a new query context for the index idx
         * @param idx an index
         */
        query_context_t(const move_r<support, sym_t, pos_t>& idx)
        {
            this->idx = &idx;
            reset();
        }

        /**
         * @brief resets the query context to an empty pattern
         */
        inline void reset()
        {
            idx->init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);
            l = 0;
            i = b;
        }

        /**
         * @brief returns the length of the currently matched pattern
         * @return length of the currently matched pattern
         */
        inline pos_t length() const
        {
            return l;
        }

        /**
         * @brief returns the overall number of occurrences of the currently matched pattern
         * @return overall number of occurrences
         */
        inline pos_t num_occ() const
        {
            return e >= b ? e - b + 1 : 0;
        }

        /**
         * @brief returns the number of remaining (not yet reported) occurrences of the currently matched pattern
         * @return number of remaining occurrences
         */
        inline pos_t num_occ_rem() const
            requires(supports_multiple_locate)
        {
            return e >= i ? e - i + 1 : 0;
        }

        /**
         * @brief returns the suffix array interval of the currently matched pattern
         * @return suffix array interval
         */
        inline std::pair<pos_t, pos_t> sa_interval() const
        {
            return std::make_pair(b, e);
        }

        /**
         * @brief prepends sym to the currently matched pattern P; if symP occurs in the input, true is
         * returned and the query context is adjusted to store the information for the pattern symP; else,
         * false is returned and the query context is not modified
         * @param sym
         * @return whether symP occurs in the input
         */
        bool prepend(sym_t sym);

        /**
         * @brief reports the next occurrence of the currently matched pattern
         * @return next occurrence
         */
        inline pos_t next_occ() requires(supports_multiple_locate && !has_lzendsa);

        /**
         * @brief reports one occurrence of the currently matched pattern
         * @return an occurrence
         */
        inline pos_t one_occ() const requires(supports_locate);

        /**
         * @brief locates the remaining (not yet reported) occurrences of the currently matched pattern
         * @return vector containing the occurrences
         */
        template <typename report_fnc_t>
        void locate(report_fnc_t report) requires(supports_multiple_locate);

        /**
         * @brief locates the remaining (not yet reported) occurrences of the currently matched pattern
         * @return vector containing the occurrences
         */
        std::vector<pos_t> locate() requires(supports_multiple_locate)
        {
            std::vector<pos_t> Occ;
            Occ.reserve(num_occ_rem());
            locate([&](pos_t occ){Occ.emplace_back(occ);});
            return Occ;
        }
    };

    /**
     * @brief returns a query context for the index
     * @return query_context_t
     */
    inline query_context_t query() const
    {
        return query_context_t(*this);
    }

    /**
     * @brief initializes the variables to start a new backward search
     * @param b Left interval limit of the suffix array interval.
     * @param e Right interval limit of the suffix array interval.
     * @param b_ index of the input interval in M_LF containing b.
     * @param e_ index of the input interval in M_LF containing e.
     * @param hat_b_ap_y \hat{b}'_y
     * @param y y
     * @param hat_e_ap_z \hat{e}'_z
     * @param z z
     */
    inline void init_backward_search(
        pos_t& b, pos_t& e,
        pos_t& b_, pos_t& e_,
        pos_t& hat_b_ap_y, int64_t& y,
        pos_t& hat_e_ap_z, int64_t& z) const
    {
        b = 0;
        e = n - 1;
        b_ = 0;
        e_ = r_ - 1;
        hat_b_ap_y = 0;
        y = -1;
        hat_e_ap_z = r_ - 1;
        z = -1;
    }

    /**
     * @brief prepends sym to the currently matched pattern P, adjusts the variables to store
     * the query context for the pattern symP and returns whether symP occurs in the input
     * @param sym next symbol to match
     * @param b Left interval limit of the suffix array interval.
     * @param e Right interval limit of the suffix array interval.
     * @param b_ index of the input interval in M_LF containing b.
     * @param e_ index of the input interval in M_LF containing e.
     * @param hat_b_ap_y \hat{b}'_y
     * @param y y
     * @param hat_e_ap_z \hat{e}'_z
     * @param z z
     * @return whether symP occurs in the input
     */
    bool backward_search_step(
        sym_t sym,
        pos_t& b, pos_t& e,
        pos_t& b_, pos_t& e_,
        pos_t& hat_b_ap_y, int64_t& y,
        pos_t& hat_e_ap_z, int64_t& z) const;

    /**
     * @brief Sets the up a Phi^{-1}-move-pair for the suffix array sample at the starting position of the x-th input interval in M_LF
     * @param x an input interval in M_LF (the end position of the x-th input interval in M_LF must be a starting position of a BWT run)
     * @param s variable to store the suffix array sample at position M_LF.p[x]
     * @param s_ variable to store the index of the input interval in M_Phi^{-1} containing s
     */
    inline void setup_phi_m1_move_pair(pos_t& x, pos_t& s, pos_t& s_) const requires(has_locate_move || has_rlzsa);

    /**
     * @brief prepares the variables to decode SA[b]
     * @param b left interval limit of the suffix array interval
     * @param e right interval limit of the suffix array interval
     * @param s variable to store SA[b] in
     * @param s_ index of the input interval in M_Phi^{-1} containing s
     * @param hat_b_ap_y \hat{b}'_y
     * @param y y
     */
    inline void init_phi_m1(
        pos_t& b, pos_t& e,
        pos_t& s, pos_t& s_,
        pos_t& hat_b_ap_y, int64_t& y) const requires(has_locate_move || has_rlzsa);

    inline pos_t estimate_rlzsa_crossed_phrases(pos_t b, pos_t e) const requires(has_rlzsa);

    inline bool prefer_phi_locate(pos_t b, pos_t e, uint64_t pattern_length) const requires(has_rlzsa);

    template <typename report_fnc_t>
    inline void locate_phi_interval(pos_t b, pos_t e, report_fnc_t report) const
        requires(has_rlzsa && supports_bwsearch);

    template <typename report_fnc_t>
    inline void locate_rlzsa_interval(pos_t b, pos_t e, report_fnc_t report) const
        requires(has_rlzsa && supports_bwsearch);

    template <typename report_fnc_t>
    inline bool locate_rlzsa_with_adaptive_sample(pos_t b, pos_t e, report_fnc_t report) const
        requires(has_rlzsa && supports_bwsearch);

    /**
     * @brief prepares the variables to decode SA[i] and advance to the right
     * @param i current position in the suffix array
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the current or next copy phrase) of the rlzsa
     * @param s_np starting position in the rlzsa of the next phrase of the rlzsa
     */
    inline void init_rlzsa(
        pos_t i,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const requires(has_rlzsa);

    /**
     * @brief takes an rlzsa context for advancing to the right and prepares it to advance to the left
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the current or next copy phrase) of the rlzsa
     * @param s_p starting position in the rlzsa of the next phrase of the rlzsa
     */
    void turn_rlzsa_left(
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_p) const requires(has_rlzsa);

    /**
     * @brief decodes and stores SA[i] in s and prepares the context to decode
     * SA[i-1]; the context must be prepared to decode SA[i]
     * @param i [0..n-1] position in the suffix array
     * @param s variable to store SA[i] in
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the next copy phrase) of the rlzsa
     * @param s_cp starting position in the rlzsa of the current phrase of the rlzsa
     */
    inline void prev_rlzsa(
        pos_t& i, pos_t& s,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_cp) const requires(has_rlzsa);

    /**
     * @brief decodes and stores SA[i] in s and prepares the context to decode
     * SA[i+1]; the context must be prepared to decode SA[i]
     * @param i [0..n-1] position in the suffix array
     * @param s variable to store SA[i] in
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the next copy phrase) of the rlzsa
     * @param s_np starting position in the rlzsa of the next phrase of the rlzsa
     */
    inline void next_rlzsa(
        pos_t& i, pos_t& s,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const requires(has_rlzsa);

    /**
     * @brief locates the remaining (not yet reported) occurrences of the currently matched pattern
     * @param i current position in the suffix array
     * @param e right interval limit of the suffix array interval
     * @param s current suffix array value
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the next copy phrase) of the rlzsa
     * @param s_np starting position in the rlzsa of the next phrase of the rlzsa
     * @param report function that is called with every value SA[j] as a parameter, where j in [i,e]; the values are reported from left to right
     */
    template <typename report_fnc_t>
    inline void report_rlzsa_left(
        pos_t& i, pos_t b, pos_t& s,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np,
        report_fnc_t report) const requires(has_rlzsa);

    /**
     * @brief locates the remaining (not yet reported) occurrences of the currently matched pattern
     * @param i current position in the suffix array
     * @param e right interval limit of the suffix array interval
     * @param s current suffix array value
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the next copy phrase) of the rlzsa
     * @param s_np starting position in the rlzsa of the next phrase of the rlzsa
     * @param report function that is called with every value SA[j] as a parameter, where j in [i,e]; the values are reported from left to right
     */
    template <typename report_fnc_t>
    inline void report_rlzsa_right(
        pos_t& i, pos_t e, pos_t& s,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np,
        report_fnc_t report) const requires(has_rlzsa);

    /**
     * @brief advances the rlzsa context to the right up to position e
     * @param i current position in the suffix array
     * @param e position in the suffix array to advance the context to
     * @param s current suffix array value
     * @param x_p phrase-index of the phrase of the rlzsa contianing i
     * @param x_lp literal-phrase index of the current or next literal phrase of the rlzsa
     * @param x_cp copy-phrase index of the current or next copy-phrase of the rlzsa
     * @param x_r position in R inside the current copy-phrase (or the starting position in R of the next copy phrase) of the rlzsa
     * @param s_np starting position in the rlzsa of the next phrase of the rlzsa
     */
    inline void skip_rlzsa_right(
        pos_t& i, pos_t e, pos_t& s,
        pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const requires(has_rlzsa);

    /**
     * @brief returns the number of occurrences of P in the input
     * @param P the pattern to count in the input
     * @return the number of occurrences of P in the input
     */
    inline pos_t count(const inp_t& P) const;

    /**
     * @brief locates the pattern P in the input
     * @param P the pattern to locate in the input
     * @param report function that is called with every occurrence of P in the input as a parameter
     */
    template <typename report_fnc_t>
    inline void locate(const inp_t& P, report_fnc_t report) const
        requires(supports_multiple_locate);

    /**
     * @brief locates the pattern P in the input
     * @param P the pattern to locate in the input
     * @return a vector containing the occurrences of P in the input
     */
    inline std::vector<pos_t> locate(const inp_t& P) const
        requires(supports_multiple_locate)
    {
        std::vector<pos_t> Occ;
        locate(P, [&](pos_t occ){Occ.emplace_back(occ);});
        return Occ;
    }

    inline bool has_hybrid_locate() const requires(has_rlzsa)
    {
        return hybrid_locate && !_M_Phi_m1.empty();
    }

    inline void set_hybrid_locate(bool enabled) requires(has_rlzsa)
    {
        hybrid_locate = enabled && !_M_Phi_m1.empty();
    }

    inline void set_hybrid_cost_model(
        uint32_t phi_threshold,
        uint32_t phi_min_occ,
        uint32_t phi_max_pattern,
        double cost_phi,
        double cost_rlz_init,
        double cost_rlz_phrase,
        double cost_rlz_decode) requires(has_rlzsa)
    {
        hybrid_phi_threshold = phi_threshold;
        hybrid_phi_min_occ = phi_min_occ;
        hybrid_phi_max_pattern = phi_max_pattern;
        hybrid_cost_phi = cost_phi;
        hybrid_cost_rlz_init = cost_rlz_init;
        hybrid_cost_rlz_phrase = cost_rlz_phrase;
        hybrid_cost_rlz_decode = cost_rlz_decode;
    }

    inline uint64_t hybrid_phi_query_count() const requires(has_rlzsa)
    {
        return hybrid_phi_queries;
    }

    inline uint64_t hybrid_rlzsa_query_count() const requires(has_rlzsa)
    {
        return hybrid_rlzsa_queries;
    }

    inline uint64_t hybrid_phi_occurrence_count() const requires(has_rlzsa)
    {
        return hybrid_phi_occurrences;
    }

    inline uint64_t hybrid_rlzsa_occurrence_count() const requires(has_rlzsa)
    {
        return hybrid_rlzsa_occurrences;
    }

    inline uint64_t hybrid_model_query_count() const requires(has_rlzsa)
    {
        return hybrid_model_queries;
    }

    inline uint64_t hybrid_phrase_estimate_query_count() const requires(has_rlzsa)
    {
        return hybrid_phrase_estimate_queries;
    }

    inline void reset_hybrid_query_counters() const requires(has_rlzsa)
    {
        hybrid_phi_queries = 0;
        hybrid_rlzsa_queries = 0;
        hybrid_phi_occurrences = 0;
        hybrid_rlzsa_occurrences = 0;
        hybrid_model_queries = 0;
        hybrid_phrase_estimate_queries = 0;
        block_hybrid_queries = 0;
        block_hybrid_low_occ_queries = 0;
        block_hybrid_phi_blocks = 0;
        block_hybrid_rlzsa_blocks = 0;
        block_hybrid_phi_occurrences = 0;
        block_hybrid_rlzsa_occurrences = 0;
        adaptive_sample_queries = 0;
        adaptive_sample_hits = 0;
        adaptive_sample_exact_hits = 0;
        adaptive_sample_predecessor_hits = 0;
        adaptive_sample_misses = 0;
        adaptive_sample_skipped_by_occ = 0;
        adaptive_sample_occurrences = 0;
        adaptive_sample_distance_sum = 0;
    }

    inline uint64_t num_adaptive_samples() const requires(has_rlzsa)
    {
        return _adaptive_sample_pos.size();
    }

    inline uint64_t adaptive_sample_size_in_bytes() const requires(has_rlzsa)
    {
        return 7 * sizeof(pos_t) * _adaptive_sample_pos.size();
    }

    inline uint64_t adaptive_sample_query_count() const requires(has_rlzsa)
    {
        return adaptive_sample_queries;
    }

    inline uint64_t adaptive_sample_hit_count() const requires(has_rlzsa)
    {
        return adaptive_sample_hits;
    }

    inline uint64_t adaptive_sample_exact_hit_count() const requires(has_rlzsa)
    {
        return adaptive_sample_exact_hits;
    }

    inline uint64_t adaptive_sample_predecessor_hit_count() const requires(has_rlzsa)
    {
        return adaptive_sample_predecessor_hits;
    }

    inline uint64_t adaptive_sample_miss_count() const requires(has_rlzsa)
    {
        return adaptive_sample_misses;
    }

    inline uint64_t adaptive_sample_skipped_by_occ_count() const requires(has_rlzsa)
    {
        return adaptive_sample_skipped_by_occ;
    }

    inline uint64_t adaptive_sample_occurrence_count() const requires(has_rlzsa)
    {
        return adaptive_sample_occurrences;
    }

    inline uint64_t adaptive_sample_distance_total() const requires(has_rlzsa)
    {
        return adaptive_sample_distance_sum;
    }

    inline uint32_t adaptive_sample_max_occurrence_threshold() const requires(has_rlzsa)
    {
        return adaptive_sample_max_occ;
    }

    inline uint32_t adaptive_sample_min_occurrence_threshold() const requires(has_rlzsa)
    {
        return adaptive_sample_min_occ;
    }

    inline uint32_t adaptive_sample_max_distance_threshold() const requires(has_rlzsa)
    {
        return adaptive_sample_max_distance;
    }

    inline uint64_t block_hybrid_query_count() const requires(has_rlzsa)
    {
        return block_hybrid_queries;
    }

    inline uint64_t block_hybrid_low_occ_query_count() const requires(has_rlzsa)
    {
        return block_hybrid_low_occ_queries;
    }

    inline uint64_t block_hybrid_phi_block_count() const requires(has_rlzsa)
    {
        return block_hybrid_phi_blocks;
    }

    inline uint64_t block_hybrid_rlzsa_block_count() const requires(has_rlzsa)
    {
        return block_hybrid_rlzsa_blocks;
    }

    inline uint64_t block_hybrid_phi_occurrence_count() const requires(has_rlzsa)
    {
        return block_hybrid_phi_occurrences;
    }

    inline uint64_t block_hybrid_rlzsa_occurrence_count() const requires(has_rlzsa)
    {
        return block_hybrid_rlzsa_occurrences;
    }

    inline void rebuild_adaptive_sample_lookup() requires(has_rlzsa)
    {
        _adaptive_sample_lookup.clear();
        _adaptive_sample_lookup.reserve(_adaptive_sample_pos.size());
        for (pos_t i = 0; i < _adaptive_sample_pos.size(); i++) {
            _adaptive_sample_lookup[_adaptive_sample_pos[i]] = i;
        }
    }

    void build_adaptive_samples(
        std::istream& patterns,
        uint64_t sample_budget,
        bool uniform_sampling = false,
        uint32_t min_occ = 16,
        uint32_t max_occ = 4096,
        uint32_t max_distance = 0) requires(has_rlzsa && supports_bwsearch);

    inline void build_partial_rlzsa_from_blocks(
        pos_t block_size,
        pos_t stored_gap,
        const std::vector<pos_t>& selected_blocks) requires(has_rlzsa && supports_bwsearch);

    inline void build_partial_rlzsa(pos_t block_size, pos_t enhance_gap)
        requires(has_rlzsa && supports_bwsearch);

    inline void build_partial_rlzsa_adaptive(
        pos_t block_size,
        pos_t budget_blocks,
        std::istream& training_patterns,
        pos_t train_occ_threshold) requires(has_rlzsa && supports_bwsearch);

    inline bool has_partial_rlzsa() const requires(has_rlzsa)
    {
        return partial_rlzsa;
    }

    inline void set_partial_rlzsa_varint_codec(bool enabled) requires(has_rlzsa)
    {
        partial_rlzsa_codec = enabled ? 1 : 0;
    }

    inline void set_partial_rlzsa_codec(uint8_t codec) requires(has_rlzsa)
    {
        partial_rlzsa_codec = codec;
    }

    static uint64_t partial_varbyte_size(uint64_t value)
    {
        uint64_t bytes = 1;
        while (value >= 128) {
            value >>= 7;
            bytes++;
        }
        return bytes;
    }

    static uint64_t partial_zigzag_encode(int64_t value)
    {
        return value >= 0
            ? static_cast<uint64_t>(value) << 1
            : (static_cast<uint64_t>(-(value + 1)) << 1) + 1;
    }

    static void partial_write_varbyte(std::ostream& out, uint64_t value)
    {
        while (value >= 128) {
            uint8_t byte = static_cast<uint8_t>((value & 127) | 128);
            out.write((char*) &byte, 1);
            value >>= 7;
        }
        uint8_t byte = static_cast<uint8_t>(value);
        out.write((char*) &byte, 1);
    }

    static uint64_t partial_read_varbyte(std::istream& in)
    {
        uint64_t value = 0;
        uint32_t shift = 0;
        while (true) {
            uint8_t byte = 0;
            in.read((char*) &byte, 1);
            value |= static_cast<uint64_t>(byte & 127) << shift;
            if ((byte & 128) == 0) break;
            shift += 7;
        }
        return value;
    }

    template <typename value_t>
    static uint64_t partial_delta_varbyte_size(const std::vector<value_t>& values)
    {
        uint64_t bytes = 0;
        uint64_t prev = 0;
        for (auto value : values) {
            uint64_t v = static_cast<uint64_t>(value);
            bytes += partial_varbyte_size(v - prev);
            prev = v;
        }
        return bytes;
    }

    template <typename value_t>
    static uint64_t partial_value_varbyte_size(const std::vector<value_t>& values)
    {
        uint64_t bytes = 0;
        for (auto value : values) bytes += partial_varbyte_size(static_cast<uint64_t>(value));
        return bytes;
    }

    template <typename value_t>
    static uint64_t partial_zigzag_delta_varbyte_size(const std::vector<value_t>& values)
    {
        uint64_t bytes = 0;
        int64_t prev = 0;
        for (auto value : values) {
            int64_t v = static_cast<int64_t>(value);
            bytes += partial_varbyte_size(partial_zigzag_encode(v - prev));
            prev = v;
        }
        return bytes;
    }

    template <typename value_t>
    static std::string partial_encode_delta_varbyte(const std::vector<value_t>& values)
    {
        std::ostringstream bytes;
        uint64_t prev = 0;
        for (auto value : values) {
            uint64_t cur = static_cast<uint64_t>(value);
            partial_write_varbyte(bytes, cur - prev);
            prev = cur;
        }
        return bytes.str();
    }

    template <typename value_t>
    static std::string partial_encode_value_varbyte(const std::vector<value_t>& values)
    {
        std::ostringstream bytes;
        for (auto value : values) partial_write_varbyte(bytes, static_cast<uint64_t>(value));
        return bytes.str();
    }

    template <typename value_t>
    static std::string partial_encode_zigzag_delta_varbyte(const std::vector<value_t>& values)
    {
        std::ostringstream bytes;
        int64_t prev = 0;
        for (auto value : values) {
            int64_t cur = static_cast<int64_t>(value);
            partial_write_varbyte(bytes, partial_zigzag_encode(cur - prev));
            prev = cur;
        }
        return bytes.str();
    }

    static std::string partial_encode_bits(const std::vector<uint8_t>& values)
    {
        std::string bytes((values.size() + 7) / 8, '\0');
        for (uint64_t i = 0; i < values.size(); i++) {
            if (values[i] != 0) bytes[i / 8] = static_cast<char>(static_cast<uint8_t>(bytes[i / 8]) | (uint8_t{1} << (i % 8)));
        }
        return bytes;
    }

    template <typename value_t>
    static void partial_decode_delta_varbyte(std::istream& in, std::vector<value_t>& values)
    {
        uint64_t prev = 0;
        for (auto& value : values) {
            prev += partial_read_varbyte(in);
            value = static_cast<value_t>(prev);
        }
    }

    template <typename value_t>
    static void partial_decode_value_varbyte(std::istream& in, std::vector<value_t>& values)
    {
        for (auto& value : values) value = static_cast<value_t>(partial_read_varbyte(in));
    }

    template <typename value_t>
    static void partial_decode_zigzag_delta_varbyte(std::istream& in, std::vector<value_t>& values)
    {
        int64_t prev = 0;
        for (auto& value : values) {
            uint64_t zz = partial_read_varbyte(in);
            int64_t delta = (zz & 1) ? -static_cast<int64_t>((zz >> 1) + 1) : static_cast<int64_t>(zz >> 1);
            prev += delta;
            value = static_cast<value_t>(prev);
        }
    }

    static void partial_decode_bits(std::istream& in, std::vector<uint8_t>& values)
    {
        std::vector<uint8_t> bytes((values.size() + 7) / 8);
        if (!bytes.empty()) read_from_file(in, (char*) bytes.data(), bytes.size());
        for (uint64_t i = 0; i < values.size(); i++) values[i] = (bytes[i / 8] >> (i % 8)) & 1;
    }

    void write_partial_field_statistics(
        const std::string& distribution_path,
        const std::string& saving_path,
        const std::string& label) const requires(has_rlzsa)
    {
        auto append_header = [](const std::string& path, const std::string& header) {
            bool need_header = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
            std::ofstream out(path, std::ios::app);
            if (need_header) out << header << '\n';
        };

        append_header(distribution_path, "label,field,metric,count,min,median,mean,p90,p99,max,zeros,ones");
        append_header(saving_path, "label,field,current_bytes,estimated_bytes,saving_bytes,saving_percent,method");
        std::ofstream dist(distribution_path, std::ios::app);
        std::ofstream save(saving_path, std::ios::app);

        auto emit_distribution = [&](const std::string& field, const std::string& metric, auto values, uint64_t zeros = 0, uint64_t ones = 0) {
            using value_t = typename decltype(values)::value_type;
            std::vector<uint64_t> sorted;
            sorted.reserve(values.size());
            for (value_t value : values) sorted.emplace_back(static_cast<uint64_t>(value));
            std::sort(sorted.begin(), sorted.end());
            double mean = 0.0;
            if (!sorted.empty()) {
                long double sum = 0.0;
                for (uint64_t value : sorted) sum += value;
                mean = static_cast<double>(sum / sorted.size());
            }
            auto percentile = [&](double p) -> uint64_t {
                if (sorted.empty()) return 0;
                uint64_t idx = static_cast<uint64_t>(std::ceil(p * static_cast<double>(sorted.size()))) - 1;
                idx = std::min<uint64_t>(idx, sorted.size() - 1);
                return sorted[idx];
            };
            dist << label << ',' << field << ',' << metric << ',' << sorted.size()
                 << ',' << (sorted.empty() ? 0 : sorted.front())
                 << ',' << percentile(0.50)
                 << ',' << mean
                 << ',' << percentile(0.90)
                 << ',' << percentile(0.99)
                 << ',' << (sorted.empty() ? 0 : sorted.back())
                 << ',' << zeros
                 << ',' << ones
                 << '\n';
        };

        auto emit_saving = [&](const std::string& field, uint64_t current_bytes, uint64_t estimated_bytes, const std::string& method) {
            int64_t saving = static_cast<int64_t>(current_bytes) - static_cast<int64_t>(estimated_bytes);
            double saving_percent = current_bytes == 0 ? 0.0 : (100.0 * static_cast<double>(saving) / static_cast<double>(current_bytes));
            save << label << ',' << field << ',' << current_bytes << ',' << estimated_bytes
                 << ',' << saving << ',' << saving_percent << ',' << method << '\n';
        };

        std::vector<pos_t> block_gaps;
        block_gaps.reserve(_partial_rlzsa_block_ids.size());
        pos_t prev_block = 0;
        for (auto block : _partial_rlzsa_block_ids) {
            block_gaps.emplace_back(block - prev_block);
            prev_block = block;
        }
        emit_distribution("block_ids", "value", _partial_rlzsa_block_ids);
        emit_distribution("block_ids", "adjacent_gap", block_gaps);
        emit_saving("block_ids", _partial_rlzsa_block_ids.size() * sizeof(pos_t),
                    partial_delta_varbyte_size(_partial_rlzsa_block_ids), "gap_varbyte");

        emit_distribution("localR_intervals", "start", _partial_local_r_interval_starts);
        emit_distribution("localR_intervals", "length", _partial_local_r_interval_lengths);
        std::vector<pos_t> interval_start_gaps;
        interval_start_gaps.reserve(_partial_local_r_interval_starts.size());
        pos_t prev_start = 0;
        for (auto start : _partial_local_r_interval_starts) {
            interval_start_gaps.emplace_back(start - prev_start);
            prev_start = start;
        }
        emit_distribution("localR_intervals", "start_gap", interval_start_gaps);
        emit_saving("localR_intervals", 2 * _partial_local_r_interval_starts.size() * sizeof(pos_t),
                    partial_delta_varbyte_size(_partial_local_r_interval_starts) +
                    partial_value_varbyte_size(_partial_local_r_interval_lengths), "start_gap_plus_length_varbyte");

        std::vector<uint64_t> sr_delta_zigzag;
        sr_delta_zigzag.reserve(_partial_rlzsa_sr.size());
        int64_t prev_sr = 0;
        for (auto sr : _partial_rlzsa_sr) {
            int64_t cur = static_cast<int64_t>(sr);
            sr_delta_zigzag.emplace_back(partial_zigzag_encode(cur - prev_sr));
            prev_sr = cur;
        }
        emit_distribution("SR", "value", _partial_rlzsa_sr);
        emit_distribution("SR", "zigzag_delta", sr_delta_zigzag);
        emit_saving("SR", _partial_rlzsa_sr.size() * sizeof(pos_t),
                    partial_zigzag_delta_varbyte_size(_partial_rlzsa_sr), "zigzag_delta_varbyte");

        emit_distribution("CPL", "value", _partial_rlzsa_cpl);
        emit_saving("CPL", _partial_rlzsa_cpl.size() * sizeof(uint16_t),
                    partial_value_varbyte_size(_partial_rlzsa_cpl), "value_varbyte");

        uint64_t pt_zeros = 0;
        uint64_t pt_ones = 0;
        for (uint8_t value : _partial_rlzsa_pt) value == 0 ? pt_zeros++ : pt_ones++;
        emit_distribution("PT", "value", _partial_rlzsa_pt, pt_zeros, pt_ones);
        emit_saving("PT", _partial_rlzsa_pt.size() * sizeof(uint8_t),
                    (_partial_rlzsa_pt.size() + 7) / 8, "bitvector_estimate");

        auto emit_anchor_field = [&](const std::string& field, const std::vector<pos_t>& values) {
            emit_distribution(field, "value", values);
            std::vector<pos_t> gaps;
            gaps.reserve(values.size());
            pos_t prev = 0;
            for (auto value : values) {
                gaps.emplace_back(value - prev);
                prev = value;
            }
            emit_distribution(field, "adjacent_gap", gaps);
            emit_saving(field, values.size() * sizeof(pos_t), partial_delta_varbyte_size(values), "delta_varbyte");
        };
        emit_anchor_field("phrase_offsets", _partial_rlzsa_offsets);
        emit_anchor_field("copy_offsets", _partial_rlzsa_copy_offsets);
        emit_anchor_field("literal_offsets", _partial_rlzsa_literal_offsets);
        emit_distribution("LP", "value", _partial_rlzsa_lp);
        emit_saving("LP", _partial_rlzsa_lp.size() * sizeof(pos_t),
                    partial_value_varbyte_size(_partial_rlzsa_lp), "value_varbyte");
    }

    void dump_vrrlzsa_field_frequencies(const std::string& output_dir) const requires(has_rlzsa)
    {
        std::filesystem::create_directories(output_dir);
        std::unordered_map<uint64_t, uint64_t> sr_freq;
        std::unordered_map<uint64_t, uint64_t> cpl_freq;

        uint64_t total_phrases = 0;
        uint64_t total_copy_phrases = 0;
        if (partial_rlzsa) {
            total_phrases = _partial_rlzsa_pt.size();
            total_copy_phrases = _partial_rlzsa_sr.size();
            for (auto value : _partial_rlzsa_sr) sr_freq[static_cast<uint64_t>(value)]++;
            for (auto value : _partial_rlzsa_cpl) cpl_freq[static_cast<uint64_t>(value)]++;
        } else {
            total_phrases = z;
            total_copy_phrases = z_c;
            for (pos_t i = 0; i < z_c; i++) {
                sr_freq[static_cast<uint64_t>(SR(i))]++;
                cpl_freq[static_cast<uint64_t>(CPL(i))]++;
            }
        }

        auto entropy = [](const auto& freq, uint64_t total) -> double {
            if (total == 0) return 0.0;
            long double h = 0.0;
            for (const auto& [_, count] : freq) {
                long double p = static_cast<long double>(count) / static_cast<long double>(total);
                h -= p * std::log2(p);
            }
            return static_cast<double>(h);
        };

        auto sorted_freq = [](const auto& freq) {
            std::vector<std::pair<uint64_t, uint64_t>> values(freq.begin(), freq.end());
            std::sort(values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.second != rhs.second) return lhs.second > rhs.second;
                return lhs.first < rhs.first;
            });
            return values;
        };

        auto write_rank_csv = [&](const std::string& name, const auto& freq) {
            std::ofstream out(output_dir + "/" + name);
            out << "rank,value,frequency\n";
            auto values = sorted_freq(freq);
            uint64_t rank = 1;
            for (const auto& [value, count] : values) {
                out << rank++ << ',' << value << ',' << count << '\n';
            }
        };

        write_rank_csv("sr_rank_frequency.csv", sr_freq);
        write_rank_csv("cpl_rank_frequency.csv", cpl_freq);
        write_rank_csv("p_rank_frequency.csv", sr_freq);
        write_rank_csv("l_rank_frequency.csv", cpl_freq);

        auto sr_values = sorted_freq(sr_freq);
        auto cpl_values = sorted_freq(cpl_freq);
        std::ofstream summary(output_dir + "/field_summary.csv");
        summary
            << "partial_rlzsa,total_phrases,total_copy_phrases,distinct_sr_values,distinct_cpl_values,"
            << "top1_sr_frequency,top1_cpl_frequency,sr_entropy,cpl_entropy\n";
        summary
            << (partial_rlzsa ? 1 : 0) << ','
            << total_phrases << ','
            << total_copy_phrases << ','
            << sr_freq.size() << ','
            << cpl_freq.size() << ','
            << (sr_values.empty() ? 0 : sr_values.front().second) << ','
            << (cpl_values.empty() ? 0 : cpl_values.front().second) << ','
            << entropy(sr_freq, total_copy_phrases) << ','
            << entropy(cpl_freq, total_copy_phrases) << '\n';
    }

    template <typename report_fnc_t>
    inline void locate_phi(const inp_t& P, report_fnc_t report) const
        requires(has_rlzsa && supports_bwsearch);

    inline std::vector<pos_t> locate_phi(const inp_t& P) const
        requires(has_rlzsa && supports_bwsearch)
    {
        std::vector<pos_t> Occ;
        locate_phi(P, [&](pos_t occ){Occ.emplace_back(occ);});
        return Occ;
    }

    template <typename report_fnc_t>
    inline void locate_rlzsa(const inp_t& P, report_fnc_t report) const
        requires(has_rlzsa);

    inline std::vector<pos_t> locate_rlzsa(const inp_t& P) const
        requires(has_rlzsa)
    {
        std::vector<pos_t> Occ;
        locate_rlzsa(P, [&](pos_t occ){Occ.emplace_back(occ);});
        return Occ;
    }

    template <typename report_fnc_t>
    inline void locate_block_hybrid(
        const inp_t& P,
        pos_t block_size,
        pos_t enhance_gap,
        pos_t occ_threshold,
        pos_t rlz_occ_threshold,
        report_fnc_t report) const
        requires(has_rlzsa && supports_bwsearch);

    inline std::vector<pos_t> locate_block_hybrid(
        const inp_t& P,
        pos_t block_size,
        pos_t enhance_gap,
        pos_t occ_threshold,
        pos_t rlz_occ_threshold = 0) const
        requires(has_rlzsa && supports_bwsearch)
    {
        std::vector<pos_t> Occ;
        locate_block_hybrid(P, block_size, enhance_gap, occ_threshold, rlz_occ_threshold, [&](pos_t occ){Occ.emplace_back(occ);});
        return Occ;
    }

    // ############################# RETRIEVE-RANGE METHODS #############################

    struct retrieve_params {
        pos_t l = 1; // left range limit
        pos_t r = 0; // right range limit
        uint16_t num_threads = omp_get_max_threads(); // maximum number of threads to use
        // maximum number of bytes to allocate (only applicable if the method writes data to a file; default (if set to -1): ~ (r-l+1)/500)
        int64_t max_bytes_alloc = -1;
    };

protected:
    /**
     * @brief adjusts retrieve parameters, ensures (0 <= l <= r <= range_max); if l > r, it sets [l,r] <- [0,range_max]
     * @param params retrieve parameters to adjust
     * @param range_max maximum value for r
     */
    inline static void adjust_retrieve_params(retrieve_params& params, pos_t range_max)
    {
        if (params.l > params.r) {
            params.l = 0;
            params.r = range_max;
        }

        params.r = std::min(params.r, range_max);
    }

    /**
     * @brief executes retrieve_method with the parameters l, r and num_threads, buffers the output in num_threads
     * buffers and num_threads temporary files and then writes the temporary files into the file out
     * @tparam output_t type of the output data
     * @tparam output_reversed controls, whether the output should be reversed
     * @param retrieve_method function, whiches output should be buffered
     * @param file_name name of the file to write the output to
     * @param params parameters
     */
    template <typename output_t, bool output_reversed>
    void retrieve_range(
        void (move_r<support, sym_t, pos_t>::*retrieve_method)(std::function<void(pos_t, output_t)>&&, retrieve_params) const,
        std::string file_name, retrieve_params params) const;

public:
    /**
     * @brief returns the bwt in the range [l,r] (0 <= l <= r <= input size), else
     * if l > r, then the whole bwt is returned (default); $ = 0, so if the input contained 0, the output is not
     * equal to the real bwt
     * @param params parameters
     * @return the bwt range [l,r]
     */
    inp_t BWT_range(retrieve_params params = {}) const
    {
        adjust_retrieve_params(params, n - 1);
        inp_t L;
        no_init_resize(L, params.r - params.l + 1);
        BWT_range([&](pos_t i, sym_t c) { L[i - params.l] = c; }, params);
        return L;
    }

    /**
     * @brief reports the characters in the bwt in the range [l,r] (0 <= l <= r <= input size), else if l > r, then all
     * characters of the bwt are reported (default); $ = 0, so if the input contained 0, the output is not equal to the real bwt
     * @param report function that is called with every tuple (i,c) as a parameter, where i in [l,r] and c = L[i]; if num_threads = 1,
     * then the values are reported from left to right, if num_threads > 1, the order may vary
     * @param params parameters
     */
    template <typename report_fnc_t>
    void BWT_range(report_fnc_t report, retrieve_params params = {}) const;

    /**
     * @brief writes the characters in the bwt in the range [l,r] blockwise to the file out (0 <= l <= r <= input size), else if
     * l > r, then the whole bwt is written (default); $ = 0, so if the input contained 0, the output is not equal to the real bwt
     * @param file_name name of the file to write the bwt to
     * @param params parameters
     */
    void BWT_range(std::string file_name, retrieve_params params = {}) const
    {
        adjust_retrieve_params(params, n - 1);
        retrieve_range<sym_t, false>(&move_r<support, sym_t, pos_t>::BWT, file_name, params);
    }

    /**
     * @brief returns the input in the range [l,r] (0 <= l <= r < input size), else
     * if l > r, then the whole input is returned (default)
     * @param params parameters
     * @return the input range [l,r]
     */
    inp_t revert_range(retrieve_params params = {}) const
    {
        adjust_retrieve_params(params, n - 2);
        inp_t input;
        no_init_resize(input, params.r - params.l + 1);
        revert_range([&](pos_t i, sym_t c) { input[i - params.l] = c; }, params);
        return input;
    }

    /**
     * @brief reports the characters in the input in the range [l,r] (0 <= l <= r < input size), else if l > r, then
     * all characters of the input are reported (default); if num_threads = 1, then the values are reported from right
     * to left, if num_threads > 1, the order may vary
     * @param report function that is called with every tuple (i,c) as a parameter, where i in [l,r] and c = input[i]
     * @param params parameters
     */
    template <typename report_fnc_t>
    void revert_range(report_fnc_t report, retrieve_params params = {}) const requires(supports_bwsearch);

    /**
     * @brief reverts the input in the range [l,r] blockwise and writes it to the file out (0 <= l <= r < input size),
     * else if l > r, then the whole input is reverted (default)
     * @param file_name name of the file to write the reverted input to
     * @param params parameters
     */
    void revert_range(std::string file_name, retrieve_params params = {}) const
    {
        adjust_retrieve_params(params, n - 2);
        retrieve_range<sym_t, true>(&move_r<support, sym_t, pos_t>::revert_range, file_name, params);
    }

    /**
     * @brief rebuilds and returns the suffix array in the range [l,r] (0 <= l <= r <= input size),
     * else if l > r, then the whole suffix array is rebuilt (default)
     * @param params parameters
     * @return the suffix array range [l,r]
     */
    std::vector<pos_t> SA_range(retrieve_params params = {}) const requires(supports_multiple_locate)
    {
        adjust_retrieve_params(params, n - 1);
        std::vector<pos_t> SA_rng;
        no_init_resize(SA_rng, params.r - params.l + 1);
        SA_range([&](pos_t i, pos_t v){SA_rng[i - params.l] = v;}, params);
        return SA_rng;
    }

    /**
     * @brief reports the suffix array values in the range [l,r] (0 <= l <= r <= input size), else if l > r, then the
     * whole suffix array is reported (default); if num_threads = 1, then the values are reported from left to right,
     * if num_threads > 1, the order may vary
     * @param report function that is called with every tuple (i,s) as a parameter, where i in [l,r] and s = SA[i]
     * @param params parameters
     */
    template <typename report_fnc_t>
    void SA_range(report_fnc_t report, retrieve_params params = {}) const requires(supports_multiple_locate);

    /**
     * @brief writes the values in the suffix array of the input in the range [l,r] blockwise to the file out (0 <= l <= r <= input size),
     * else if l > r, then the whole suffix array is written (default)
     * @param file_name name of the file to write the suffix array to
     * @param params parameters
     */
    void SA_range(std::string file_name, retrieve_params params = {}) const requires(supports_multiple_locate)
    {
        adjust_retrieve_params(params, n - 1);
        retrieve_range<pos_t, false>(&move_r<support, sym_t, pos_t>::SA, file_name, params);
    }

    // ############################# SERIALIZATION METHODS #############################

    /**
     * @brief stores the index to an output stream
     * @param out output stream to store the index to
     */
    void serialize(std::ostream& out) const
    {
        bool is_64_bit = std::is_same_v<pos_t, uint64_t>;
        out.write((char*) &is_64_bit, 1);
        move_r_support _support = support;
        out.write((char*) &_support, sizeof(move_r_support));

        std::streampos pos_data_structure_offsets = out.tellp();
        out.seekp(pos_data_structure_offsets + (std::streamoff)sizeof(std::streamoff), std::ios::beg);

        out.write((char*) &n, sizeof(pos_t));
        out.write((char*) &sigma, sizeof(uint32_t));
        out.write((char*) &r, sizeof(pos_t));
        out.write((char*) &r_, sizeof(pos_t));
        out.write((char*) &a, sizeof(uint16_t));
        out.write((char*) &p_r, sizeof(uint16_t));
        out.write((char*) &_l_blk_size, sizeof(pos_t));

        if (p_r > 1) {
            out.write((char*) &_D_e[0], (p_r - 1) * 2 * sizeof(pos_t));
        }

        out.write((char*) &symbols_remapped, 1);
        if (symbols_remapped) {
            if constexpr (byte_alphabet) {
                out.write((char*) &_map_int[0], 256);
                out.write((char*) &_map_ext[0], 256);
            } else {
                write_to_file(out, (char*) &_map_ext[0], sizeof(sym_t) * sigma);
                std::vector<std::pair<sym_t, i_sym_t>> map_int_vec(_map_int.begin(), _map_int.end());
                write_to_file(out, (char*) &map_int_vec[0], sizeof(std::pair<sym_t, i_sym_t>) * sigma);
            }
        }

        _M_LF.serialize(out);
        
        if constexpr (byte_alphabet) {
            _L_prev.serialize(out);
            _L_next.serialize(out);
        } else {
            _RS_L_.serialize(out);
        }

        if constexpr (support == _locate_one) {
            _SA_s.serialize(out);
        } else if constexpr (has_locate_move) {
            if constexpr (support == _locate_move_bi_fwd) {
                out.write((char*) &r___, sizeof(pos_t));
                _M_Phi.serialize(out);
            }

            out.write((char*) &r__, sizeof(pos_t));
            _M_Phi_m1.serialize(out);

            if constexpr (support == _locate_move) {
                out.write((char*) &omega_idx, 1);
                _SA_Phi_m1.serialize(out);
            } else {
                _SA_s.serialize(out);
                _SA_s_.serialize(out);
            }
        } else if constexpr (has_rlzsa) {
            out.write((char*) &z, sizeof(pos_t));
            out.write((char*) &z_l, sizeof(pos_t));
            out.write((char*) &z_c, sizeof(pos_t));

            _SA_s.serialize(out);
            _SA_s_.serialize(out);
            _R.serialize(out);
            out.write((char*) &partial_rlzsa, 1);
            if (partial_rlzsa) {
                pos_t partial_offsets = _partial_rlzsa_offsets.size();
                pos_t partial_phrases = _partial_rlzsa_pt.size();
                pos_t partial_copies = _partial_rlzsa_cpl.size();
                pos_t partial_literals = _partial_rlzsa_lp.size();
                pos_t partial_blocks = _partial_rlzsa_block_ids.size();
                if (partial_rlzsa_codec != 0) {
                    pos_t sentinel = 0;
                    uint8_t format_version = partial_rlzsa_codec;
                    out.write((char*) &sentinel, sizeof(pos_t));
                    out.write((char*) &format_version, 1);
                    out.write((char*) &partial_rlzsa_codec, 1);
                    out.write((char*) &partial_rlzsa_block_size, sizeof(pos_t));
                    out.write((char*) &partial_rlzsa_gap, sizeof(pos_t));
                    out.write((char*) &partial_offsets, sizeof(pos_t));
                    out.write((char*) &partial_phrases, sizeof(pos_t));
                    out.write((char*) &partial_copies, sizeof(pos_t));
                    out.write((char*) &partial_literals, sizeof(pos_t));
                    out.write((char*) &partial_blocks, sizeof(pos_t));

                    std::string offsets_payload = partial_rlzsa_codec >= 2 ? partial_encode_delta_varbyte(_partial_rlzsa_offsets) : "";
                    std::string copy_offsets_payload = partial_rlzsa_codec >= 2 ? partial_encode_delta_varbyte(_partial_rlzsa_copy_offsets) : "";
                    std::string literal_offsets_payload = partial_rlzsa_codec >= 2 ? partial_encode_delta_varbyte(_partial_rlzsa_literal_offsets) : "";
                    std::string block_id_payload = partial_encode_delta_varbyte(_partial_rlzsa_block_ids);
                    std::string pt_payload = partial_rlzsa_codec >= 2 ? partial_encode_bits(_partial_rlzsa_pt) : "";
                    std::string cpl_payload = partial_rlzsa_codec >= 2 ? partial_encode_value_varbyte(_partial_rlzsa_cpl) : "";
                    std::string sr_payload = partial_encode_zigzag_delta_varbyte(_partial_rlzsa_sr);

                    uint64_t offsets_payload_size = offsets_payload.size();
                    uint64_t copy_offsets_payload_size = copy_offsets_payload.size();
                    uint64_t literal_offsets_payload_size = literal_offsets_payload.size();
                    uint64_t block_id_payload_size = block_id_payload.size();
                    uint64_t pt_payload_size = pt_payload.size();
                    uint64_t cpl_payload_size = cpl_payload.size();
                    uint64_t sr_payload_size = sr_payload.size();

                    if (partial_rlzsa_codec >= 2) {
                        out.write((char*) &offsets_payload_size, sizeof(uint64_t));
                        out.write((char*) &copy_offsets_payload_size, sizeof(uint64_t));
                        out.write((char*) &literal_offsets_payload_size, sizeof(uint64_t));
                        out.write((char*) &block_id_payload_size, sizeof(uint64_t));
                        out.write((char*) &pt_payload_size, sizeof(uint64_t));
                        out.write((char*) &cpl_payload_size, sizeof(uint64_t));
                        out.write((char*) &sr_payload_size, sizeof(uint64_t));
                    } else {
                        out.write((char*) &block_id_payload_size, sizeof(uint64_t));
                        out.write((char*) &sr_payload_size, sizeof(uint64_t));
                    }

                    if (partial_rlzsa_codec >= 2) {
                        if (offsets_payload_size) write_to_file(out, (char*) offsets_payload.data(), offsets_payload_size);
                        if (copy_offsets_payload_size) write_to_file(out, (char*) copy_offsets_payload.data(), copy_offsets_payload_size);
                        if (literal_offsets_payload_size) write_to_file(out, (char*) literal_offsets_payload.data(), literal_offsets_payload_size);
                    } else {
                        if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_offsets.data(), partial_offsets * sizeof(pos_t));
                        if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_copy_offsets.data(), partial_offsets * sizeof(pos_t));
                        if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_literal_offsets.data(), partial_offsets * sizeof(pos_t));
                    }
                    if (block_id_payload_size) write_to_file(out, (char*) block_id_payload.data(), block_id_payload_size);
                    if (partial_rlzsa_codec >= 2) {
                        if (pt_payload_size) write_to_file(out, (char*) pt_payload.data(), pt_payload_size);
                    } else if (partial_phrases) {
                        write_to_file(out, (char*) _partial_rlzsa_pt.data(), partial_phrases * sizeof(uint8_t));
                    }
                    if (partial_copies) {
                        if (partial_rlzsa_codec >= 2) {
                            if (cpl_payload_size) write_to_file(out, (char*) cpl_payload.data(), cpl_payload_size);
                        } else {
                            write_to_file(out, (char*) _partial_rlzsa_cpl.data(), partial_copies * sizeof(uint16_t));
                        }
                        if (sr_payload_size) write_to_file(out, (char*) sr_payload.data(), sr_payload_size);
                    }
                    if (partial_literals) write_to_file(out, (char*) _partial_rlzsa_lp.data(), partial_literals * sizeof(pos_t));
                } else {
                    out.write((char*) &partial_rlzsa_block_size, sizeof(pos_t));
                    out.write((char*) &partial_rlzsa_gap, sizeof(pos_t));
                    out.write((char*) &partial_offsets, sizeof(pos_t));
                    out.write((char*) &partial_phrases, sizeof(pos_t));
                    out.write((char*) &partial_copies, sizeof(pos_t));
                    out.write((char*) &partial_literals, sizeof(pos_t));
                    out.write((char*) &partial_blocks, sizeof(pos_t));
                    if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_copy_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_offsets) write_to_file(out, (char*) _partial_rlzsa_literal_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_blocks) write_to_file(out, (char*) _partial_rlzsa_block_ids.data(), partial_blocks * sizeof(pos_t));
                    if (partial_phrases) write_to_file(out, (char*) _partial_rlzsa_pt.data(), partial_phrases * sizeof(uint8_t));
                    if (partial_copies) {
                        write_to_file(out, (char*) _partial_rlzsa_cpl.data(), partial_copies * sizeof(uint16_t));
                        write_to_file(out, (char*) _partial_rlzsa_sr.data(), partial_copies * sizeof(pos_t));
                    }
                    if (partial_literals) write_to_file(out, (char*) _partial_rlzsa_lp.data(), partial_literals * sizeof(pos_t));
                }
            } else {
                _SCP_S.serialize(out);
                write_to_file(out, (char*) &_CPL[0], (z_c + 2) * sizeof(uint16_t));
                _SR.serialize(out);
                _LP.serialize(out);
                _PT.serialize(out);
            }
            if constexpr (supports_bwsearch) {
                bool has_hybrid_phi = hybrid_locate && !_M_Phi_m1.empty();
                out.write((char*) &has_hybrid_phi, 1);
                if (has_hybrid_phi) {
                    out.write((char*) &r__, sizeof(pos_t));
                    _M_Phi_m1.serialize(out);
                    out.write((char*) &hybrid_phi_threshold, sizeof(uint32_t));
                    out.write((char*) &hybrid_cost_phi, sizeof(double));
                    out.write((char*) &hybrid_cost_rlz_init, sizeof(double));
                    out.write((char*) &hybrid_cost_rlz_phrase, sizeof(double));
                    out.write((char*) &hybrid_cost_rlz_decode, sizeof(double));
                    out.write((char*) &hybrid_phi_min_occ, sizeof(uint32_t));
                    out.write((char*) &hybrid_phi_max_pattern, sizeof(uint32_t));
                }

                pos_t num_adaptive_samples = _adaptive_sample_pos.size();
                out.write((char*) &num_adaptive_samples, sizeof(pos_t));
                if (num_adaptive_samples != 0) {
                    write_to_file(out, (char*) _adaptive_sample_pos.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_sa.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_x_p.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_x_lp.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_x_cp.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_x_r.data(), num_adaptive_samples * sizeof(pos_t));
                    write_to_file(out, (char*) _adaptive_sample_s_np.data(), num_adaptive_samples * sizeof(pos_t));
                }
                out.write((char*) &adaptive_sample_min_occ, sizeof(uint32_t));
                out.write((char*) &adaptive_sample_max_occ, sizeof(uint32_t));
                out.write((char*) &adaptive_sample_max_distance, sizeof(uint32_t));
            }

            if constexpr (!supports_bwsearch) {
                out.write((char*) &delta, sizeof(pos_t));
                out.write((char*) &last_sa, sizeof(pos_t));
                _SA_delta.serialize(out);
            }
        } else if constexpr (support == _locate_bi_bwd) {
            _SA_s.serialize(out);
            _SA_s_.serialize(out);
        } else if constexpr (has_lzendsa) {
            _SA_s.serialize(out);
            _SA_s_.serialize(out);
            out.write((char*) &z_end, sizeof(pos_t));
            _lzendsa.serialize(out);
        }

        std::streamoff offs_end = out.tellp() - pos_data_structure_offsets;
        out.seekp(pos_data_structure_offsets, std::ios::beg);
        out.write((char*) &offs_end, sizeof(std::streamoff));
        out.seekp(pos_data_structure_offsets + offs_end, std::ios::beg);
    }

    /**
     * @brief reads a serialized index from an input stream
     * @param in an input stream storing a serialized index
     */
    void load(std::istream& in)
    {
        bool is_64_bit;
        in.read((char*) &is_64_bit, 1);

        if (is_64_bit != std::is_same_v<pos_t, uint64_t>) {
            std::cout << "error: cannot load a" << (is_64_bit ? "64" : "32") << "-bit"
                      << " index into a " << (is_64_bit ? "32" : "64") << "-bit index-object" << std::flush;
            return;
        }

        move_r_support _support;
        in.read((char*) &_support, sizeof(move_r_support));

        std::streampos pos_data_structure_offsets = in.tellg();
        std::streamoff offs_end;
        in.read((char*) &offs_end, sizeof(std::streamoff));

        in.read((char*) &n, sizeof(pos_t));
        in.read((char*) &sigma, sizeof(uint32_t));
        in.read((char*) &r, sizeof(pos_t));
        in.read((char*) &r_, sizeof(pos_t));
        in.read((char*) &a, sizeof(uint16_t));
        in.read((char*) &p_r, sizeof(uint16_t));
        in.read((char*) &_l_blk_size, sizeof(pos_t));

        if (p_r > 1) {
            _D_e.resize(p_r - 1);
            in.read((char*) &_D_e[0], (p_r - 1) * 2 * sizeof(pos_t));
        }

        in.read((char*) &symbols_remapped, 1);
        if (symbols_remapped) {
            if constexpr (byte_alphabet) {
                _map_int.resize(256);
                in.read((char*) &_map_int[0], 256);

                _map_ext.resize(256);
                in.read((char*) &_map_ext[0], 256);
            } else {
                no_init_resize(_map_ext, sigma);
                read_from_file(in, (char*) &_map_ext[0], sizeof(sym_t) * sigma);

                std::vector<std::pair<sym_t, i_sym_t>> map_int_vec;
                no_init_resize(map_int_vec, sigma);
                read_from_file(in, (char*) &map_int_vec[0], sizeof(std::pair<sym_t, i_sym_t>) * sigma);
                uint64_t alloc_before = malloc_count_current();
                _map_int.insert(map_int_vec.begin(), map_int_vec.end());
                size_map_int = malloc_count_current() - alloc_before;
            }
        }

        _M_LF.load(in);

        if constexpr (byte_alphabet) {
            _L_prev.load(in);
            _L_next.load(in);
        } else {
            _RS_L_.load(in);
        }

        if constexpr (support == _locate_one) {
            _SA_s.load(in);
        } else if constexpr (has_locate_move)
        {
            if constexpr (support == _locate_move_bi_fwd) {
                in.read((char*) &r___, sizeof(pos_t));
                _M_Phi.load(in);
            }

            in.read((char*) &r__, sizeof(pos_t));
            _M_Phi_m1.load(in);

            if constexpr (support == _locate_move) {
                in.read((char*) &omega_idx, 1);
                _SA_Phi_m1.load(in);
            } else {
                _SA_s.load(in);
                _SA_s_.load(in);
            }
        } else if constexpr (has_rlzsa)
        {
            in.read((char*) &z, sizeof(pos_t));
            in.read((char*) &z_l, sizeof(pos_t));
            in.read((char*) &z_c, sizeof(pos_t));

            _SA_s.load(in);
            _SA_s_.load(in);
            _R.load(in);
            in.read((char*) &partial_rlzsa, 1);
            if (partial_rlzsa) {
                in.read((char*) &partial_rlzsa_block_size, sizeof(pos_t));
                pos_t partial_offsets = 0;
                pos_t partial_phrases = 0;
                pos_t partial_copies = 0;
                pos_t partial_literals = 0;
                pos_t partial_blocks = 0;
                if (partial_rlzsa_block_size == 0) {
                    uint8_t partial_format_version = 0;
                    in.read((char*) &partial_format_version, 1);
                    in.read((char*) &partial_rlzsa_codec, 1);
                    in.read((char*) &partial_rlzsa_block_size, sizeof(pos_t));
                    in.read((char*) &partial_rlzsa_gap, sizeof(pos_t));
                    in.read((char*) &partial_offsets, sizeof(pos_t));
                    in.read((char*) &partial_phrases, sizeof(pos_t));
                    in.read((char*) &partial_copies, sizeof(pos_t));
                    in.read((char*) &partial_literals, sizeof(pos_t));
                    in.read((char*) &partial_blocks, sizeof(pos_t));
                    uint64_t offsets_payload_size = 0;
                    uint64_t copy_offsets_payload_size = 0;
                    uint64_t literal_offsets_payload_size = 0;
                    uint64_t block_id_payload_size = 0;
                    uint64_t pt_payload_size = 0;
                    uint64_t cpl_payload_size = 0;
                    uint64_t sr_payload_size = 0;
                    if (partial_format_version >= 2 || partial_rlzsa_codec >= 2) {
                        in.read((char*) &offsets_payload_size, sizeof(uint64_t));
                        in.read((char*) &copy_offsets_payload_size, sizeof(uint64_t));
                        in.read((char*) &literal_offsets_payload_size, sizeof(uint64_t));
                        in.read((char*) &block_id_payload_size, sizeof(uint64_t));
                        in.read((char*) &pt_payload_size, sizeof(uint64_t));
                        in.read((char*) &cpl_payload_size, sizeof(uint64_t));
                        in.read((char*) &sr_payload_size, sizeof(uint64_t));
                    } else {
                        in.read((char*) &block_id_payload_size, sizeof(uint64_t));
                        in.read((char*) &sr_payload_size, sizeof(uint64_t));
                    }

                    no_init_resize(_partial_rlzsa_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_copy_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_literal_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_block_ids, partial_blocks);
                    no_init_resize(_partial_rlzsa_pt, partial_phrases);
                    no_init_resize(_partial_rlzsa_cpl, partial_copies);
                    no_init_resize(_partial_rlzsa_sr, partial_copies);
                    no_init_resize(_partial_rlzsa_lp, partial_literals);
                    if (partial_rlzsa_codec >= 2) {
                        if (partial_offsets) partial_decode_delta_varbyte(in, _partial_rlzsa_offsets);
                        if (partial_offsets) partial_decode_delta_varbyte(in, _partial_rlzsa_copy_offsets);
                        if (partial_offsets) partial_decode_delta_varbyte(in, _partial_rlzsa_literal_offsets);
                    } else {
                        if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_offsets.data(), partial_offsets * sizeof(pos_t));
                        if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_copy_offsets.data(), partial_offsets * sizeof(pos_t));
                        if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_literal_offsets.data(), partial_offsets * sizeof(pos_t));
                    }
                    if (partial_rlzsa_codec >= 1) {
                        if (partial_blocks) partial_decode_delta_varbyte(in, _partial_rlzsa_block_ids);
                    } else if (partial_blocks) {
                        read_from_file(in, (char*) _partial_rlzsa_block_ids.data(), partial_blocks * sizeof(pos_t));
                    } else if (block_id_payload_size != 0) {
                        in.seekg(block_id_payload_size, std::ios::cur);
                    }
                    if (partial_rlzsa_codec >= 2) {
                        if (partial_phrases) partial_decode_bits(in, _partial_rlzsa_pt);
                    } else if (partial_phrases) {
                        read_from_file(in, (char*) _partial_rlzsa_pt.data(), partial_phrases * sizeof(uint8_t));
                    } else if (pt_payload_size != 0) {
                        in.seekg(pt_payload_size, std::ios::cur);
                    }
                    if (partial_copies) {
                        if (partial_rlzsa_codec >= 2) {
                            partial_decode_value_varbyte(in, _partial_rlzsa_cpl);
                        } else {
                            read_from_file(in, (char*) _partial_rlzsa_cpl.data(), partial_copies * sizeof(uint16_t));
                        }
                        if (partial_rlzsa_codec >= 1) {
                            partial_decode_zigzag_delta_varbyte(in, _partial_rlzsa_sr);
                        } else {
                            read_from_file(in, (char*) _partial_rlzsa_sr.data(), partial_copies * sizeof(pos_t));
                        }
                    } else if (cpl_payload_size != 0) {
                        in.seekg(cpl_payload_size, std::ios::cur);
                    } else if (sr_payload_size != 0) {
                        in.seekg(sr_payload_size, std::ios::cur);
                    }
                    if (partial_literals) read_from_file(in, (char*) _partial_rlzsa_lp.data(), partial_literals * sizeof(pos_t));
                } else {
                    partial_rlzsa_codec = 0;
                    in.read((char*) &partial_rlzsa_gap, sizeof(pos_t));
                    in.read((char*) &partial_offsets, sizeof(pos_t));
                    in.read((char*) &partial_phrases, sizeof(pos_t));
                    in.read((char*) &partial_copies, sizeof(pos_t));
                    in.read((char*) &partial_literals, sizeof(pos_t));
                    in.read((char*) &partial_blocks, sizeof(pos_t));
                    no_init_resize(_partial_rlzsa_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_copy_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_literal_offsets, partial_offsets);
                    no_init_resize(_partial_rlzsa_block_ids, partial_blocks);
                    no_init_resize(_partial_rlzsa_pt, partial_phrases);
                    no_init_resize(_partial_rlzsa_cpl, partial_copies);
                    no_init_resize(_partial_rlzsa_sr, partial_copies);
                    no_init_resize(_partial_rlzsa_lp, partial_literals);
                    if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_copy_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_offsets) read_from_file(in, (char*) _partial_rlzsa_literal_offsets.data(), partial_offsets * sizeof(pos_t));
                    if (partial_blocks) read_from_file(in, (char*) _partial_rlzsa_block_ids.data(), partial_blocks * sizeof(pos_t));
                    if (partial_phrases) read_from_file(in, (char*) _partial_rlzsa_pt.data(), partial_phrases * sizeof(uint8_t));
                    if (partial_copies) {
                        read_from_file(in, (char*) _partial_rlzsa_cpl.data(), partial_copies * sizeof(uint16_t));
                        read_from_file(in, (char*) _partial_rlzsa_sr.data(), partial_copies * sizeof(pos_t));
                    }
                    if (partial_literals) read_from_file(in, (char*) _partial_rlzsa_lp.data(), partial_literals * sizeof(pos_t));
                }
            } else {
                _SCP_S.load(in);
                no_init_resize(_CPL, z_c + 2);
                read_from_file(in, (char*) &_CPL[0], (z_c + 2) * sizeof(uint16_t));
                _SR.load(in);
                _LP.load(in);
                _PT.load(in);
            }

            if constexpr (supports_bwsearch) {
                if (in.tellg() < pos_data_structure_offsets + offs_end) {
                    bool has_hybrid_phi = false;
                    in.read((char*) &has_hybrid_phi, 1);
                    hybrid_locate = has_hybrid_phi;
                    if (has_hybrid_phi) {
                        in.read((char*) &r__, sizeof(pos_t));
                        _M_Phi_m1.load(in);
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_phi_threshold, sizeof(uint32_t));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_cost_phi, sizeof(double));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_cost_rlz_init, sizeof(double));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_cost_rlz_phrase, sizeof(double));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_cost_rlz_decode, sizeof(double));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_phi_min_occ, sizeof(uint32_t));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &hybrid_phi_max_pattern, sizeof(uint32_t));
                        }
                    }

                    if (in.tellg() < pos_data_structure_offsets + offs_end) {
                        pos_t num_adaptive_samples = 0;
                        in.read((char*) &num_adaptive_samples, sizeof(pos_t));
                        if (num_adaptive_samples != 0) {
                            no_init_resize(_adaptive_sample_pos, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_sa, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_x_p, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_x_lp, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_x_cp, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_x_r, num_adaptive_samples);
                            no_init_resize(_adaptive_sample_s_np, num_adaptive_samples);
                            read_from_file(in, (char*) _adaptive_sample_pos.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_sa.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_x_p.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_x_lp.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_x_cp.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_x_r.data(), num_adaptive_samples * sizeof(pos_t));
                            read_from_file(in, (char*) _adaptive_sample_s_np.data(), num_adaptive_samples * sizeof(pos_t));
                            rebuild_adaptive_sample_lookup();
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &adaptive_sample_min_occ, sizeof(uint32_t));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &adaptive_sample_max_occ, sizeof(uint32_t));
                        }
                        if (in.tellg() < pos_data_structure_offsets + offs_end) {
                            in.read((char*) &adaptive_sample_max_distance, sizeof(uint32_t));
                        }
                    }
                }
            }
            
            if constexpr (!supports_bwsearch) {
                in.read((char*) &delta, sizeof(pos_t));
                in.read((char*) &last_sa, sizeof(pos_t));
                _SA_delta.load(in);
            }
        } else if constexpr (support == _locate_bi_bwd) {
            _SA_s.load(in);
            _SA_s_.load(in);
        } else if constexpr (has_lzendsa) {
            _SA_s.load(in);
            _SA_s_.load(in);
            in.read((char*) &z_end, sizeof(pos_t));
            _lzendsa.load(in);
        }

        in.seekg(pos_data_structure_offsets + offs_end, std::ios::beg);
    }

    std::ostream& operator>>(std::ostream& os) const
    {
        serialize(os);
        return os;
    }

    std::istream& operator<<(std::istream& is)
    {
        load(is);
        return is;
    }
};

#include "construction/construction.hpp"
#include "queries.cpp"
