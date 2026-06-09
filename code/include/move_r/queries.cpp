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

#include <move_r/move_r.hpp>
#include <algorithms/sparse_sa_bin_search.cpp>

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::setup_phi_m1_move_pair(pos_t& x, pos_t& s, pos_t& s_) const
    requires(has_locate_move || has_rlzsa)
{
    if constexpr (support == _locate_move) {
        // the index of the pair in M_Phi^{-1} creating the output interval with starting position s = SA[M_LF.p[x]]
        pos_t x_s_ = SA_Phi_m1(x);

        // set s_ to the index of the input interval in M_Phi^{-1} containing s
        s_ = M_Phi_m1().idx(x_s_);

        // compute s
        s = M_Phi_m1().p(s_) + M_Phi_m1().offs(x_s_);
    } else {
        s = SA_s(x);
        s_ = bin_search_max_leq<pos_t>(s, 0, r__ - 1, [&](pos_t x){ return M_Phi_m1().p(x); });
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
sym_t move_r<support, sym_t, pos_t>::BWT(pos_t i) const
{
    // find the index of the input interval in M_LF containing i with a binary search.
    return unmap_symbol(L_(bin_search_max_leq<pos_t>(i, 0, r_ - 1, [&](pos_t x) { return M_LF().p(x); })));
}

template <move_r_support support, typename sym_t, typename pos_t>
pos_t move_r<support, sym_t, pos_t>::SA(pos_t i) const
    requires(supports_multiple_locate)
{
    if constexpr (has_lzendsa) {
        pos_t x = bin_search_min_geq<pos_t>(i, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_ + 1) - 1; });

        while (SA_s_(x) == n) {
            x++;
        }

        pos_t j = M_LF().p(x + 1) - 1;
        int64_t s = SA_s_(x);
        _lzendsa.extract_deltas(i + 1, j, [&](int64_t d){s -= d;});

        return s;
    } else if constexpr (has_rlzsa) {
        // index of the input interval in M_LF containing i.
        pos_t x = bin_search_max_leq<pos_t>(i, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_); });

        while (SA_s(x) == n) {
            x--;
        }

        // position in the suffix array of the current suffix s
        pos_t j = M_LF().p(x);

        // the current suffix (s = SA[j])
        pos_t s = SA_s(x);

        if (j == i) {
            return s;
        }

        j++;
        pos_t x_p, x_lp, x_cp, x_r, s_np;

        // initialize the rlzsa context to position j
        init_rlzsa(j, x_p, x_lp, x_cp, x_r, s_np);

        // compute SA[i]
        skip_rlzsa_right(j, i + 1, s, x_p, x_lp, x_cp, x_r, s_np);

        return s;
    } else if constexpr (has_locate_move) {
        // index of the input interval in M_LF containing i.
        pos_t x = bin_search_max_leq<pos_t>(i, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_); });

        if constexpr (support == _locate_move) {
            /* if i is a bwt run end position (i = M_LF.p(x+1)-1) and SA_Phi^{-1}[x+1] != r'', then
                SA[i] = Phi(SA_s[(x+1) mod r'])
                    = Phi(M_Phi^{-1}.q(SA_Phi^{-1}[(x+1) mod r']))
                    =     M_Phi^{-1}.p(SA_Phi^{-1}[(x+1) mod r'])
            */
            if (i == M_LF().p(x + 1) - 1) [[unlikely]] {
                pos_t xp1 = (x + 1) == r_ ? 0 : (x + 1);

                if (SA_Phi_m1(xp1) != r__) [[unlikely]] {
                    return M_Phi_m1().p(SA_Phi_m1(xp1));
                }
            }

            // decrement x until the starting position of the x-th input interval of M_LF is a starting position of a bwt run
            while (SA_Phi_m1(x) == r__) {
                x--;
            }
        }

        // begin iterating at the start of the x-th run, because there is a
        // suffix array sample at the end position of the x-th input interval

        // position in the suffix array of the current suffix s
        pos_t j = M_LF().p(x);

        // index of the input interval in M_Phi^{-1} containing s
        pos_t s_;
        // the current suffix (s = SA[j])
        pos_t s;

        setup_phi_m1_move_pair(x, s, s_);

        // Perform Phi-move queries until s is the suffix at position
        // i; in each iteration, s = SA[j] = \Phi^{i-j}(SA[i]) holds.
        while (j < i) {
            // Set s = \Phi(s)
            M_Phi_m1().move(s, s_);
            j++;
        }

        // Since j = i, now s = SA[i] holds.
        return s;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
