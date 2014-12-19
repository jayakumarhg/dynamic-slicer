CXX=g++
CXXFLAGS=`diabloflowgraph_opt32-config --cflags` -I ../backend -g3 
LDFLAGS=`diabloflowgraph_opt32-config --libs`

all: slicer.naive

slicer.naive: ../backend/diablo.o ../backend/cellset.o slicer.naive.o
	$(CXX) $(CXXFLAGS)  $? -o $@ $(LDFLAGS)

../backend/diablo.o: ../backend
	make -C ../backend diablo.o

../backend/cellset.o: ../backend
	make -C ../backend cellset.o

slicer.naive.o: ../backend/diablo.hxx slicer.naive.cxx
	$(CXX) $(CXXFLAGS) -c  slicer.naive.cxx

clean:
	rm -rf slicer.naive *.o
