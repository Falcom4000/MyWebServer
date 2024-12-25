CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -fsanitize=address -O2 

SRCS = test.cpp 
OBJS = $(SRCS:.cpp=.o)

TARGET = test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lfmt

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)