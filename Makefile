CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -O2 -g
LDFLAGS = -pthread

# Directory structure
SRCDIR = src
INCDIR = include
OBJDIR = output
TESTDIR = tests

# Source files
CORE_SRCS = $(SRCDIR)/core/logger.c $(SRCDIR)/core/mem_pool.c $(SRCDIR)/core/ring_buff.c
HW_SRCS   = $(SRCDIR)/hardware/camera.c
APP_SRC   = $(SRCDIR)/main.c

# Object files mapping
CORE_OBJS = $(patsubst $(SRCDIR)/core/%.c, $(OBJDIR)/core/%.o, $(CORE_SRCS))
HW_OBJS   = $(patsubst $(SRCDIR)/hardware/%.c, $(OBJDIR)/hardware/%.o, $(HW_SRCS))
APP_OBJ   = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(APP_SRC))

# Target executable
TARGET = $(OBJDIR)/elcs_cam

# Test executables
TEST_MEM_POOL = $(OBJDIR)/test_mem_pool
TEST_RING_BUF = $(OBJDIR)/test_ring_buff
TEST_LOGGER   = $(OBJDIR)/test_logger

# Phony targets
.PHONY: all clean dirs test tests

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJDIR)/core
	@mkdir -p $(OBJDIR)/hardware

# Link main target
$(TARGET): $(CORE_OBJS) $(HW_OBJS) $(APP_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile object files
$(OBJDIR)/core/%.o: $(SRCDIR)/core/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/hardware/%.o: $(SRCDIR)/hardware/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Tests compilation
tests: dirs $(TEST_MEM_POOL) $(TEST_RING_BUF) $(TEST_LOGGER)

$(TEST_MEM_POOL): $(OBJDIR)/core/mem_pool.o $(TESTDIR)/test_mem_pool.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_RING_BUF): $(OBJDIR)/core/ring_buff.o $(TESTDIR)/test_ring_buff.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_LOGGER): $(OBJDIR)/core/logger.o $(TESTDIR)/test_logger.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: tests
	@echo "--- RUNNING UNIT TESTS ---"
	@./$(TEST_MEM_POOL)
	@./$(TEST_RING_BUF)
	@./$(TEST_LOGGER)
	@echo "All tests completed."

clean:
	rm -rf $(OBJDIR)/*
