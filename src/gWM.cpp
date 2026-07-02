/*
 * gWM.cpp
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

#include "gWM.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace GRAPHKERNEL;

void gWM::buildInvertedIndex(vector<vector<uint32_t>> &invertedIndex) {
  graphKernel.initialize();
  const size_t gSize = graphKernel.getGraphSize();
  vnums.assign(gSize, 0);

  vector<uint64_t> labels;
  auto addLabels = [&]() {
    for (size_t id = 0; id < gSize; id++) {
      graphKernel.getVertexLabels(id, labels);
      sort(labels.begin(), labels.end());
      labels.erase(unique(labels.begin(), labels.end()), labels.end());
      vnums[id] += labels.size();
      for (const uint64_t label : labels) {
        if (invertedIndex.size() <= label)
          invertedIndex.resize(label + 1);
        invertedIndex[label].push_back(id);
      }
    }
  };

  graphKernel.initialTransfer();
  addLabels();
  for (uint32_t i = 0; i < iteration; i++) {
    graphKernel.transferLabels();
    addLabels();
  }
}

void gWM::buildInitialIntervals(const vector<vector<uint32_t>> &invertedIndex) {
  intervalIndex.resize(invertedIndex.size());
  uint64_t s = 0;
  for (size_t i = 0; i < invertedIndex.size(); i++) {
    const uint64_t e = s + invertedIndex[i].size() - 1;
    intervalIndex[i] = make_pair(s, e);
    s = e + 1;
  }
}

void gWM::converter(const vector<vector<uint32_t>> &invertedIndex,
                    vector<uint32_t> &cids) {
  size_t total = 0;
  for (const auto &ids : invertedIndex)
    total += ids.size();
  cids.reserve(total);
  for (const auto &ids : invertedIndex)
    cids.insert(cids.end(), ids.begin(), ids.end());
}

void gWM::inspectDepth(const vector<uint32_t> &cids) {
  const uint32_t maxv =
      cids.empty() ? 0 : *max_element(cids.begin(), cids.end());
  depth = 1;
  while (maxv >> depth)
    depth++;
}

inline float gWM::calcSimilarity(const Ranges &ranges, uint32_t bit) const {
  return (float)ranges.size() / (sqrt(qSize) * sqrt(vnums[bit]));
}

inline bool gWM::pruning(const Ranges &ranges) const {
  return (ranges.size() < lowerBound || upperBound < ranges.size());
}

// DFS over the wavelet matrix.  Splitting is done into per-level scratch
// buffers (see gWM.hpp) instead of freshly allocated vectors; the 1-side is
// descended before the 0-side to keep the hit output order of the previous
// stack-based implementation.
void gWM::rangeDfs(uint32_t d, uint32_t bit, const Ranges &ranges,
                   vector<pair<uint32_t, float>> &ids) {
  if (depth == d) {
    const float sim = calcSimilarity(ranges, bit);
    if (sim >= kernelThreshold)
      ids.emplace_back(bit, sim);
    return;
  }
  if (pruning(ranges))
    return;

  rank9sel *rsdic = dic[d].get();
  Ranges &ranges0 = scratch0[d];
  Ranges &ranges1 = scratch1[d];
  ranges0.clear();
  ranges1.clear();
  for (const auto &r : ranges) {
    uint64_t rs = rsdic->rank(r.first);
    uint64_t re = rsdic->rank(r.second + 1);
    if (rs < re)
      ranges1.emplace_back(rs + zeros[d], re - 1 + zeros[d]);
    rs = r.first - rs;
    re = (r.second + 1) - re;
    if (rs < re)
      ranges0.emplace_back(rs, re - 1);
  }
  if ((bit | (1U << (depth - d - 1))) < vnums.size())
    rangeDfs(d + 1, bit | (1U << (depth - d - 1)), ranges1, ids);
  rangeDfs(d + 1, bit, ranges0, ids);
}

void gWM::rangeSearch(Ranges &ranges, vector<pair<uint32_t, float>> &ids) {
  if (scratch0.size() < depth) {
    scratch0.resize(depth);
    scratch1.resize(depth);
  }
  rangeDfs(0, 0, ranges, ids);
}

void gWM::search(const vector<uint64_t> &query,
                 vector<pair<uint32_t, float>> &ids) {
  queryRanges.clear();
  for (const uint64_t q : query) {
    if (q < intervalIndex.size())
      queryRanges.push_back(intervalIndex[q]);
  }

  qSize      = query.size();
  lowerBound = float(qSize) * kernelThreshold2;
  upperBound = float(qSize) / kernelThreshold2;
  rangeSearch(queryRanges, ids);
}

void gWM::buildRankDictionary() {
  dic.clear();
  dic.reserve(depth);
  for (uint64_t i = 0; i < depth; ++i)
    dic.push_back(make_unique<rank9sel>(bits[i].data(), bits[i].size() * 64));
}

void gWM::setZeros(const vector<uint32_t> &cids) {
  zeros.assign(dic.size(), 0);
  for (size_t i = 0; i + 1 < dic.size(); ++i)
    zeros[i] = cids.size() - dic[i]->rank(cids.size());
}

// Runs a query WLKernel (already populated with graphs) through the same
// initialTransfer/transferLabels rounds as the database, collecting each
// query graph's deduplicated label set.  Shared by the CLI's searcher() and
// the in-process encodeQueryFile()/encodeQueryGraphs().
void gWM::encodeQueries(WLKernel &queryGraphKernel,
                        vector<vector<uint64_t>> &out) const {
  const size_t gSize = queryGraphKernel.getGraphSize();
  out.assign(gSize, {});
  vector<uint64_t> labels;

  auto collectLabels = [&]() {
    for (size_t j = 0; j < gSize; j++) {
      queryGraphKernel.getVertexLabels(j, labels);
      out[j].insert(out[j].end(), labels.begin(), labels.end());
    }
  };
  queryGraphKernel.initialTransfer();
  collectLabels();
  for (uint32_t i = 0; i < iteration; i++) {
    queryGraphKernel.transferLabels();
    collectLabels();
  }
  for (auto &q : out) {
    sort(q.begin(), q.end());
    q.erase(unique(q.begin(), q.end()), q.end());
  }
}

void gWM::searcher(const char *index, const char *name,
                   float _kernelThreshold) {
  ifstream ifs(index);
  if (!ifs) {
    cerr << "cannot open:" << index << endl;
    exit(1);
  }
  cerr << "reading index file:" << index << endl;
  load(ifs);
  cerr << "end reading" << endl;
  ifs.close();

  cerr << "start building rank dictionary" << endl;
  buildRankDictionary();
  cerr << "end building rank dictionary" << endl;

  cerr << "start transferring labels" << endl;
  WLKernel queryGraphKernel(graphKernel);
  queryGraphKernel.readfile(name);
  vector<vector<uint64_t>> qGraphs;
  encodeQueries(queryGraphKernel, qGraphs);
  const size_t gSize = qGraphs.size();
  cerr << "end transferring labels" << endl;

  kernelThreshold  = _kernelThreshold;
  kernelThreshold2 = kernelThreshold * kernelThreshold;

  uint64_t resNum = 0;
  vector<double> times;
  times.reserve(gSize);
  vector<pair<uint32_t, float>> ids;
  for (size_t i = 0; i < gSize; i++) {
    ids.clear();
    const double stime = clock();
    search(qGraphs[i], ids);
    const double etime = clock();
    times.push_back(etime - stime);

    fprintf(stdout, "id:%zu ", i);
    for (const auto &hit : ids)
      fprintf(stdout, "%u:%f ", hit.first, hit.second);
    fprintf(stdout, "\n");
    resNum += ids.size();
  }

  double total = 0.0;
  for (const double t : times)
    total += t;
  total /= CLOCKS_PER_SEC;

  const double mean = total / (double)times.size();
  double dev = 0.0;
  for (const double t : times)
    dev += (t / CLOCKS_PER_SEC - mean) * (t / CLOCKS_PER_SEC - mean);
  dev = sqrt(dev / (double)(times.size() - 1));

  fprintf(stdout, "cpu time (sec):%f\n", total);
  fprintf(stdout, "average cpu time (sec):%f\n", mean);
  fprintf(stdout, "dev cpu time:%f\n", dev);
  const double averageNum = (double)resNum / (double)gSize;
  fprintf(stdout, "average # of outputs:%f\n", averageNum);
}

void gWM::load(istream &is) {
  is.read(reinterpret_cast<char *>(&iteration), sizeof(iteration));
  is.read(reinterpret_cast<char *>(&depth), sizeof(depth));
  {
    uint64_t vnumSize = 0;
    is.read(reinterpret_cast<char *>(&vnumSize), sizeof(vnumSize));
    vnums.resize(vnumSize);
    is.read(reinterpret_cast<char *>(vnums.data()), sizeof(uint32_t) * vnumSize);
  }
  {
    uint64_t size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    bits.resize(size);
    for (uint64_t i = 0; i < size; ++i) {
      uint64_t width = 0;
      is.read(reinterpret_cast<char *>(&width), sizeof(width));
      bits[i].resize(width);
      is.read(reinterpret_cast<char *>(bits[i].data()), sizeof(uint64_t) * width);
    }
  }
  {
    uint64_t size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    intervalIndex.resize(size);
    for (uint64_t i = 0; i < size; ++i) {
      uint64_t s, e;
      is.read(reinterpret_cast<char *>(&s), sizeof(s));
      is.read(reinterpret_cast<char *>(&e), sizeof(e));
      intervalIndex[i] = make_pair(s, e);
    }
  }
  {
    uint64_t size = 0;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    zeros.resize(size);
    is.read(reinterpret_cast<char *>(zeros.data()), sizeof(uint64_t) * size);
  }
  graphKernel.load(is);
}

void gWM::save(ostream &os) const {
  os.write(reinterpret_cast<const char *>(&iteration), sizeof(iteration));
  os.write(reinterpret_cast<const char *>(&depth), sizeof(depth));
  {
    const uint64_t vnumSize = vnums.size();
    os.write(reinterpret_cast<const char *>(&vnumSize), sizeof(vnumSize));
    os.write(reinterpret_cast<const char *>(vnums.data()),
             sizeof(uint32_t) * vnumSize);
  }
  {
    const uint64_t size = bits.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    for (uint64_t i = 0; i < size; ++i) {
      const uint64_t width = bits[i].size();
      os.write(reinterpret_cast<const char *>(&width), sizeof(width));
      os.write(reinterpret_cast<const char *>(bits[i].data()),
               sizeof(uint64_t) * width);
    }
  }
  {
    const uint64_t size = intervalIndex.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    for (uint64_t i = 0; i < size; ++i) {
      const uint64_t s = intervalIndex[i].first;
      const uint64_t e = intervalIndex[i].second;
      os.write(reinterpret_cast<const char *>(&s), sizeof(s));
      os.write(reinterpret_cast<const char *>(&e), sizeof(e));
    }
  }
  {
    const uint64_t size = zeros.size();
    os.write(reinterpret_cast<const char *>(&size), sizeof(size));
    os.write(reinterpret_cast<const char *>(zeros.data()),
             sizeof(uint64_t) * size);
  }
  graphKernel.save(os);
}

void gWM::makeBit(const vector<uint32_t> &v, uint64_t d,
                  vector<uint64_t> &dst) {
  for (size_t i = 0; i < v.size(); ++i) {
    if (getBit(v[i], d))
      dst[i / 64] |= (1ULL << (i % 64));
  }
}

void gWM::initMatrix(uint64_t width) {
  bits.assign(depth, vector<uint64_t>(width));
}

void gWM::buildWaveletMatrix(vector<uint32_t> &cids) {
  vector<uint32_t> newCids(cids.size());
  for (uint64_t d = 0; d < depth; ++d) {
    makeBit(cids, depth - d - 1, bits[d]);
    if (d == depth - 1)
      break;
    size_t counter = 0;
    for (const uint32_t c : cids) {
      if (!getBit(c, depth - d - 1))
        newCids[counter++] = c;
    }
    for (const uint32_t c : cids) {
      if (getBit(c, depth - d - 1))
        newCids[counter++] = c;
    }
    cids.swap(newCids);
  }
}

// Same pipeline as constructor(), minus the progress/stat output and the
// save() to disk.  Used by the in-process buildIndex()/buildIndexFromGraphs().
void gWM::buildIndexCore() {
  vector<vector<uint32_t>> invertedIndex;
  buildInvertedIndex(invertedIndex);
  buildInitialIntervals(invertedIndex);

  vector<uint32_t> cids;
  converter(invertedIndex, cids);
  inspectDepth(cids);

  initMatrix(cids.size() / 64 + 1);
  buildWaveletMatrix(cids);
  buildRankDictionary();
  setZeros(cids);
}

void gWM::constructor(const char *fname, const char *oname, int _iteration) {
  iteration = _iteration;

  cerr << "start reading inputfile:" << fname << endl;
  graphKernel.readfile(fname);
  cerr << "end reading inputfile" << endl;

  const double startTime = clock();
  double sTime = clock();
  cerr << "start building inverted index" << endl;
  vector<vector<uint32_t>> invertedIndex;
  buildInvertedIndex(invertedIndex);
  cerr << "end building inverted index" << endl;
  double eTime = clock();

  fprintf(stdout, "cpu time for building inverted index (sec):%f\n",
          (eTime - sTime) / CLOCKS_PER_SEC);

  sTime = clock();
  cerr << "start building initial intervals" << endl;
  buildInitialIntervals(invertedIndex);
  cerr << "end building initial intervals" << endl;

  vector<uint32_t> cids;
  cerr << "start converting inverted index" << endl;
  converter(invertedIndex, cids);
  cerr << "end converting inverted index" << endl;

  cout << "length of id list:" << cids.size() << endl;

  cerr << "start inspecting height of cids" << endl;
  inspectDepth(cids);
  cerr << "end inspecting height of cids" << endl;

  cout << "depth of wavelet matrix:" << depth << endl;

  cerr << "start building wavelet matrix" << endl;
  initMatrix(cids.size() / 64 + 1);
  buildWaveletMatrix(cids);
  cerr << "end building wavelet matrix" << endl;

  cerr << "start building rank dictionaries" << endl;
  buildRankDictionary();
  cerr << "end building rank dictionaries" << endl;

  cerr << "start setting zero arrays" << endl;
  setZeros(cids);
  cerr << "end setting zero arrays" << endl;

  eTime = clock();

  fprintf(stdout, "cpu time for building wavelet matrix (sec):%f\n",
          (eTime - sTime) / CLOCKS_PER_SEC);

  const double endTime = clock();

  fprintf(stdout, "total construction time (sec):%f\n",
          (endTime - startTime) / CLOCKS_PER_SEC);

  ofstream ofs(oname);
  if (!ofs) {
    cerr << "cannot open:" << oname << endl;
    exit(1);
  }
  save(ofs);
  ofs.close();

  cout << "total memory (byte): " << getByte() << endl;
}

void gWM::buildIndex(const string &fname, int _iteration) {
  iteration = _iteration;
  if (graphKernel.readfile(fname.c_str()) != 0)
    throw runtime_error("cannot open: " + fname);
  buildIndexCore();
}

void gWM::buildIndexFromGraphs(vector<Graph> &&graphs, int _iteration) {
  iteration = _iteration;
  graphKernel.graphs = std::move(graphs);
  buildIndexCore();
}

void gWM::saveIndex(const string &path) const {
  ofstream ofs(path);
  if (!ofs)
    throw runtime_error("cannot open: " + path);
  save(ofs);
}

void gWM::loadIndex(const string &path) {
  ifstream ifs(path);
  if (!ifs)
    throw runtime_error("cannot open: " + path);
  graphKernel.initialize();
  load(ifs);
  buildRankDictionary();
}

vector<vector<uint64_t>> gWM::encodeQueryFile(const string &fname) const {
  WLKernel queryGraphKernel(graphKernel);
  if (queryGraphKernel.readfile(fname.c_str()) != 0)
    throw runtime_error("cannot open: " + fname);
  vector<vector<uint64_t>> out;
  encodeQueries(queryGraphKernel, out);
  return out;
}

vector<vector<uint64_t>> gWM::encodeQueryGraphs(vector<Graph> &&graphs) const {
  WLKernel queryGraphKernel(graphKernel);
  queryGraphKernel.graphs = std::move(graphs);
  vector<vector<uint64_t>> out;
  encodeQueries(queryGraphKernel, out);
  return out;
}

vector<pair<uint32_t, float>>
gWM::searchEncoded(const vector<uint64_t> &query, float threshold) {
  kernelThreshold  = threshold;
  kernelThreshold2 = kernelThreshold * kernelThreshold;
  vector<pair<uint32_t, float>> ids;
  search(query, ids);
  return ids;
}
