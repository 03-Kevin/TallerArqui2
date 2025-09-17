CXX = g++
CXXFLAGS = -O3 -std=c++17
LDFLAGS = -pthread

# To build OpenMP enabled:
# make OPENMP=1
ifdef OPENMP
  CXXFLAGS += -fopenmp
endif

all: histogram

histogram: src/histogram.cpp
	$(CXX) $(CXXFLAGS) src/histogram.cpp -o histogram $(LDFLAGS)

clean:
	rm -f histogram
