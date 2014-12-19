CXX=g++-3.4
CXXFLAGS=`diabloflowgraph_opt32-config --cflags` -g3
LDFLAGS=`diabloflowgraph_opt32-config --libs`

all: diablo.o cellset.o

diablo.o: diablo.hxx diablo.cxx
	$(CXX) $(CXXFLAGS) -c  diablo.cxx

cellset.o: cellset.hxx cellset.cxx
	$(CXX) $(CXXFLAGS) -c cellset.cxx

clean:
	rm -rf *.o
