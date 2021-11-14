TARGET = dedupe

IDIR = include
SDIR = src
BDIR = build
DBG_BDIR = debug

SRCS = $(wildcard $(SDIR)/*.cpp)
HDRS = $(wildcard $(IDIR)/*.hpp)
OBJS = $(SRCS:$(SDIR)/%.cpp=$(BDIR)/%.o)
DBG_OBJS = $(SRCS:$(SDIR)/%.cpp=$(DBG_BDIR)/%.o)

CXX = clang++
CXXFLAGS = --std=c++20 -I $(IDIR) -Wall -Wextra -Wpedantic -O3
DBGFLAGS = -g -O1 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS = -lcrypto -pthread

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

debug: $(TARGET)_dbg
	
$(TARGET)_dbg: $(DBG_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DBGFLAGS) $(DBG_OBJS) -o $(TARGET)

$(BDIR)/%.o: $(SDIR)/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(DBG_BDIR)/%.o: $(SDIR)/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(TARGET)_dbg $(BDIR)/*.o $(DBG_BDIR)/*.o
