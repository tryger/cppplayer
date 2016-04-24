CXX=g++
CXXFLAGS=-std=c++11 -Wall -Wextra -pedantic 

LIBS=-pthread -lsfml-system -lsfml-audio -lmpg123 -lboost_system -lboost_filesystem -lboost_program_options -ltag

DEPS = manager.h config.h music.h
OBJ = main.o manager.o config.o music.o

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

release: clean
release: CXXFLAGS += -O3 -flto
release: LIBS += -flto
release: player++ 

debug: clean
debug: CXXFLAGS += -ggdb -Og -fsanitize=thread -DDEBUG
debug: LIBS += -fsanitize=thread
debug: player++

player++:	$(OBJ)
	$(CXX) -o dplayer++ $(OBJ) $(LIBS)

clean:
	rm -f dplayer++ *.o

install:
	cp dplayer++ /usr/local/bin/

uninstall:
	rm /usr/local/bin/dplayer++

