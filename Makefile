CC  = gcc
CXX = g++

CFLAGS   = -Wall -Wextra -Iinclude -O2 -g
CXXFLAGS = -Wall -Wextra -Iinclude -O2 -g \
           -I$(LIVE555_DIR)/liveMedia/include \
           -I$(LIVE555_DIR)/BasicUsageEnvironment/include \
           -I$(LIVE555_DIR)/UsageEnvironment/include \
           -I$(LIVE555_DIR)/groupsock/include

# Đường dẫn live555 đã biên dịch
LIVE555_DIR = $(HOME)/Desktop/workspace/software/live

# live555 static libs (thứ tự quan trọng: liveMedia → groupsock → Basic/UsageEnvironment)
LIVE555_LIBS = \
    $(LIVE555_DIR)/liveMedia/libliveMedia.a \
    $(LIVE555_DIR)/groupsock/libgroupsock.a \
    $(LIVE555_DIR)/BasicUsageEnvironment/libBasicUsageEnvironment.a \
    $(LIVE555_DIR)/UsageEnvironment/libUsageEnvironment.a

LDFLAGS = -pthread -lx264 $(LIVE555_LIBS) -lssl -lcrypto

# Directory structure
SRCDIR  = src
INCDIR  = include
OBJDIR  = output
TESTDIR = tests

# Source files
CORE_SRCS    = $(SRCDIR)/core/logger.c $(SRCDIR)/core/mem_pool.c $(SRCDIR)/core/ring_buff.c
HW_SRCS      = $(SRCDIR)/hardware/camera.c
CODEC_SRCS   = $(SRCDIR)/codec/encoder.c
STREAM_CSRCS = $(SRCDIR)/streaming/nal_queue.c
STREAM_CPPSRCS = $(SRCDIR)/streaming/h264_live_source.cpp \
                 $(SRCDIR)/streaming/rtsp_server.cpp
APP_SRC      = $(SRCDIR)/main.c

# Object files mapping
CORE_OBJS      = $(patsubst $(SRCDIR)/core/%.c,        $(OBJDIR)/core/%.o,      $(CORE_SRCS))
HW_OBJS        = $(patsubst $(SRCDIR)/hardware/%.c,    $(OBJDIR)/hardware/%.o,  $(HW_SRCS))
CODEC_OBJS     = $(patsubst $(SRCDIR)/codec/%.c,       $(OBJDIR)/codec/%.o,     $(CODEC_SRCS))
STREAM_COBJS   = $(patsubst $(SRCDIR)/streaming/%.c,   $(OBJDIR)/streaming/%.o, $(STREAM_CSRCS))
STREAM_CPPOBJS = $(patsubst $(SRCDIR)/streaming/%.cpp, $(OBJDIR)/streaming/%.o, $(STREAM_CPPSRCS))
APP_OBJ        = $(patsubst $(SRCDIR)/%.c,             $(OBJDIR)/%.o,           $(APP_SRC))

ALL_OBJS = $(CORE_OBJS) $(HW_OBJS) $(CODEC_OBJS) \
           $(STREAM_COBJS) $(STREAM_CPPOBJS) $(APP_OBJ)

# Target executable
TARGET = $(OBJDIR)/elcs_cam

# Test executables
TEST_MEM_POOL = $(OBJDIR)/test_mem_pool
TEST_RING_BUF = $(OBJDIR)/test_ring_buff
TEST_LOGGER   = $(OBJDIR)/test_logger
TEST_ENCODER  = $(OBJDIR)/test_encoder

# Phony targets
.PHONY: all clean dirs test tests

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJDIR)/core
	@mkdir -p $(OBJDIR)/hardware
	@mkdir -p $(OBJDIR)/codec
	@mkdir -p $(OBJDIR)/streaming

# Link main target — phải dùng g++ khi có C++ objects
$(TARGET): $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile C source files
$(OBJDIR)/core/%.o: $(SRCDIR)/core/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hardware/%.o: $(SRCDIR)/hardware/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/codec/%.o: $(SRCDIR)/codec/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/streaming/%.o: $(SRCDIR)/streaming/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ source files (streaming)
$(OBJDIR)/streaming/%.o: $(SRCDIR)/streaming/%.cpp \
                          $(SRCDIR)/streaming/h264_live_source.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Tests compilation (chỉ link C libs — không cần live555)
tests: dirs $(TEST_MEM_POOL) $(TEST_RING_BUF) $(TEST_LOGGER) $(TEST_ENCODER)

$(TEST_MEM_POOL): $(OBJDIR)/core/mem_pool.o $(TESTDIR)/test_mem_pool.c
	$(CC) $(CFLAGS) $^ -o $@ -pthread

$(TEST_RING_BUF): $(OBJDIR)/core/ring_buff.o $(TESTDIR)/test_ring_buff.c
	$(CC) $(CFLAGS) $^ -o $@ -pthread

$(TEST_LOGGER): $(OBJDIR)/core/logger.o $(TESTDIR)/test_logger.c
	$(CC) $(CFLAGS) $^ -o $@ -pthread

$(TEST_ENCODER): $(OBJDIR)/core/logger.o $(OBJDIR)/codec/encoder.o $(TESTDIR)/test_encoder.c
	$(CC) $(CFLAGS) $^ -o $@ -pthread -lx264

test: tests
	@echo "--- RUNNING UNIT TESTS ---"
	@./$(TEST_MEM_POOL)
	@./$(TEST_RING_BUF)
	@./$(TEST_LOGGER)
	@./$(TEST_ENCODER)
	@echo "All tests completed."

clean:
	rm -rf $(OBJDIR)/*
