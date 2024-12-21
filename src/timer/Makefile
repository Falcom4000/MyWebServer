
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -fsanitize=address

SRCS = heaptimer.cpp 
OBJS = $(SRCS:.cpp=.o)

TARGET = heaptimer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)