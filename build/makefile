CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -fsanitize=address  -lmysqlclient -g

SRCS = ../src/main.cpp ../src/http/httpconn.cpp ../src/http/httprequest.cpp ../src/http/httpresponse.cpp ../src/log/*.cpp ../src/pool/*.cpp ../src/server/epoller.cpp ../src/server/webserver.cpp ../src/timer/*.cpp ../src/buffer/*.cpp 
OBJS = $(SRCS:.cpp=.o)

TARGET = main

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lfmt -lmysqlclient

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)