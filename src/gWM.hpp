/*
 * gWM.hpp
 * Copyright (c) 2013 Yasuo Tabei All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GWM_GWM_HPP_
#define GWM_GWM_HPP_

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "WLKernel.hpp"
#include "rank9sel.h"

class gWM {
  using Ranges = std::vector<std::pair<uint64_t, uint64_t>>;

public:
  void constructor(const char *fname, const char *oname, int _iteration);
  void searcher(const char *index, const char *name, float _kernelThreshold);

  // In-process API for library embedding (e.g. the Python bindings): throws
  // std::runtime_error on I/O failure instead of the CLI entry points' cerr
  // + exit(1), and never writes to stdout/stderr.
  void buildIndex(const std::string &fname, int _iteration);
  void buildIndexFromGraphs(std::vector<GRAPHKERNEL::Graph> &&graphs,
                             int _iteration);
  void saveIndex(const std::string &path) const;
  void loadIndex(const std::string &path);
  std::vector<std::vector<uint64_t>>
  encodeQueryFile(const std::string &fname) const;
  std::vector<std::vector<uint64_t>>
  encodeQueryGraphs(std::vector<GRAPHKERNEL::Graph> &&graphs) const;
  std::vector<std::pair<uint32_t, float>>
  searchEncoded(const std::vector<uint64_t> &query, float threshold);

  size_t   numGraphs()   const { return vnums.size(); }
  uint64_t matrixDepth() const { return depth; }
  uint32_t iterations()  const { return iteration; }
  uint64_t indexLength() const {
    return intervalIndex.empty() ? 0 : intervalIndex.back().second + 1;
  }

  size_t getVnumsSize() const {
    return (vnums.size() * 32) / 8;
  }

  uint64_t getByte() {
    uint64_t memory = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
      memory += bits[i].size() * sizeof(uint64_t);
      memory += dic[i]->bit_count() / 8;
    }
    memory += zeros.size() * sizeof(uint64_t);
    memory += vnums.size() * sizeof(uint32_t);
    return memory;
  }

private:
  void save(std::ostream &os) const;
  void load(std::istream &is);
  void buildIndexCore();
  void encodeQueries(GRAPHKERNEL::WLKernel &queryGraphKernel,
                      std::vector<std::vector<uint64_t>> &out) const;
  void inspectDepth(const std::vector<uint32_t> &cids);
  void buildWaveletMatrix(std::vector<uint32_t> &cids);
  void buildInvertedIndex(std::vector<std::vector<uint32_t>> &invertedIndex);
  void buildRankDictionary();
  void buildInitialIntervals(const std::vector<std::vector<uint32_t>> &invertedIndex);
  void converter(const std::vector<std::vector<uint32_t>> &invertedIndex,
                 std::vector<uint32_t> &cids);
  void makeBit(const std::vector<uint32_t> &v, uint64_t d,
               std::vector<uint64_t> &dst);
  void rangeSearch(Ranges &ranges,
                   std::vector<std::pair<uint32_t, float>> &ids);
  void rangeDfs(uint32_t d, uint32_t bit, const Ranges &ranges,
                std::vector<std::pair<uint32_t, float>> &ids);
  void search(const std::vector<uint64_t> &query,
              std::vector<std::pair<uint32_t, float>> &ids);
  bool pruning(const Ranges &ranges) const;
  float calcSimilarity(const Ranges &ranges, uint32_t bit) const;
  void setZeros(const std::vector<uint32_t> &cids);
  void initMatrix(uint64_t width);

  static uint64_t getBit(uint64_t val, uint64_t d) {
    return (val >> d) & 1ULL;
  }

  uint32_t iteration        = 0;
  float    kernelThreshold  = 0.0f;
  float    kernelThreshold2 = 0.0f;
  float    lowerBound       = 0.0f;
  float    upperBound       = 0.0f;
  uint64_t depth            = 0;
  size_t   qSize            = 0;
  std::vector<std::pair<uint64_t, uint64_t>> intervalIndex;
  GRAPHKERNEL::WLKernel                      graphKernel;
  std::vector<uint32_t>                      vnums;
  std::vector<std::vector<uint64_t>>         bits;
  std::vector<std::unique_ptr<rank9sel>>     dic;
  std::vector<uint64_t>                      zeros;

  // Search-time scratch, reused across queries so the DFS allocates nothing
  // in steady state.  One buffer pair per level suffices: along a DFS path
  // only one node per level is live at a time.
  Ranges              queryRanges;
  std::vector<Ranges> scratch0, scratch1;
};

#endif  // GWM_GWM_HPP_