bool move_r<support, sym_t, pos_t>::query_context_t::prepend(sym_t sym)
{
    query_context_t ctx_old = *this;

    if (idx->backward_search_step(sym, b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
        l++;
        i = b;
        return true;
    } else {
        *this = ctx_old;
        return false;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
pos_t move_r<support, sym_t, pos_t>::query_context_t::next_occ()
    requires(supports_multiple_locate && !has_lzendsa)
{
    if constexpr (has_rlzsa) {
        if (i == b) [[unlikely]] {
            // compute the suffix array value at b
            s = idx->SA_s(hat_b_ap_y) - (y + 1);
            i++;

            // check if there is more than one occurrence
            if (b < e) {
                idx->init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
            }

            return s;
        } else {
            idx->next_rlzsa(i, s, x_p, x_lp, x_cp, x_r, s_np);
            return s;
        }
    } else {
        if (i == b) [[unlikely]] {
            // compute the suffix array value at b
            idx->init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
            i++;
            return s;
        } else {
            idx->M_Phi_m1().move(s, s_);
            i++;
            return s;
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
pos_t move_r<support, sym_t, pos_t>::query_context_t::one_occ() const
    requires(supports_locate)
{
    return idx->SA_s(hat_b_ap_y) - (y + 1);
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::query_context_t::locate(report_fnc_t report)
    requires(supports_multiple_locate)
{
    if constexpr (has_lzendsa) {
        // compute the suffix array value at b
        int64_t s = idx->SA_s_(hat_e_ap_z) - (z + 1);
        report(s);
        idx->_lzendsa.extract_deltas(b + 1, e, [&](int64_t d){s -= d; report(s);});
        i = e + 1;
    } else if constexpr (has_rlzsa) {

        if (i == b) [[unlikely]] {
            // compute the suffix array value at b
            s = idx->SA_s(hat_b_ap_y) - (y + 1);
            report(s);
            i++;

            // check if there is more than one occurrence
            if (b < e) [[likely]] {
                idx->init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
            }
        }

        // compute the remaining occurrences SA(b,e]
        if (i <= e) [[likely]] {
            idx->report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
        }
    } else if constexpr (has_locate_move) {
        // compute the suffix array value at b
        if (i == b) [[unlikely]] {
            idx->init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
            report(s);
            i++;
        }

        // compute the remaining occurrences SA(b,e]
        while (i <= e) {
            idx->M_Phi_m1().move(s, s_);
            report(s);
            i++;
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
bool move_r<support, sym_t, pos_t>::backward_search_step(
    sym_t sym,
    pos_t& b, pos_t& e,
    pos_t& b_, pos_t& e_,
    pos_t& hat_b_ap_y, int64_t& y,
    pos_t& hat_e_ap_z, int64_t& z) const
{
    // If the characters have been remapped internally, the pattern also has to be remapped.
    i_sym_t i_sym = map_symbol(sym);

    // If sym does not occur in L', then P[i..m] does not occur in T
    if (i_sym == 0) [[unlikely]] {
        return false;
    }

    // Find the lexicographically smallest suffix in the current suffix array interval that is prefixed by P[i]
    if (i_sym != L_(b_)) {
        /* To do so, we can at first find the first (sub-)run with character P[i] after the b_-th (sub-)run, save
        its index in b_ and set b to its start position M_LF.p(b_). */

        if constexpr (byte_alphabet) {
            pos_t blk = div_ceil<pos_t>(b_, L_block_size());
            pos_t max_b_ = std::min<pos_t>(blk * L_block_size(), e_);

            while (b_ <= max_b_ && i_sym != L_(b_)) {
                b_++;
            }

            if (b_ > max_b_ && i_sym != L_(b_)) [[likely]] {
                b_ = L_next(blk, i_sym);
            }
        } else {
            b_ = RS_L_().rank(i_sym, b_);

            if (b_ == RS_L_().frequency(i_sym)) [[unlikely]] {
                return false;
            }

            b_ = RS_L_().select(i_sym, b_ + 1);
        }

        if (b_ > e_) [[unlikely]] {
            return false;
        }

        b = M_LF().p(b_);

        // update y (Case 1).
        y = 0;
        // update \hat{b}'_y.
        hat_b_ap_y = b_;
    } else {
        y++;
    }

    // Find the lexicographically largest suffix in the current suffix array interval that is prefixed by P[i]
    if (i_sym != L_(e_)) {
        /* To do so, we can at first find the (sub-)last run with character P[i] before the e_-th (sub-)run, save
        its index in e_ and set e to its end position M_LF.p(e_+1)-1. */

        if constexpr (byte_alphabet) {
            pos_t blk = e_ / L_block_size();
            pos_t min_e_ = std::max<pos_t>(blk * L_block_size(), b_);

            while (e_ >= min_e_ && i_sym != L_(e_)) {
                e_--;
            }

            if (e_ < min_e_ && i_sym != L_(e_)) [[likely]] {
                e_ = L_prev(blk, i_sym);
            }
        } else {
            e_ = RS_L_().select(i_sym, RS_L_().rank(i_sym, e_));
        }

        e = M_LF().p(e_ + 1) - 1;

        // update z (Case 1).
        z = 0;
        // update \hat{e}'_z.
        hat_e_ap_z = e_;
    } else {
        z++;
    }

    // Else, because each suffix i in the previous suffix array interval starts with P[i+1..m] and the current
    // interval [b,e] contains all suffixes of it, before which there is a P[i] in T, all suffixes in the
    // interval SA[LF(b),LF(e)] start with P[i..m]

    /* If the suffix array interval [LF(b),LF(e)] of P[i..m] is empty, then b > e,
    because LF(i) is monotonic for a fixed L[i], hence it suffices to check, whether
    b <= e holds. */

    // If the suffix array interval is empty, P does not occur in T, so return false.
    if (b > e) [[unlikely]] {
        return false;
    }

    /* Else, set b <- LF(b) and e <- LF(e). The following two optimizations increase query throughput slightly
        if there are only few occurrences */
    if (b_ == e_) {
        if (b == e) {
            /* If \hat{b'}_i == \hat{e'}_i and b'_i = e'_i, then computing
            (e_i,\hat{e}_i) <- M_LF.move(e'_i,\hat{e'}_i) is redundant */
            M_LF().move(b, b_);
            e = b;
            e_ = b_;
        } else {
            /* If \hat{b'}_i == \hat{e'}_i, but b'_i != e'_i, then e_i = b_i + e'_i - b'_i and therefore
            \hat{b'}_i < \hat{e'}_i, hence we can compute \hat{e'}_i by setting e_ <- \hat{b'}_i = b_ and
            incrementing e_ until e < M_LF.p[e_+1] holds; This takes O(a) time because of the a-balancedness property */
            pos_t diff_eb = e - b;
            M_LF().move(b, b_);
            e = b + diff_eb;
            e_ = b_;

            while (e >= M_LF().p(e_ + 1)) {
                e_++;
            }
        }
    } else {
        M_LF().move(b, b_);
        M_LF().move(e, e_);
    }

    return true;
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::init_phi_m1(
    pos_t& b, pos_t& e,
    pos_t& s, pos_t& s_,
    pos_t& hat_b_ap_y, int64_t& y) const
    requires(has_locate_move || has_rlzsa)
{
    setup_phi_m1_move_pair(hat_b_ap_y, s, s_);
    s -= y + 1;

    // If there is more than one occurrence and s < M_Phi^{-1}.p[s_], now an input interval of M_Phi^{-1} before
    // the s_-th one contains s, so we have to decrease s_. To find the correct value for s_, we perform
    // an exponential search to the left over the input interval starting positions of M_Phi^{-1} starting at s_.
    if (b < e && s < M_Phi_m1().p(s_)) {
        s_ = exp_search_max_leq<pos_t, LEFT>(s, 0, s_, [&](pos_t x) { return M_Phi_m1().p(x); });
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
pos_t move_r<support, sym_t, pos_t>::estimate_rlzsa_crossed_phrases(pos_t b, pos_t e) const
    requires(has_rlzsa)
{
    if (b > e) return 0;

    pos_t x_p_b, x_lp_b, x_cp_b, x_r_b, s_np_b;
    pos_t x_p_e, x_lp_e, x_cp_e, x_r_e, s_np_e;
    init_rlzsa(b, x_p_b, x_lp_b, x_cp_b, x_r_b, s_np_b);
    init_rlzsa(e, x_p_e, x_lp_e, x_cp_e, x_r_e, s_np_e);

    return x_p_e >= x_p_b ? (x_p_e - x_p_b + 1) : 1;
}

template <move_r_support support, typename sym_t, typename pos_t>
bool move_r<support, sym_t, pos_t>::prefer_phi_locate(pos_t b, pos_t e, uint64_t pattern_length) const
    requires(has_rlzsa)
{
    if (!hybrid_locate || _M_Phi_m1.empty()) return false;

    pos_t occ = e - b + 1;
    if (occ < hybrid_phi_min_occ) return false;
    if (hybrid_phi_max_pattern != 0 && pattern_length > hybrid_phi_max_pattern) return false;
    if (occ > hybrid_phi_threshold) return false;

    double cost_phi = hybrid_cost_phi * static_cast<double>(occ);

    if (hybrid_cost_phi >= 0.0 &&
        hybrid_cost_rlz_init >= 0.0 &&
        hybrid_cost_rlz_phrase >= 0.0 &&
        hybrid_cost_rlz_decode >= 0.0) {
        double min_cost_rlz =
            hybrid_cost_rlz_init +
            hybrid_cost_rlz_phrase +
            hybrid_cost_rlz_decode * static_cast<double>(occ);

        if (cost_phi <= min_cost_rlz) {
            return true;
        }

        double max_cost_rlz =
            hybrid_cost_rlz_init +
            hybrid_cost_rlz_phrase * static_cast<double>(occ) +
            hybrid_cost_rlz_decode * static_cast<double>(occ);

        if (cost_phi > max_cost_rlz) {
            return false;
        }
    }

    hybrid_phrase_estimate_queries++;
    pos_t crossed_phrases = estimate_rlzsa_crossed_phrases(b, e);
    double cost_rlz =
        hybrid_cost_rlz_init +
        hybrid_cost_rlz_phrase * static_cast<double>(crossed_phrases) +
        hybrid_cost_rlz_decode * static_cast<double>(occ);

    return cost_phi <= cost_rlz;
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::build_partial_rlzsa_from_blocks(
    pos_t block_size,
    pos_t stored_gap,
    const std::vector<pos_t>& selected_blocks)
    requires(has_rlzsa && supports_bwsearch)
{
    partial_rlzsa = true;
    partial_rlzsa_block_size = block_size;
    partial_rlzsa_gap = stored_gap;
    _partial_rlzsa_offsets.clear();
    _partial_rlzsa_copy_offsets.clear();
    _partial_rlzsa_literal_offsets.clear();
    _partial_rlzsa_block_ids.clear();
    _partial_rlzsa_pt.clear();
    _partial_rlzsa_cpl.clear();
    _partial_rlzsa_sr.clear();
    _partial_rlzsa_lp.clear();
    _partial_local_r_interval_starts.clear();
    _partial_local_r_interval_lengths.clear();

    struct reference_interval_t {
        pos_t start;
        pos_t end;
        pos_t local_start;
    };
    std::vector<reference_interval_t> reference_intervals;

    _partial_rlzsa_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_copy_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_literal_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_block_ids.reserve(selected_blocks.size());

    pos_t phrase = 0;
    pos_t phrase_start = 0;
    pos_t cp = 0;
    pos_t lp = 0;

    auto phrase_len = [&](pos_t p, pos_t c) -> pos_t {
        return PT(p) ? 1 : CPL(c);
    };

    auto advance_phrase = [&]() {
        if (phrase >= z) return;
        pos_t len = phrase_len(phrase, cp);
        phrase_start += len;
        if (PT(phrase)) {
            lp++;
        } else {
            cp++;
        }
        phrase++;
    };

    for (pos_t block : selected_blocks) {
        _partial_rlzsa_block_ids.emplace_back(block);
        _partial_rlzsa_offsets.emplace_back(_partial_rlzsa_pt.size());
        _partial_rlzsa_copy_offsets.emplace_back(_partial_rlzsa_cpl.size());
        _partial_rlzsa_literal_offsets.emplace_back(_partial_rlzsa_lp.size());
        pos_t block_start = block * block_size;
        pos_t block_end = std::min<pos_t>(n - 1, block_start + block_size - 1);
        if (block_start >= block_end) continue;

        pos_t delta_l = block_start + 1;
        pos_t delta_r = block_end;

        while (phrase < z) {
            pos_t len = phrase_len(phrase, cp);
            if (phrase_start + len > delta_l) break;
            advance_phrase();
        }

        pos_t p = phrase;
        pos_t p_start = phrase_start;
        pos_t p_cp = cp;
        pos_t p_lp = lp;

        while (p < z && p_start <= delta_r) {
            bool literal = PT(p);
            pos_t len = literal ? 1 : CPL(p_cp);
            pos_t p_end = p_start + len - 1;
            pos_t chunk_l = std::max<pos_t>(p_start, delta_l);
            pos_t chunk_r = std::min<pos_t>(p_end, delta_r);

            if (chunk_l <= chunk_r) {
                if (literal) {
                    _partial_rlzsa_pt.emplace_back(1);
                    _partial_rlzsa_lp.emplace_back(LP(p_lp));
                } else {
                    _partial_rlzsa_pt.emplace_back(0);
                    pos_t chunk_len = chunk_r - chunk_l + 1;
                    pos_t src = SR(p_cp) + (chunk_l - p_start);
                    _partial_rlzsa_cpl.emplace_back(static_cast<uint16_t>(chunk_len));
                    _partial_rlzsa_sr.emplace_back(src);
                    reference_intervals.push_back({ src, src + chunk_len, 0 });
                }
            }

            p_start += len;
            if (literal) p_lp++; else p_cp++;
            p++;
        }
    }

    _partial_rlzsa_offsets.emplace_back(_partial_rlzsa_pt.size());
    _partial_rlzsa_copy_offsets.emplace_back(_partial_rlzsa_cpl.size());
    _partial_rlzsa_literal_offsets.emplace_back(_partial_rlzsa_lp.size());

    std::sort(reference_intervals.begin(), reference_intervals.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.start != rhs.start) return lhs.start < rhs.start;
                  return lhs.end < rhs.end;
              });

    std::vector<reference_interval_t> merged_intervals;
    merged_intervals.reserve(reference_intervals.size());
    for (const auto& interval : reference_intervals) {
        if (merged_intervals.empty() || interval.start > merged_intervals.back().end) {
            merged_intervals.push_back(interval);
        } else if (interval.end > merged_intervals.back().end) {
            merged_intervals.back().end = interval.end;
        }
    }

    pos_t local_reference_size = 0;
    _partial_local_r_interval_starts.clear();
    _partial_local_r_interval_lengths.clear();
    _partial_local_r_interval_starts.reserve(merged_intervals.size());
    _partial_local_r_interval_lengths.reserve(merged_intervals.size());
    for (auto& interval : merged_intervals) {
        _partial_local_r_interval_starts.emplace_back(interval.start);
        _partial_local_r_interval_lengths.emplace_back(interval.end - interval.start);
        interval.local_start = local_reference_size;
        local_reference_size += interval.end - interval.start;
    }

    for (auto& src : _partial_rlzsa_sr) {
        auto it = std::upper_bound(merged_intervals.begin(), merged_intervals.end(), src,
                                   [](pos_t value, const auto& interval) {
                                       return value < interval.start;
                                   });
        --it;
        src = it->local_start + (src - it->start);
    }

    interleaved_byte_aligned_vectors<uint64_t, pos_t> local_R({ byte_width(2 * n + 1) });
    local_R.resize_no_init(local_reference_size);
    for (const auto& interval : merged_intervals) {
        for (pos_t src = interval.start; src < interval.end; src++) {
            local_R.template set_parallel<0, uint64_t>(interval.local_start + (src - interval.start), R(src));
        }
    }
    _R = std::move(local_R);
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::build_partial_rlzsa(pos_t block_size, pos_t enhance_gap)
    requires(has_rlzsa && supports_bwsearch)
{
    if (block_size == 0) block_size = 128;
    if (enhance_gap == 0) enhance_gap = 1;
    if (z == 0) return;

    pos_t num_blocks = (n + block_size - 1) / block_size;
    std::vector<pos_t> selected_blocks;
    selected_blocks.reserve((num_blocks + enhance_gap - 1) / enhance_gap);
    for (pos_t block = 0; block < num_blocks; block += enhance_gap) {
        selected_blocks.emplace_back(block);
    }

    build_partial_rlzsa_from_blocks(block_size, enhance_gap, selected_blocks);
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::build_partial_rlzsa_adaptive(
    pos_t block_size,
    pos_t budget_blocks,
    std::istream& training_patterns,
    pos_t train_occ_threshold)
    requires(has_rlzsa && supports_bwsearch)
{
    if (block_size == 0) block_size = 128;
    if (budget_blocks == 0 || z == 0 || !training_patterns.good()) return;

    pos_t num_blocks = (n + block_size - 1) / block_size;
    budget_blocks = std::min<pos_t>(budget_blocks, num_blocks);

    std::vector<uint64_t> score(num_blocks, 0);
    auto score_patterns = [&](std::istream& in) {
        std::string header;
        std::getline(in, header);
        uint64_t num_patterns = number_of_patterns(header);
        uint64_t pattern_length = patterns_length(header);
        inp_t pattern;
        no_init_resize(pattern, pattern_length);

        for (uint64_t i = 0; i < num_patterns; i++) {
            in.read(pattern.data(), pattern_length);
            if (!in.good()) break;

            pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
            int64_t y, z_;
            init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z_);

            bool found = true;
            for (int64_t j = pattern.size() - 1; j >= 0; j--) {
                if (!backward_search_step(pattern[j], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z_)) {
                    found = false;
                    break;
                }
            }
            if (!found) continue;

            pos_t occ = e - b + 1;
            if (occ < train_occ_threshold) continue;

            pos_t first_block = b / block_size;
            pos_t last_block = e / block_size;
            for (pos_t block = first_block; block <= last_block; block++) {
                pos_t block_l = block * block_size;
                pos_t block_r = std::min<pos_t>(n - 1, block_l + block_size - 1);
                pos_t l = std::max<pos_t>(b, block_l);
                pos_t r = std::min<pos_t>(e, block_r);
                if (l <= r) score[block] += static_cast<uint64_t>(r - l + 1);
            }
        }
    };

    std::string first_line;
    std::getline(training_patterns, first_line);
    if (first_line.find("mixed_patterns_manifest=1") != std::string::npos) {
        std::string path;
        while (std::getline(training_patterns, path)) {
            if (path.empty() || path[0] == '#') continue;
            std::ifstream in(path, std::ios::binary);
            if (in.good()) score_patterns(in);
        }
    } else {
        std::stringstream buffer;
        buffer << first_line << '\n' << training_patterns.rdbuf();
        score_patterns(buffer);
    }

    std::vector<pos_t> selected_blocks(num_blocks);
    std::iota(selected_blocks.begin(), selected_blocks.end(), 0);
    std::sort(selected_blocks.begin(), selected_blocks.end(), [&](pos_t lhs, pos_t rhs) {
        if (score[lhs] != score[rhs]) return score[lhs] > score[rhs];
        return lhs < rhs;
    });
    selected_blocks.resize(budget_blocks);
    std::sort(selected_blocks.begin(), selected_blocks.end());

    partial_rlzsa = true;
    partial_rlzsa_block_size = block_size;
    partial_rlzsa_gap = std::max<pos_t>(1, num_blocks / budget_blocks);
    _partial_rlzsa_offsets.clear();
    _partial_rlzsa_copy_offsets.clear();
    _partial_rlzsa_literal_offsets.clear();
    _partial_rlzsa_block_ids.clear();
    _partial_rlzsa_pt.clear();
    _partial_rlzsa_cpl.clear();
    _partial_rlzsa_sr.clear();
    _partial_rlzsa_lp.clear();
    _partial_local_r_interval_starts.clear();
    _partial_local_r_interval_lengths.clear();

    struct reference_interval_t {
        pos_t start;
        pos_t end;
        pos_t local_start;
    };
    std::vector<reference_interval_t> reference_intervals;

    _partial_rlzsa_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_copy_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_literal_offsets.reserve(selected_blocks.size() + 1);
    _partial_rlzsa_block_ids.reserve(selected_blocks.size());

    pos_t phrase = 0;
    pos_t phrase_start = 0;
    pos_t cp = 0;
    pos_t lp = 0;

    auto phrase_len = [&](pos_t p, pos_t c) -> pos_t {
        return PT(p) ? 1 : CPL(c);
    };

    auto advance_phrase = [&]() {
        if (phrase >= z) return;
        pos_t len = phrase_len(phrase, cp);
        phrase_start += len;
        if (PT(phrase)) {
            lp++;
        } else {
            cp++;
        }
        phrase++;
    };

    for (pos_t block : selected_blocks) {
        _partial_rlzsa_block_ids.emplace_back(block);
        _partial_rlzsa_offsets.emplace_back(_partial_rlzsa_pt.size());
        _partial_rlzsa_copy_offsets.emplace_back(_partial_rlzsa_cpl.size());
        _partial_rlzsa_literal_offsets.emplace_back(_partial_rlzsa_lp.size());
        pos_t block_start = block * block_size;
        pos_t block_end = std::min<pos_t>(n - 1, block_start + block_size - 1);
        if (block_start >= block_end) continue;

        pos_t delta_l = block_start + 1;
        pos_t delta_r = block_end;

        while (phrase < z) {
            pos_t len = phrase_len(phrase, cp);
            if (phrase_start + len > delta_l) break;
            advance_phrase();
        }

        pos_t p = phrase;
        pos_t p_start = phrase_start;
        pos_t p_cp = cp;
        pos_t p_lp = lp;

        while (p < z && p_start <= delta_r) {
            bool literal = PT(p);
            pos_t len = literal ? 1 : CPL(p_cp);
            pos_t p_end = p_start + len - 1;
            pos_t chunk_l = std::max<pos_t>(p_start, delta_l);
            pos_t chunk_r = std::min<pos_t>(p_end, delta_r);

            if (chunk_l <= chunk_r) {
                if (literal) {
                    _partial_rlzsa_pt.emplace_back(1);
                    _partial_rlzsa_lp.emplace_back(LP(p_lp));
                } else {
                    _partial_rlzsa_pt.emplace_back(0);
                    pos_t chunk_len = chunk_r - chunk_l + 1;
                    pos_t src = SR(p_cp) + (chunk_l - p_start);
                    _partial_rlzsa_cpl.emplace_back(static_cast<uint16_t>(chunk_len));
                    _partial_rlzsa_sr.emplace_back(src);
                    reference_intervals.push_back({ src, src + chunk_len, 0 });
                }
            }

            p_start += len;
            if (literal) p_lp++; else p_cp++;
            p++;
        }
    }

    _partial_rlzsa_offsets.emplace_back(_partial_rlzsa_pt.size());
    _partial_rlzsa_copy_offsets.emplace_back(_partial_rlzsa_cpl.size());
    _partial_rlzsa_literal_offsets.emplace_back(_partial_rlzsa_lp.size());

    std::sort(reference_intervals.begin(), reference_intervals.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.start != rhs.start) return lhs.start < rhs.start;
                  return lhs.end < rhs.end;
              });

    std::vector<reference_interval_t> merged_intervals;
    merged_intervals.reserve(reference_intervals.size());
    for (const auto& interval : reference_intervals) {
        if (merged_intervals.empty() || interval.start > merged_intervals.back().end) {
            merged_intervals.push_back(interval);
        } else if (interval.end > merged_intervals.back().end) {
            merged_intervals.back().end = interval.end;
        }
    }

    pos_t local_reference_size = 0;
    _partial_local_r_interval_starts.clear();
    _partial_local_r_interval_lengths.clear();
    _partial_local_r_interval_starts.reserve(merged_intervals.size());
    _partial_local_r_interval_lengths.reserve(merged_intervals.size());
    for (auto& interval : merged_intervals) {
        _partial_local_r_interval_starts.emplace_back(interval.start);
        _partial_local_r_interval_lengths.emplace_back(interval.end - interval.start);
        interval.local_start = local_reference_size;
        local_reference_size += interval.end - interval.start;
    }

    for (auto& src : _partial_rlzsa_sr) {
        auto it = std::upper_bound(merged_intervals.begin(), merged_intervals.end(), src,
                                   [](pos_t value, const auto& interval) {
                                       return value < interval.start;
                                   });
        --it;
        src = it->local_start + (src - it->start);
    }

    interleaved_byte_aligned_vectors<uint64_t, pos_t> local_R({ byte_width(2 * n + 1) });
    local_R.resize_no_init(local_reference_size);
    for (const auto& interval : merged_intervals) {
        for (pos_t src = interval.start; src < interval.end; src++) {
            local_R.template set_parallel<0, uint64_t>(interval.local_start + (src - interval.start), R(src));
        }
    }
    _R = std::move(local_R);
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate_phi_interval(pos_t b, pos_t e, report_fnc_t report) const
    requires(has_rlzsa && supports_bwsearch)
{
    if (b > e) return;
    if (_M_Phi_m1.empty()) {
        locate_rlzsa_interval(b, e, report);
        return;
    }

    pos_t s = SA(b);
    pos_t s_ = bin_search_max_leq<pos_t>(s, 0, r__ - 1, [&](pos_t x) {
        return M_Phi_m1().p(x);
    });

    report(s);
    for (pos_t i = b + 1; i <= e; i++) {
        M_Phi_m1().move(s, s_);
        report(s);
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate_rlzsa_interval(pos_t b, pos_t e, report_fnc_t report) const
    requires(has_rlzsa && supports_bwsearch)
{
    if (b > e) return;

    pos_t s = SA(b);
    report(s);

    if (b < e) {
        pos_t i = b + 1;
        pos_t x_p, x_lp, x_cp, x_r, s_np;
        init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
        report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::init_rlzsa(
    pos_t i,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const
    requires(has_rlzsa)
{
    // index in SCP_S of the last sampled copy phrase starting before or at i
    pos_t x_scps = SCP_S().rank_1(i + 1);

    if (x_scps == 0) [[unlikely]] {
        // i lies before the first copy phrase
        s_np = i + 1;
        x_p = i;
        x_lp = i;
        x_cp = 0;
        x_r = SR(0);
    } else {
        // copy-phrase index of the last copy-phrase starting before or at i
        pos_t x_cp_lcp = (x_scps - 1) * sample_rate_scp;
        // starting position of the last copy-phrase starting before or at i
        pos_t s_lcp = SCP_S().select_1(x_scps);
        // phrase index of the last copy-phrase starting before or at i
        pos_t x_p_lcp = PT().select_0(x_cp_lcp + 1);
        // phrase index of the x_cp_lcp+1-th copy phrase
        pos_t x_p_ncp = PT().select_0(x_cp_lcp + 2);
        // number of literal phrases between the current and the next copy-phrase
        pos_t n_lp = x_p_ncp - x_p_lcp - 1;
        // starting position of the next copy-phrase
        pos_t s_ncp = s_lcp + CPL(x_cp_lcp) + n_lp;

        // find the last copy-phrase starting before or at i
        while (s_ncp <= i) {
            x_cp_lcp++;
            s_lcp = s_ncp;
            x_p_lcp = x_p_ncp;
            x_p_ncp = PT().select_0(x_cp_lcp + 2);
            n_lp = x_p_ncp - x_p_lcp - 1;
            s_ncp += CPL(x_cp_lcp) + n_lp;
        }

        if (i >= s_ncp - n_lp) {
            // there is a literal phrase at position i
            s_np = i + 1;
            x_cp = x_cp_lcp + 1;
            x_r = SR(x_cp);
            x_p = x_p_lcp + 1 + (i - (s_ncp - n_lp));
        } else {
            // i lies within a copy-phrase
            x_cp = x_cp_lcp;
            x_p = x_p_lcp;
            x_r = SR(x_cp) + (i - s_lcp);
            s_np = s_ncp - n_lp;
        }

        x_lp = x_p - x_cp;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::turn_rlzsa_left(
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_p) const
    requires(has_rlzsa)
{
    if (PT(x_p)) {
        s_p--;

        if (x_cp > 0) [[likely]] {
            x_cp--;
            x_r = SR(x_cp) + CPL(x_cp) - 1;
        }
    } else {
        s_p -= CPL(x_cp);
        x_lp--;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::skip_rlzsa_right(
    pos_t& i, pos_t e, pos_t& s,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const
    requires(has_rlzsa)
{
    while (i < e) {
        // decode all copy-phrases before the next literal phrase
        while (i < e && !PT(x_p)) {
            // decode the x_cp-th copy-phrase
            while (i < s_np && i < e) {
                s += R(x_r);
                s -= n;
                i++;
                x_r++;
            }

            if (i < e || i == s_np) [[likely]] {
                x_p++;
                x_cp++;
                x_r = SR(x_cp);
                s_np += PT(x_p) ? 1 : CPL(x_cp);
            }
        }

        // decode all literal phrases before the next copy-phrase
        while (i < e && PT(x_p)) {
            // decode the x_lp-th literal phrase
            s += LP(x_lp);
            s -= n;
            i++;
            x_p++;
            x_lp++;

            if (PT(x_p)) [[likely]] {
                s_np++;
            } else {
                s_np += PT(x_p) ? 1 : CPL(x_cp);
            }
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::prev_rlzsa(
    pos_t& i, pos_t& s,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_cp) const
    requires(has_rlzsa)
{
    if (PT(x_p)) {
        // literal phrase
        s += n;
        s -= LP(x_lp);
        i--;
        x_p--;
        x_lp--;

        if (i > 0) [[likely]] {
            if (PT(x_p)) {
                // the previous phrase is a literal phrase
                s_cp--;
            } else {
                // the previous phrase is a copy-phrase
                s_cp -= CPL(x_cp);
            }
        }
    } else {
        // copy-prhase
        s += n;
        s -= R(x_r);
        i--;
        x_r--;

        // i lies within the previous phrase
        if (s_cp > 0 && i < s_cp) [[unlikely]] {
            x_p--;
            
            if (x_cp > 0) [[likely]] {
                x_cp--;
                x_r = SR(x_cp) + CPL(x_cp) - 1;
            }

            if (PT(x_p)) {
                // the next phrase is a literal phrase
                s_cp--;
            } else {
                // the next phrase is a copy-phrase
                s_cp -= CPL(x_cp);
            }
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::next_rlzsa(
    pos_t& i, pos_t& s,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np) const
    requires(has_rlzsa)
{
    if (PT(x_p)) {
        // literal phrase
        s += LP(x_lp);
        s -= n;
        i++;
        x_p++;
        x_lp++;

        if (i < n) [[likely]] {
            if (PT(x_p)) {
                // the next phrase is a literal phrase
                s_np++;
            } else {
                // the next phrase is a copy-phrase
                s_np += CPL(x_cp);
            }
        }
    } else {
        // copy-prhase
        s += R(x_r);
        s -= n;
        i++;
        x_r++;

        // there is a new phrase starting at i
        if (s_np <= i && i < n) [[unlikely]] {
            x_p++;
            x_cp++;
            x_r = SR(x_cp);

            if (PT(x_p)) {
                // the next phrase is a literal phrase
                s_np++;
            } else {
                // the next phrase is a copy-phrase
                s_np += CPL(x_cp);
            }
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::report_rlzsa_left(
    pos_t& i, pos_t b, pos_t& s,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_cp,
    report_fnc_t report) const
    requires(has_rlzsa)
{
    static constexpr bool report_pos = function_traits<report_fnc_t>::arity > 1;

    while (true) {
        // decode all copy-phrases after the previous literal phrase
        while (!PT(x_p)) {
            // decode the x_cp-th copy-phrase
            while (i >= s_cp) {
                s += n;
                s -= R(x_r);
                i--;
                if constexpr (report_pos) report(i, s); else report(s);
                if (i == b) [[unlikely]] return;
                x_r--;
            }

            if (x_cp > 0) [[likely]] {
                x_cp--;
                x_r = SR(x_cp) + CPL(x_cp) - 1;
            }

            x_p--;
            s_cp -= PT(x_p) ? 1 : CPL(x_cp);
        }

        // decode all literal phrases after the previous copy-phrase
        while (PT(x_p)) {
            // decode the x_lp-th literal phrase
            s += n;
            s -= LP(x_lp);
            i--;
            if constexpr (report_pos) report(i, s); else report(s);
            if (i == b) [[unlikely]] return;
            x_p--;
            x_lp--;
            s_cp--;
        }

        // set s_cp to the starting position of the last (the x_lp-th)
        // literal phrase before the current (the x_cp-th) copy-phrase
        s_cp -= CPL(x_cp) - 1;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::report_rlzsa_right(
    pos_t& i, pos_t e, pos_t& s,
    pos_t& x_p, pos_t& x_lp, pos_t& x_cp, pos_t& x_r, pos_t& s_np,
    report_fnc_t report) const
    requires(has_rlzsa)
{
    static constexpr bool report_pos = function_traits<report_fnc_t>::arity > 1;

    while (true) {
        // decode all copy-phrases before the next literal phrase
        while (!PT(x_p)) {
            // decode the x_cp-th copy-phrase
            while (i < s_np) {
                s += R(x_r);
                s -= n;
                if constexpr (report_pos) report(i, s); else report(s);
                if (i == e) [[unlikely]] return;
                i++;
                x_r++;
            }

            x_p++;
            x_cp++;
            x_r = SR(x_cp);
            s_np += PT(x_p) ? 1 : CPL(x_cp);
        }

        // decode all literal phrases before the next copy-phrase
        while (PT(x_p)) {
            // decode the x_lp-th literal phrase
            s += LP(x_lp);
            s -= n;
            if constexpr (report_pos) report(i, s); else report(s);
            if (i == e) [[unlikely]] return;
            i++;
            x_p++;
            x_lp++;
            s_np++;
        }

        // set s_np to the starting position of the next (the x_lp-th)
        // literal phrase after the current (the x_cp-th) copy-phrase
        s_np += CPL(x_cp) - 1;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
pos_t move_r<support, sym_t, pos_t>::count(const inp_t& P) const
{
    if constexpr (!supports_bwsearch) {
        auto [beg, end] = binary_sa_search_and_extract<pos_t>(*input, P, _SA_delta.size() + 1,
            [&](pos_t i){return i == _SA_delta.size() ? last_sa : _SA_delta[i];},
            [&](pos_t i){return i == _SA_delta.size() ? n - 1 : (i * delta);},
            [&](pos_t b, pos_t e, pos_t sa_b, pos_t sa_e, auto report){
                report(sa_b);
                if (e == b) return;
                b++;
                pos_t x_p, x_lp, x_cp, x_r, s_np;
                init_rlzsa(b, x_p, x_lp, x_cp, x_r, s_np);
                report_rlzsa_right(b, e, sa_b, x_p, x_lp, x_cp, x_r, s_np, report);
            }, [](pos_t){}, false, false);
        
        return end - beg + 1;
    }

    pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
    int64_t y, z;

    init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

    for (int64_t i = P.size() - 1; i >= 0; i--) {
        if (!backward_search_step(P[i], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
            return 0;
        }
    }

    return e - b + 1;
}

template <move_r_support support, typename sym_t, typename pos_t>
void move_r<support, sym_t, pos_t>::build_adaptive_samples(
    std::istream& patterns,
    uint64_t sample_budget,
    bool uniform_sampling,
    uint32_t min_occ,
    uint32_t max_occ,
    uint32_t max_distance)
    requires(has_rlzsa && supports_bwsearch)
{
    _adaptive_sample_pos.clear();
    _adaptive_sample_sa.clear();
    _adaptive_sample_x_p.clear();
    _adaptive_sample_x_lp.clear();
    _adaptive_sample_x_cp.clear();
    _adaptive_sample_x_r.clear();
    _adaptive_sample_s_np.clear();

    adaptive_sample_min_occ = min_occ;
    adaptive_sample_max_occ = max_occ;
    adaptive_sample_max_distance = max_distance;
    if (sample_budget == 0 || !patterns.good()) return;

    struct sample_candidate {
        pos_t pos = 0;
        uint64_t freq = 0;
        uint64_t occ_sum = 0;
        double score = 0.0;
    };

    std::string header;
    std::getline(patterns, header);
    uint64_t num_patterns = number_of_patterns(header);
    uint64_t pattern_length = patterns_length(header);
    inp_t pattern;
    no_init_resize(pattern, pattern_length);

    std::unordered_map<pos_t, sample_candidate> candidates;

    for (uint64_t i = 0; i < num_patterns; i++) {
        patterns.read(pattern.data(), pattern_length);
        if (!patterns.good()) break;

        pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
        int64_t y, z;
        init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

        bool found = true;
        for (int64_t j = pattern.size() - 1; j >= 0; j--) {
            if (!backward_search_step(pattern[j], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
                found = false;
                break;
            }
        }

        if (!found) continue;

        pos_t occ = e - b + 1;
        auto& candidate = candidates[b];
        candidate.pos = b;
        candidate.freq++;
        candidate.occ_sum += occ;
    }

    std::vector<sample_candidate> selected;
    selected.reserve(candidates.size());

    for (auto& [_, candidate] : candidates) {
        pos_t avg_occ = std::max<uint64_t>(1, candidate.occ_sum / candidate.freq);
        pos_t estimate_end = std::min<pos_t>(n - 1, candidate.pos + avg_occ - 1);
        pos_t crossed_phrases = estimate_rlzsa_crossed_phrases(candidate.pos, estimate_end);
        candidate.score =
            static_cast<double>(candidate.freq) *
            (hybrid_cost_rlz_init + hybrid_cost_rlz_phrase * static_cast<double>(crossed_phrases)) +
            hybrid_cost_rlz_decode * static_cast<double>(candidate.occ_sum);
        selected.emplace_back(candidate);
    }

    if (uniform_sampling) {
        std::sort(selected.begin(), selected.end(), [](const sample_candidate& a, const sample_candidate& b) {
            return a.pos < b.pos;
        });

        if (selected.size() > sample_budget) {
            std::vector<sample_candidate> uniform_selected;
            uniform_selected.reserve(sample_budget);

            if (sample_budget == 1) {
                uniform_selected.emplace_back(selected[selected.size() / 2]);
            } else {
                for (uint64_t i = 0; i < sample_budget; i++) {
                    uint64_t idx = (i * (selected.size() - 1)) / (sample_budget - 1);
                    uniform_selected.emplace_back(selected[idx]);
                }
            }

            selected.swap(uniform_selected);
        }
    } else {
        std::sort(selected.begin(), selected.end(), [](const sample_candidate& a, const sample_candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.freq != b.freq) return a.freq > b.freq;
            return a.pos < b.pos;
        });

        if (selected.size() > sample_budget) selected.resize(sample_budget);
    }

    std::sort(selected.begin(), selected.end(), [](const sample_candidate& a, const sample_candidate& b) {
        return a.pos < b.pos;
    });

    _adaptive_sample_pos.reserve(selected.size());
    _adaptive_sample_sa.reserve(selected.size());
    _adaptive_sample_x_p.reserve(selected.size());
    _adaptive_sample_x_lp.reserve(selected.size());
    _adaptive_sample_x_cp.reserve(selected.size());
    _adaptive_sample_x_r.reserve(selected.size());
    _adaptive_sample_s_np.reserve(selected.size());

    for (const auto& sample : selected) {
        pos_t sa = SA(sample.pos);

        pos_t x_p = 0, x_lp = 0, x_cp = 0, x_r = 0, s_np = 0;
        if (sample.pos + 1 < n) {
            init_rlzsa(sample.pos + 1, x_p, x_lp, x_cp, x_r, s_np);
        }

        _adaptive_sample_pos.emplace_back(sample.pos);
        _adaptive_sample_sa.emplace_back(sa);
        _adaptive_sample_x_p.emplace_back(x_p);
        _adaptive_sample_x_lp.emplace_back(x_lp);
        _adaptive_sample_x_cp.emplace_back(x_cp);
        _adaptive_sample_x_r.emplace_back(x_r);
        _adaptive_sample_s_np.emplace_back(s_np);
    }

    rebuild_adaptive_sample_lookup();
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
bool move_r<support, sym_t, pos_t>::locate_rlzsa_with_adaptive_sample(pos_t b, pos_t e, report_fnc_t report) const
    requires(has_rlzsa && supports_bwsearch)
{
    if (_adaptive_sample_pos.empty()) return false;

    adaptive_sample_queries++;
    pos_t occ = e - b + 1;
    if (occ < adaptive_sample_min_occ || occ > adaptive_sample_max_occ) {
        adaptive_sample_skipped_by_occ++;
        return false;
    }

    pos_t x = 0;
    pos_t sample_pos = b;
    if (adaptive_sample_max_distance == 0) {
        auto exact = _adaptive_sample_lookup.find(b);
        if (exact == _adaptive_sample_lookup.end()) {
            adaptive_sample_misses++;
            return false;
        }
        x = exact->second;
    } else {
        auto pred = std::upper_bound(_adaptive_sample_pos.begin(), _adaptive_sample_pos.end(), b);
        if (pred != _adaptive_sample_pos.begin()) {
            --pred;
            sample_pos = *pred;
            pos_t distance = b - sample_pos;
            if (distance <= adaptive_sample_max_distance) {
                x = pred - _adaptive_sample_pos.begin();
            } else {
                adaptive_sample_misses++;
                return false;
            }
        } else {
            adaptive_sample_misses++;
            return false;
        }
    }

    pos_t distance = b - sample_pos;
    pos_t s = _adaptive_sample_sa[x];
    adaptive_sample_hits++;
    if (distance == 0) {
        adaptive_sample_exact_hits++;
    } else {
        adaptive_sample_predecessor_hits++;
        adaptive_sample_distance_sum += distance;
    }
    adaptive_sample_occurrences += occ;

    if (distance == 0) {
        report(s);
        if (b < e) {
            pos_t i = b + 1;
            pos_t x_p = _adaptive_sample_x_p[x];
            pos_t x_lp = _adaptive_sample_x_lp[x];
            pos_t x_cp = _adaptive_sample_x_cp[x];
            pos_t x_r = _adaptive_sample_x_r[x];
            pos_t s_np = _adaptive_sample_s_np[x];
            report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
        }
    } else {
        pos_t i = sample_pos + 1;
        pos_t x_p = _adaptive_sample_x_p[x];
        pos_t x_lp = _adaptive_sample_x_lp[x];
        pos_t x_cp = _adaptive_sample_x_cp[x];
        pos_t x_r = _adaptive_sample_x_r[x];
        pos_t s_np = _adaptive_sample_s_np[x];
        skip_rlzsa_right(i, b, s, x_p, x_lp, x_cp, x_r, s_np);
        report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
    }

    return true;
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate_phi(const inp_t& P, report_fnc_t report) const
    requires(has_rlzsa && supports_bwsearch)
{
    if (_M_Phi_m1.empty()) {
        locate_rlzsa(P, report);
        return;
    }

    pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
    int64_t y, z;

    init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

    for (int64_t i = P.size() - 1; i >= 0; i--) {
        if (!backward_search_step(P[i], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
            return;
        }
    }

    pos_t s, s_;
    init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
    report(s);

    for (pos_t i = b + 1; i <= e; i++) {
        M_Phi_m1().move(s, s_);
        report(s);
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate_rlzsa(const inp_t& P, report_fnc_t report) const
    requires(has_rlzsa)
{
    if constexpr (!supports_bwsearch) {
        auto [beg, end] = binary_sa_search_and_extract<pos_t>(*input, P, _SA_delta.size() + 1,
            [&](pos_t i){return i == _SA_delta.size() ? last_sa : _SA_delta[i];},
            [&](pos_t i){return i == _SA_delta.size() ? n - 1 : (i * delta);},
            [&](pos_t b, pos_t e, pos_t sa_b, pos_t sa_e, auto report){
                report(sa_b);
                if (e == b) return;
                b++;
                pos_t x_p, x_lp, x_cp, x_r, s_np;
                init_rlzsa(b, x_p, x_lp, x_cp, x_r, s_np);
                report_rlzsa_right(b, e, sa_b, x_p, x_lp, x_cp, x_r, s_np, report);
            }, report, false, true);

        return;
    }

    pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
    int64_t y, z;

    init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

    for (int64_t i = P.size() - 1; i >= 0; i--) {
        if (!backward_search_step(P[i], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
            return;
        }
    }

    if (partial_rlzsa) {
        pos_t s, s_;
        init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
        report(s);
        for (pos_t i = b + 1; i <= e; i++) {
            M_Phi_m1().move(s, s_);
            report(s);
        }
        return;
    }

    if (locate_rlzsa_with_adaptive_sample(b, e, report)) return;

    pos_t s = SA_s(hat_b_ap_y) - (y + 1);
    report(s);

    if (b < e) {
        pos_t i = b + 1;
        pos_t x_p, x_lp, x_cp, x_r, s_np;

        init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
        report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate_block_hybrid(
    const inp_t& P,
    pos_t block_size,
    pos_t enhance_gap,
    pos_t occ_threshold,
    pos_t rlz_occ_threshold,
    report_fnc_t report) const
    requires(has_rlzsa && supports_bwsearch)
{
    if (block_size == 0) block_size = 128;
    if (enhance_gap == 0) enhance_gap = 1;

    pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
    int64_t y, z;

    init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

    for (int64_t i = P.size() - 1; i >= 0; i--) {
        if (!backward_search_step(P[i], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
            return;
        }
    }

    block_hybrid_queries++;
    pos_t occ = e - b + 1;

    auto enhanced_id_for_block = [&](pos_t block, pos_t& enhanced_id) -> bool {
        if (partial_rlzsa) {
            auto it = std::lower_bound(_partial_rlzsa_block_ids.begin(), _partial_rlzsa_block_ids.end(), block);
            if (it == _partial_rlzsa_block_ids.end() || *it != block) return false;
            enhanced_id = static_cast<pos_t>(it - _partial_rlzsa_block_ids.begin());
            return true;
        }
        if (block % enhance_gap != 0) return false;
        enhanced_id = block / enhance_gap;
        return true;
    };

    auto locate_with_phi = [&]() {
        pos_t s, s_;
        init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
        report(s);
        for (pos_t i = b + 1; i <= e; i++) {
            M_Phi_m1().move(s, s_);
            report(s);
        }
    };

    if (occ < occ_threshold || _M_Phi_m1.empty()) {
        block_hybrid_low_occ_queries++;
        block_hybrid_phi_blocks++;
        block_hybrid_phi_occurrences += occ;
        locate_with_phi();
        return;
    }

    if (rlz_occ_threshold != 0) {
        pos_t occ_rlz = 0;
        pos_t scan = b;
        while (scan <= e) {
            pos_t block = scan / block_size;
            pos_t block_end = std::min<pos_t>(e, ((block + 1) * block_size) - 1);
            pos_t enhanced_id = 0;
            if (enhanced_id_for_block(block, enhanced_id)) {
                occ_rlz += block_end - scan + 1;
                if (occ_rlz >= rlz_occ_threshold) break;
            }
            if (block_end == std::numeric_limits<pos_t>::max()) break;
            scan = block_end + 1;
        }
        if (occ_rlz < rlz_occ_threshold) {
            block_hybrid_low_occ_queries++;
            block_hybrid_phi_blocks++;
            block_hybrid_phi_occurrences += occ;
            locate_with_phi();
            return;
        }
    }

    pos_t s, s_;
    init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
    pos_t cur = b;
    while (cur <= e) {
        pos_t block = cur / block_size;
        pos_t block_end = std::min<pos_t>(e, ((block + 1) * block_size) - 1);
        pos_t len = block_end - cur + 1;

        pos_t enhanced_id = 0;
        if (enhanced_id_for_block(block, enhanced_id)) {
            block_hybrid_rlzsa_blocks++;
            block_hybrid_rlzsa_occurrences += len;
            report(s);
            if (cur < block_end) {
                if (partial_rlzsa) {
                    pos_t p = _partial_rlzsa_offsets[enhanced_id];
                    pos_t p_end = _partial_rlzsa_offsets[enhanced_id + 1];
                    pos_t cp = _partial_rlzsa_copy_offsets[enhanced_id];
                    pos_t lp = _partial_rlzsa_literal_offsets[enhanced_id];
                    pos_t delta_pos = block * block_size + 1;
                    pos_t target = cur + 1;

                    while (p < p_end && delta_pos <= block_end) {
                        bool literal = _partial_rlzsa_pt[p] != 0;
                        pos_t plen = literal ? 1 : _partial_rlzsa_cpl[cp];
                        pos_t pend = delta_pos + plen - 1;

                        if (pend >= target) {
                            pos_t skip = target > delta_pos ? target - delta_pos : 0;
                            if (literal) {
                                s += _partial_rlzsa_lp[lp];
                                s -= n;
                                report(s);
                            } else {
                                pos_t x_r = _partial_rlzsa_sr[cp] + skip;
                                for (pos_t d = delta_pos + skip; d <= std::min<pos_t>(pend, block_end); d++) {
                                    s += R(x_r++);
                                    s -= n;
                                    report(s);
                                }
                            }
                        }

                        delta_pos += plen;
                        if (literal) lp++; else cp++;
                        p++;
                    }
                } else {
                    pos_t i = cur + 1;
                    pos_t x_p, x_lp, x_cp, x_r, s_np;
                    init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
                    report_rlzsa_right(i, block_end, s, x_p, x_lp, x_cp, x_r, s_np, report);
                }
            }
        } else {
            block_hybrid_phi_blocks++;
            block_hybrid_phi_occurrences += len;
            s_ = bin_search_max_leq<pos_t>(s, 0, r__ - 1, [&](pos_t x) {
                return M_Phi_m1().p(x);
            });
            report(s);
            for (pos_t i = cur + 1; i <= block_end; i++) {
                M_Phi_m1().move(s, s_);
                report(s);
            }
        }

        if (block_end == std::numeric_limits<pos_t>::max()) break;
        if (block_end < e) {
            s_ = bin_search_max_leq<pos_t>(s, 0, r__ - 1, [&](pos_t x) {
                return M_Phi_m1().p(x);
            });
            M_Phi_m1().move(s, s_);
        }
        cur = block_end + 1;
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::locate(const inp_t& P, report_fnc_t report) const
    requires(supports_multiple_locate)
{
    if constexpr (!supports_bwsearch) {
        auto [beg, end] = binary_sa_search_and_extract<pos_t>(*input, P, _SA_delta.size() + 1,
            [&](pos_t i){return i == _SA_delta.size() ? last_sa : _SA_delta[i];},
            [&](pos_t i){return i == _SA_delta.size() ? n - 1 : (i * delta);},
            [&](pos_t b, pos_t e, pos_t sa_b, pos_t sa_e, auto report){
                report(sa_b);
                if (e == b) return;
                b++;
                pos_t x_p, x_lp, x_cp, x_r, s_np;
                init_rlzsa(b, x_p, x_lp, x_cp, x_r, s_np);
                report_rlzsa_right(b, e, sa_b, x_p, x_lp, x_cp, x_r, s_np, report);
            }, report, false, true);

        return;
    }

    pos_t b, e, b_, e_, hat_b_ap_y, hat_e_ap_z;
    int64_t y, z;

    init_backward_search(b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z);

    for (int64_t i = P.size() - 1; i >= 0; i--) {
        if (!backward_search_step(P[i], b, e, b_, e_, hat_b_ap_y, y, hat_e_ap_z, z)) {
            return;
        }
    }

    if constexpr (has_lzendsa) {
        // compute the suffix array value at e
        int64_t s = SA_s_(hat_e_ap_z) - (z + 1);
        report(s);

        // compute the remaining occurrences SA[b,e)
        _lzendsa.extract_deltas(b + 1, e, [&](int64_t d){s -= d; report(s);});
    } else if constexpr (has_rlzsa) {
        if (partial_rlzsa) {
            pos_t s, s_;
            init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
            report(s);
            for (pos_t i = b + 1; i <= e; i++) {
                M_Phi_m1().move(s, s_);
                report(s);
            }
            return;
        }

        pos_t occ = e - b + 1;
        if (has_hybrid_locate()) {
            hybrid_model_queries++;
        }

        if (prefer_phi_locate(b, e, P.size())) {
            hybrid_phi_queries++;
            hybrid_phi_occurrences += occ;
            pos_t s, s_;
            init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
            report(s);

            for (pos_t i = b + 1; i <= e; i++) {
                M_Phi_m1().move(s, s_);
                report(s);
            }
            return;
        }

        hybrid_rlzsa_queries++;
        hybrid_rlzsa_occurrences += occ;
        if constexpr (supports_bwsearch) {
            if (locate_rlzsa_with_adaptive_sample(b, e, report)) return;
        }

        pos_t s = SA_s(hat_b_ap_y) - (y + 1);
        report(s);

        if (b < e) {
            pos_t i = b + 1;
            pos_t x_p, x_lp, x_cp, x_r, s_np;

            init_rlzsa(i, x_p, x_lp, x_cp, x_r, s_np);
            report_rlzsa_right(i, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
        }
    } else if constexpr (has_locate_move) {
        pos_t s, s_;
        init_phi_m1(b, e, s, s_, hat_b_ap_y, y);
        report(s);

        if (b < e) {
            pos_t i = b + 1;

            while (i <= e) {
                M_Phi_m1().move(s, s_);
                report(s);
                i++;
            }
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::revert_range(report_fnc_t report, retrieve_params params) const
    requires(supports_bwsearch)
{
    adjust_retrieve_params(params, n - 2);

    pos_t l = params.l;
    pos_t r = params.r;

    // leftmost section to revert
    uint16_t s_l;
    // rightmost section to revert
    uint16_t s_r;

    if (p_r == 1) {
        s_l = 0;
        s_r = 0;
    } else {
        s_l = bin_search_min_gt<pos_t>(l, 0, p_r - 1, [&](pos_t x) { return _D_e[x].second; });
        s_r = bin_search_min_geq<pos_t>(r, 0, p_r - 1, [&](pos_t x) { return _D_e[x].second; });
    }

    uint16_t p = std::max(
        (uint16_t)1, // use at least one thread
        std::min({
            (uint16_t)(s_r - s_l + 1), // use at most s_r-s_l+1 threads
            (uint16_t)omp_get_max_threads(), // use at most all threads
            params.num_threads // use at most the specified number of threads
        }));
    
    if (r == n - 1) {
        report(r, 0);
    }

    #pragma omp parallel num_threads(p)
    {
        // Index in [0..p-1] of the current thread.
        uint16_t i_p = omp_get_thread_num();

        // leftmost section for thread i_p to revert
        uint16_t sl_ip = s_l + (i_p * (s_r - s_l + 1)) / p;
        // rightmost section for thread i_p to revert
        uint16_t sr_ip = i_p == p - 1 ? s_r : s_l + ((i_p + 1) * (s_r - s_l + 1)) / p - 1;

        // Iteration range start position of thread i_p.
        pos_t j_l = std::max(l, sl_ip == 0 ? 0 : (_D_e[sl_ip - 1].second + 1) % n);
        // Iteration range end position of thread i_p.
        pos_t j_r = sr_ip == p_r - 1 ? n - 2 : _D_e[sr_ip].second;

        // index of the input interval in M_LF containing i.
        pos_t x = sr_ip == p_r - 1 ? 0 : _D_e[sr_ip].first;
        // The position in the bwt of the current character in T.
        pos_t i = sr_ip == p_r - 1 ? 0 : M_LF().p(x);

        // start iterating at the right iteration range end position
        pos_t j = j_r;

        // iterate until j = r
        while (j > r) {
            // Set i <- LF(i) and j <- j-1.
            M_LF().move(i, x);
            j--;
        }

        if (j >= l) {
            // Report T[r] = T[j] = L[i] = L'[x]
            report(j, unmap_symbol(L_(x)));
        }

        // report T[l,r-1] from right to left
        while (j > j_l) {
            // Set i <- LF(i) and j <- j-1.
            M_LF().move(i, x);
            j--;
            // Report T[j] = L[i] = L'[x].
            report(j, unmap_symbol(L_(x)));
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::BWT_range(report_fnc_t report, retrieve_params params) const
{
    adjust_retrieve_params(params, n - 1);

    pos_t l = params.l;
    pos_t r = params.r;

    uint16_t p = std::max(
        (uint16_t)1, // use at least one thread
        std::min({
            (uint16_t)omp_get_max_threads(), // use at most all threads
            (uint16_t)((r - l + 1) / 10), // use at most (r-l+1)/100 threads
            params.num_threads // use at most the specified number of threads
        }));

    #pragma omp parallel num_threads(p)
    {
        // Index in [0..p-1] of the current thread.
        uint16_t i_p = omp_get_thread_num();

        // Iteration range start position of thread i_p.
        pos_t b = l + i_p * ((r - l + 1) / p);
        // Iteration range end position of thread i_p.
        pos_t e = i_p == p - 1 ? r : l + (i_p + 1) * ((r - l + 1) / p) - 1;

        // Current position in the bwt.
        pos_t i = b;

        // index of the input interval in M_LF containing i.
        pos_t x = bin_search_max_leq<pos_t>(i, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_); });

        // start position of the next input interval in M_LF
        pos_t l_xp1;
 
        // iterate until x is the input interval containing e
        while ((l_xp1 = M_LF().p(x + 1)) <= e) {

            // iterate over all positions in the x-th input interval
            while (i < l_xp1) {
                report(i, unmap_symbol(L_(x)));
                i++;
            }

            x++;
        }

        // report the remaining characters
        while (i <= e) {
            report(i, unmap_symbol(L_(x)));
            i++;
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename report_fnc_t>
void move_r<support, sym_t, pos_t>::SA_range(report_fnc_t report, retrieve_params params) const
    requires(supports_multiple_locate)
{
    adjust_retrieve_params(params, n - 1);

    pos_t l = params.l;
    pos_t r = params.r;

    uint16_t p = std::max(
        (uint16_t)1, // use at least one thread
        std::min({
            (uint16_t)omp_get_max_threads(), // use at most all threads
            params.num_threads, // use at most the specified number of threads
            (uint16_t)(((r - l + 1) * (double)r__) / (10.0 * (double)n)) // use at most (r-l+1)*(r/n)*(1/10) threads
        }));

    #pragma omp parallel num_threads(p)
    {
        // Index in [0..p-1] of the current thread.
        uint16_t i_p = omp_get_thread_num();

        // iteration range start position
        pos_t b = l + i_p * ((r - l + 1) / p);
        // iteration range end position
        pos_t e = i_p == p - 1 ? r : l + (i_p + 1) * ((r - l + 1) / p) - 1;

        if constexpr (has_lzendsa) {
            // compute the suffix array value at e
            int64_t s = SA(e);
            pos_t i = e;
            report(e, s);

            // compute the remaining occurrences SA[b,e)
            _lzendsa.extract_deltas(b + 1, e, [&](int64_t d){s -= d; i--; report(i, s);});
        } else if constexpr (has_locate_move) {
            // the input interval of M_LF containing i
            pos_t x = bin_search_max_leq<pos_t>(b, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_); });

            if constexpr (support == _locate_move) {
                // decrement x until the starting position of the x-th input interval of M_LF is a starting position of a bwt run
                while (SA_Phi_m1(x) == r__) {
                    x--;
                }
            }

            // current position in the suffix array, initially the starting position of the x-th interval of M_LF
            pos_t i = M_LF().p(x);

            // index of the input interval in M_Phi^{-1} containing s
            pos_t s_;
            /* the current suffix array value (SA[i]), initially the suffix array sample of the x-th run,
            initially the suffix array value at b */
            pos_t s;

            setup_phi_m1_move_pair(x, s, s_);

            // iterate up to the iteration range starting position
            while (i < b) {
                M_Phi_m1().move(s, s_);
                i++;
            }

            // report SA[b]
            report(i, s);

            // report the SA-values SA[b+1,e] from left to right
            while (i < e) {
                M_Phi_m1().move(s, s_);
                i++;
                report(i, s);
            }
        } else if constexpr (has_rlzsa) {
            // index of the input interval in M_LF containing i.
            pos_t x = bin_search_max_leq<pos_t>(b, 0, r_ - 1, [&](pos_t x_) { return M_LF().p(x_); });

            // position in the suffix array of the current suffix s
            pos_t j = M_LF().p(x);

            // the current suffix (s = SA[j])
            pos_t s = SA_s(x);

            if (j == b) {
                report(b, s);
            }

            if (j < e) [[likely]] {
                j++;
                pos_t x_p, x_lp, x_cp, x_r, s_np;

                // initialize the rlzsa context to position j
                init_rlzsa(j, x_p, x_lp, x_cp, x_r, s_np);

                // advance the rlzsa context to position b
                skip_rlzsa_right(j, b, s, x_p, x_lp, x_cp, x_r, s_np);

                // decode and report SA[b..e]
                report_rlzsa_right(j, e, s, x_p, x_lp, x_cp, x_r, s_np, report);
            }            
        }
    }
}

template <move_r_support support, typename sym_t, typename pos_t>
template <typename output_t, bool output_reversed>
void move_r<support, sym_t, pos_t>::retrieve_range(
    void (move_r<support, sym_t, pos_t>::*retrieve_method)(
        std::function<void(pos_t, output_t)>&&, move_r<support, sym_t, pos_t>::retrieve_params) const,
    std::string file_name, move_r<support, sym_t, pos_t>::retrieve_params params) const
{
    pos_t l = params.l;
    pos_t r = params.r;
    uint16_t num_threads = params.num_threads;
    uint64_t buffer_size_per_thread = std::max<uint64_t>(1024,
        params.max_bytes_alloc != -1 ? params.max_bytes_alloc / num_threads : ((r - l + 1) * sizeof(output_t)) / (num_threads * 500));

    std::filesystem::resize_file(std::filesystem::current_path() / file_name, (r - l + 1) * sizeof(output_t));
    std::vector<sdsl::int_vector_buffer<sizeof(output_t) * 8>> file_bufs;

    for (uint16_t i = 0; i < num_threads; i++) {
        file_bufs.emplace_back(sdsl::int_vector_buffer<sizeof(output_t) * 8>(file_name, std::ios::in, buffer_size_per_thread, sizeof(output_t) * 8, true));
    }
    
    (this->*retrieve_method)([&](pos_t pos, output_t val) {
        file_bufs[omp_get_thread_num()][pos] = *reinterpret_cast<uint64_t*>(&val);
    }, params);
}
