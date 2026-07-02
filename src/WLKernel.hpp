/*
 * WLKernel.hpp
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

#ifndef GWM_WLKERNEL_HPP_
#define GWM_WLKERNEL_HPP_

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace GRAPHKERNEL {

struct Edge {
  uint32_t from   = 0;
  uint32_t to     = 0;
  uint64_t elabel = 0;
};

struct Vertex {
  uint64_t vlabel     = 0;
  uint64_t transLabel = 0;
  std::vector<Edge> edge;

  void push(uint32_t from, uint32_t to, uint64_t elabel) {
    edge.push_back({from, to, elabel});
  }
};

// Vertex ids in input files start at 1; slot 0 stays unused.
class Graph : public std::vector<Vertex> {
public:
  // Reads one record.  `line`/`haveLine` act as a one-line lookahead owned by
  // the caller: on entry they hold the first unconsumed line, on return the
  // next record's "t" header (or haveLine == false at end of stream).
  int read(std::ifstream &ifs, std::string &line, bool &haveLine);

  void getVertexLabels(std::vector<uint64_t> &vlabels) const {
    vlabels.clear();
    if (size() <= 1)
      return;
    vlabels.reserve(size() - 1);
    for (size_t i = 1; i < size(); i++)
      vlabels.push_back((*this)[i].vlabel);
  }

  void print() const {
    for (size_t i = 1; i < size(); i++)
      std::cout << i << " " << (*this)[i].vlabel << std::endl;
    std::cout << std::endl;
  }
};

class WLKernel {
public:
  uint32_t                                  iteration = 0;
  std::unordered_map<std::string, uint64_t> s2c;
  uint64_t                                  counter = 0;
  std::vector<Graph>                        graphs;

  WLKernel() = default;
  // Shares the label dictionary of an existing kernel so query graphs are
  // encoded with the same codes; graphs themselves are not copied.
  WLKernel(const WLKernel &gk) : s2c(gk.s2c), counter(gk.counter) {}

  int  readfile(const char *fname);
  int  save(std::ostream &os) const;
  int  load(std::istream &is);
  void initialize();
  void initialTransfer();
  void transferLabels();

  size_t size() const { return graphs.size(); }
  size_t getGraphSize() const { return graphs.size(); }
  size_t getValLabels() const { return counter; }

  void getVertexLabels(size_t i, std::vector<uint64_t> &vlabels) const {
    graphs[i].getVertexLabels(vlabels);
  }

  void print() const {
    for (size_t i = 0; i < graphs.size(); i++) {
      std::cout << "id:" << i << std::endl;
      graphs[i].print();
    }
    std::cout << std::endl;
  }
};

}  // namespace GRAPHKERNEL

#endif  // GWM_WLKERNEL_HPP_
