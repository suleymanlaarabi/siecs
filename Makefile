NAME        := bin/app
TEST_NAME   := bin/tests
BENCH_NAME  := bin/bench

CC          := gcc

CSTD        := -std=c23
WARNINGS    := -Wall -Wextra -Wpedantic
DEBUG       := -g3
DEPFLAGS    := -MMD -MP
BENCH_OPT   := -O2

CFLAGS      := $(CSTD) $(WARNINGS) $(OPTIM) $(DEBUG) $(DEPFLAGS)
CPPFLAGS    := -I.
LDFLAGS     :=
LDLIBS      :=

TEST_LDLIBS := -lcriterion

OBJ_DIR     := .build/obj
BIN_DIR     := bin


# Source and Test discovery
# We use find and then strip the ./ prefix for consistency
ALL_C_FILES := $(shell find . -type f -name '*.c' -not -path "./.build/*")
ALL_C_FILES := $(ALL_C_FILES:./%=%)

SRC       := $(filter-out tests/% bench/%, $(ALL_C_FILES))
TEST_SRC  := $(filter tests/%, $(ALL_C_FILES))
BENCH_SRC := $(filter bench/%, $(ALL_C_FILES))
LIB_SRC   := $(filter-out main.c, $(SRC))

OBJ       := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
LIB_OBJ   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIB_SRC))
TEST_OBJ  := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRC))
BENCH_OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(BENCH_SRC))

DEPS := $(OBJ:.o=.d) $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d)

.PHONY: all build run test bench clean fclean re

all: build

build: $(NAME)

.PHONY: run
run: $(NAME)
	./$(NAME)

.PHONY: test
test: $(TEST_NAME)
	./$(TEST_NAME)

.PHONY: bench
bench: $(BENCH_NAME)
	./$(BENCH_NAME)

$(NAME): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_NAME): $(LIB_OBJ) $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(LIB_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS) $(TEST_LDLIBS)

$(BENCH_NAME): $(LIB_OBJ) $(BENCH_OBJ) | $(BIN_DIR)
	$(CC) $(LIB_OBJ) $(BENCH_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

# Bench objects compile with optimisations enabled
$(OBJ_DIR)/bench/%.o: bench/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BENCH_OPT) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR)

.PHONY: fclean
fclean: clean
	rm -rf $(BIN_DIR)

.PHONY: re
re: fclean build

-include $(DEPS)
