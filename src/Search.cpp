/*
 * Search.cpp
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

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "gWM.hpp"

namespace {

const char *fname           = nullptr;
const char *indexFile       = nullptr;
float       kernelThreshold = 0.8f;

void version() {
  std::cerr << "gWM version 3.1.2" << std::endl
            << "Written by Yasuo Tabei" << std::endl
            << std::endl;
}

void usage() {
  std::cerr
      << std::endl
      << "Usage: gwm-search [OPTION]... INDEXFILE QUERYFILE" << std::endl
      << std::endl
      << "       where [OPTION]...  is a list of zero or more optional arguments" << std::endl
      << "             INDEXFILE    is the name of an index file" << std::endl
      << "             QUERYFILE    is the name of a query graph file" << std::endl
      << std::endl
      << "Additional arguments (input and output files may be specified):" << std::endl
      << "       -kthreshold [float value]: similarity threshold" << std::endl
      << "       (default: " << kernelThreshold << ")" << std::endl
      << std::endl;
  exit(0);
}

void parse_parameters(int argc, char **argv) {
  if (argc == 1)
    usage();
  int argno;
  for (argno = 1; argno < argc; argno++) {
    if (argv[argno][0] != '-')
      break;
    if (!strcmp(argv[argno], "-version")) {
      version();
    } else if (!strcmp(argv[argno], "-kthreshold")) {
      if (argno == argc - 1) {
        std::cerr << "Must specify kthreshold after -kthreshold" << std::endl;
        exit(1);
      }
      kernelThreshold = atof(argv[++argno]);
    } else {
      usage();
    }
  }
  if (argno + 1 >= argc)
    usage();

  indexFile = argv[argno];
  fname     = argv[argno + 1];
}

}  // namespace

int main(int argc, char **argv) {
  parse_parameters(argc, argv);

  gWM gwm;
  gwm.searcher(indexFile, fname, kernelThreshold);

  return 0;
}
