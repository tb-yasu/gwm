/*
 * WLKernel.cpp
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

#include "WLKernel.hpp"

#include <algorithm>
#include <utility>

using namespace std;

namespace GRAPHKERNEL {

int WLKernel::readfile(const char *fname) {
  ifstream ifs(fname);
  if (!ifs) {
    cerr << "cannot open: " << fname << endl;
    return 1;
  }

  int count = 0;
  string line;
  bool haveLine = static_cast<bool>(getline(ifs, line));
  while (haveLine) {
    Graph g;
    if (g.read(ifs, line, haveLine) != 0) {
      cerr << "file read error after graph " << count << endl;
      while (haveLine && !line.empty())  // skip to the next record separator
        haveLine = static_cast<bool>(getline(ifs, line));
      if (haveLine)
        haveLine = static_cast<bool>(getline(ifs, line));
      continue;
    }
    if (!g.empty()) {
      graphs.push_back(std::move(g));
      count++;
    }
  }
  return 0;
}

void WLKernel::initialize() {
  s2c.clear();
  counter   = 0;
  iteration = 0;
}

void WLKernel::initialTransfer() {
  for (auto &g : graphs) {
    for (size_t j = 1; j < g.size(); j++) {
      string key = to_string(iteration) + '_' + to_string(g[j].vlabel);
      const auto res = s2c.try_emplace(std::move(key), counter);
      if (res.second)
        counter++;
      g[j].transLabel = res.first->second;
    }
  }
  for (auto &g : graphs)
    for (auto &v : g)
      v.vlabel = v.transLabel;
}

void WLKernel::transferLabels() {
  iteration++;
  vector<pair<uint64_t, uint64_t>> labels;
  for (auto &g : graphs) {
    for (size_t j = 1; j < g.size(); j++) {
      labels.clear();
      labels.emplace_back(g[j].vlabel, 0);
      for (const Edge &e : g[j].edge)
        labels.emplace_back(g[e.to].vlabel, e.elabel);
      sort(labels.begin() + 1, labels.end());

      string key = to_string(iteration);
      for (const auto &l : labels) {
        key += '_';
        key += to_string(l.first);
        key += '_';
        key += to_string(l.second);
      }
      const auto res = s2c.try_emplace(std::move(key), counter);
      if (res.second)
        counter++;
      g[j].transLabel = res.first->second;
    }
  }
  for (auto &g : graphs)
    for (auto &v : g)
      v.vlabel = v.transLabel;
}

int WLKernel::save(ostream &os) const {
  os.write(reinterpret_cast<const char *>(&counter), sizeof(counter));
  os.write(reinterpret_cast<const char *>(&iteration), sizeof(iteration));

  const uint64_t size = s2c.size();
  os.write(reinterpret_cast<const char *>(&size), sizeof(size));
  for (const auto &kv : s2c) {
    const uint32_t len = kv.first.size();
    const uint64_t val = kv.second;
    os.write(reinterpret_cast<const char *>(&len), sizeof(len));
    os.write(kv.first.data(), len);
    os.write(reinterpret_cast<const char *>(&val), sizeof(val));
  }
  return 0;
}

int WLKernel::load(istream &is) {
  is.read(reinterpret_cast<char *>(&counter), sizeof(counter));
  is.read(reinterpret_cast<char *>(&iteration), sizeof(iteration));

  uint64_t size = 0;
  is.read(reinterpret_cast<char *>(&size), sizeof(size));
  s2c.reserve(size);
  string word;
  for (uint64_t i = 0; i < size; i++) {
    uint32_t len = 0;
    uint64_t val = 0;
    is.read(reinterpret_cast<char *>(&len), sizeof(len));
    word.resize(len);
    is.read(word.data(), len);
    is.read(reinterpret_cast<char *>(&val), sizeof(val));
    s2c[word] = val;
  }
  return 0;
}

}  // namespace GRAPHKERNEL
