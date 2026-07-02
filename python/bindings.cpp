/*
 * bindings.cpp
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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gWM.hpp"
#include "WLKernel.hpp"

#ifndef GWM_VERSION
#define GWM_VERSION "unknown"
#endif

namespace py = pybind11;
using namespace GRAPHKERNEL;

namespace {

// One in-memory graph as passed from Python: 0-based vertex labels, and
// edges as 0-based (u, v, edge_label) triples.  pybind11/stl.h converts a
// Python (list[int], list[tuple[int, int, int]]) pair into this type while
// the GIL is still held, so every PyIndex method below receives plain C++
// data and never touches a Python object itself.
using PyEdge  = std::tuple<uint64_t, uint64_t, uint64_t>;
using PyGraph = std::pair<std::vector<uint64_t>, std::vector<PyEdge>>;

// Converts to the library's 1-based Graph (slot 0 is unused, matching
// Graph::read), pushing each edge on both endpoints as Graph::read does.
Graph toGraph(const PyGraph &g) {
  const std::vector<uint64_t> &labels = g.first;
  const std::vector<PyEdge> &edges    = g.second;

  Graph graph;
  graph.resize(labels.size() + 1);
  for (size_t i = 0; i < labels.size(); ++i)
    graph[i + 1].vlabel = labels[i];

  for (const PyEdge &e : edges) {
    const uint64_t u0 = std::get<0>(e);
    const uint64_t v0 = std::get<1>(e);
    if (u0 >= labels.size() || v0 >= labels.size())
      throw std::invalid_argument("edge endpoint out of range");
    const uint32_t u      = static_cast<uint32_t>(u0) + 1;
    const uint32_t v      = static_cast<uint32_t>(v0) + 1;
    const uint64_t elabel = std::get<2>(e);
    graph[u].push(u, v, elabel);
    graph[v].push(v, u, elabel);
  }
  return graph;
}

std::vector<Graph> toGraphs(const std::vector<PyGraph> &graphs) {
  std::vector<Graph> out;
  out.reserve(graphs.size());
  for (const PyGraph &g : graphs)
    out.push_back(toGraph(g));
  return out;
}

// Wraps gWM with a mutex so one Index can be safely called from multiple
// Python threads.  Every method below is bound with
// py::call_guard<py::gil_scoped_release>, which releases the GIL *before*
// invoking the method (argument conversion above has already happened with
// the GIL held); the method then locks mu_.  That order matters: locking
// mu_ before releasing the GIL could deadlock against another thread that
// holds mu_ while blocked trying to reacquire the GIL on its way out.
class PyIndex {
public:
  void build(const std::string &fname, int iteration) {
    std::lock_guard<std::mutex> lock(mu_);
    impl_.buildIndex(fname, iteration);
  }

  void buildFromGraphs(std::vector<PyGraph> graphs, int iteration) {
    std::vector<Graph> converted = toGraphs(graphs);
    std::lock_guard<std::mutex> lock(mu_);
    impl_.buildIndexFromGraphs(std::move(converted), iteration);
  }

  void save(const std::string &path) const {
    std::lock_guard<std::mutex> lock(mu_);
    impl_.saveIndex(path);
  }

  void load(const std::string &path) {
    std::lock_guard<std::mutex> lock(mu_);
    impl_.loadIndex(path);
  }

  std::vector<std::vector<uint64_t>> encodeQueryFile(const std::string &fname) const {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.encodeQueryFile(fname);
  }

  std::vector<std::vector<uint64_t>> encodeQueryGraphs(std::vector<PyGraph> graphs) const {
    std::vector<Graph> converted = toGraphs(graphs);
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.encodeQueryGraphs(std::move(converted));
  }

  std::vector<std::pair<uint32_t, float>>
  searchEncoded(const std::vector<uint64_t> &query, float threshold) {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.searchEncoded(query, threshold);
  }

  size_t numGraphs() const {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.numGraphs();
  }

  uint64_t matrixDepth() const {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.matrixDepth();
  }

  uint32_t iterationsDone() const {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.iterations();
  }

  uint64_t indexLength() const {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.indexLength();
  }

  uint64_t memoryBytes() {
    std::lock_guard<std::mutex> lock(mu_);
    return impl_.getByte();
  }

private:
  gWM impl_;
  mutable std::mutex mu_;
};

}  // namespace

PYBIND11_MODULE(_core, m) {
  m.doc() = "gwm native extension: Weisfeiler-Lehman kernel + wavelet-matrix "
            "graph similarity search";
  m.attr("__version__") = GWM_VERSION;

  py::class_<PyIndex>(m, "Index")
      .def(py::init<>())
      .def("build", &PyIndex::build, py::arg("fname"), py::arg("iteration"),
           py::call_guard<py::gil_scoped_release>())
      .def("build_from_graphs", &PyIndex::buildFromGraphs, py::arg("graphs"),
           py::arg("iteration"), py::call_guard<py::gil_scoped_release>())
      .def("save", &PyIndex::save, py::arg("path"),
           py::call_guard<py::gil_scoped_release>())
      .def("load", &PyIndex::load, py::arg("path"),
           py::call_guard<py::gil_scoped_release>())
      .def("encode_query_file", &PyIndex::encodeQueryFile, py::arg("fname"),
           py::call_guard<py::gil_scoped_release>())
      .def("encode_query_graphs", &PyIndex::encodeQueryGraphs, py::arg("graphs"),
           py::call_guard<py::gil_scoped_release>())
      .def("search_encoded", &PyIndex::searchEncoded, py::arg("query"),
           py::arg("threshold"), py::call_guard<py::gil_scoped_release>())
      // def_property_readonly can't take call_guard as an extra (pybind11
      // rejects it with a static_assert); apply it to the getter itself via
      // py::cpp_function instead, as pybind11's error message directs.
      .def_property_readonly(
          "num_graphs",
          py::cpp_function(&PyIndex::numGraphs, py::call_guard<py::gil_scoped_release>()))
      .def_property_readonly(
          "matrix_depth",
          py::cpp_function(&PyIndex::matrixDepth, py::call_guard<py::gil_scoped_release>()))
      .def_property_readonly(
          "iterations",
          py::cpp_function(&PyIndex::iterationsDone, py::call_guard<py::gil_scoped_release>()))
      .def_property_readonly(
          "index_length",
          py::cpp_function(&PyIndex::indexLength, py::call_guard<py::gil_scoped_release>()))
      .def_property_readonly(
          "memory_bytes",
          py::cpp_function(&PyIndex::memoryBytes, py::call_guard<py::gil_scoped_release>()));
}
