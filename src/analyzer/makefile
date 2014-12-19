CXXFLAGS=`diabloflowgraph_opt32-config --cflags` -I ../backend -g3 
LDFLAGS=`diabloflowgraph_opt32-config --libs`

analyzer: ../backend/diablo.o loop.o region.o main.o
	$(CXX) $(CXXFLAGS)  $? -o $@ $(LDFLAGS)

../backend/diablo.o: ../backend
	make -C ../backend

main.o: ../backend/diablo.hxx region.hxx main.cxx
	$(CXX) $(CXXFLAGS) -c  main.cxx

region.o: ../backend/diablo.hxx region.hxx region.cxx
	$(CXX) $(CXXFLAGS) -c  region.cxx

loop.o: ../backend/diablo.hxx loop.hxx loop.cxx
	$(CXX)  $(CXXFLAGS) -c loop.cxx



all: analyzer 

clean:
	rm -rf analyzer *.o
