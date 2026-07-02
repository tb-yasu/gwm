/*
 * Graph.cpp
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

#include <cstdlib>

using namespace std;

namespace GRAPHKERNEL {

namespace {

inline bool isBlank(char c) {
  return c == ' ' || c == '\t' || c == '\r';
}

inline bool parseU64(const char *&p, uint64_t &out) {
  char *end;
  const uint64_t v = strtoull(p, &end, 10);
  if (end == p)
    return false;
  out = v;
  p = end;
  return true;
}

}  // namespace

// Parses gSpan format ("t # ..." / "v id label" / "e from to elabel").  A
// blank line or the "t" line of the next record ends the graph; the latter is
// left in the caller's lookahead so no stream seeking is needed (tellg per
// line forces an lseek and a buffer flush in libc++, which used to dominate
// database loading time).
int Graph::read(ifstream &ifs, string &line, bool &haveLine) {
  clear();
  while (haveLine) {
    if (line.empty()) {  // record separator
      haveLine = static_cast<bool>(getline(ifs, line));
      break;
    }
    const char *p = line.c_str();
    while (isBlank(*p))
      ++p;
    const char tok = *p;
    if (tok != '\0' && (p[1] == '\0' || isBlank(p[1]))) {
      const char *q = p + 1;
      if (tok == 't') {
        if (!empty())
          break;  // next record's header: keep it in the lookahead
      } else if (tok == 'v') {
        uint64_t id, vlabel;
        if (parseU64(q, id) && parseU64(q, vlabel)) {
          if (size() <= id)
            resize(id + 1);
          (*this)[id].vlabel = vlabel;
        }
      } else if (tok == 'e') {
        uint64_t from, to, elabel;
        if (parseU64(q, from) && parseU64(q, to) && parseU64(q, elabel)) {
          if (size() <= from || size() <= to) {
            cerr << "Format Error: define lists before edges" << endl;
            return 1;
          }
          (*this)[from].push(from, to, elabel);
          (*this)[to].push(to, from, elabel);
        }
      }
    }
    haveLine = static_cast<bool>(getline(ifs, line));
  }
  return 0;
}

}  // namespace GRAPHKERNEL
