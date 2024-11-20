CXX = g++
CXXFLAGS = -std=c++20 -O2
TARGET = main
SRCS = ../src/ThreadPool/ThreadPool.cpp 
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
