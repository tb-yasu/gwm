CXX      = g++
CXXFLAGS = -std=c++17 -O3 -DNDEBUG -flto -Wall -Wextra -MMD -MP
LDLIBS   = -lm

CORE_SRCS = Graph.cpp WLKernel.cpp gWM.cpp rank9sel.cpp
CORE_OBJS = $(CORE_SRCS:%.cpp=%.o)
ALL_OBJS  = $(CORE_OBJS) Build.o Search.o

all: gwm-build gwm-search

gwm-build: $(CORE_OBJS) Build.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

gwm-search: $(CORE_OBJS) Search.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# rank9sel is vendored third-party code (sux); keep its warnings quiet.
rank9sel.o: CXXFLAGS += -Wno-sign-compare -Wno-unused-private-field

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS = -std=c++17 -g -Wall -Wextra -MMD -MP
debug: all

clean:
	$(RM) gwm-build gwm-search $(ALL_OBJS) $(ALL_OBJS:%.o=%.d)
	$(RM) -r *.dSYM

-include $(ALL_OBJS:%.o=%.d)

.PHONY: all debug clean
