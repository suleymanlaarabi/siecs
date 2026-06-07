NAME        := bin/app
TEST_NAME   := bin/tests
TEST_UBSAN_NAME := bin/tests-ubsan
BENCH_NAME  := bin/bench

CC          := gcc

CSTD        := -std=c23
WARNINGS    := -Wall -Wextra -Wpedantic
DEBUG       := -g3
DEPFLAGS    := -MMD -MP
BENCH_OPT   := -O2
UBSAN_FLAGS := -fsanitize=undefined -fno-sanitize-recover=undefined

CFLAGS      := $(CSTD) $(WARNINGS) $(OPTIM) $(DEBUG) $(DEPFLAGS)
CPPFLAGS    := -I.
LDFLAGS     :=
LDLIBS      :=

TEST_LDLIBS := -lcriterion

OBJ_DIR     := .build/obj
UBSAN_OBJ_DIR := .build/ubsan
BIN_DIR     := bin
CLANG_TIDY  := clang-tidy
CLANG_TIDY_FLAGS :=
CPPCHECK    := cppcheck
CPPCHECK_FLAGS := --enable=style,performance --std=c23 --inline-suppr --quiet
CPPCHECK_FLAGS += --suppress=nullPointerOutOfMemory
CPPCHECK_FLAGS += --suppress=invalidPrintfArgType_sint
CPPCHECK_FLAGS += --suppress=normalCheckLevelMaxBranches
CPPCHECK_FLAGS += --suppress=returnImplicitInt
CPPCHECK_FLAGS += --suppress=unknownMacro

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

FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) -not -path "./.build/*" -not -path "./docs/*")
LINT_FILES   := $(filter-out docs/%,$(ALL_C_FILES))

DEPS := $(OBJ:.o=.d) $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d)

.PHONY: all build run test bench clean fclean re format format-check lint lint-strict cppcheck tidy

all: build

build: $(NAME)

.PHONY: run
run: $(NAME)
	./$(NAME)

.PHONY: test
test: $(TEST_NAME)
	./$(TEST_NAME)
	$(MAKE) $(TEST_UBSAN_NAME) TEST_NAME=$(TEST_UBSAN_NAME) OBJ_DIR=$(UBSAN_OBJ_DIR) CFLAGS='$(CFLAGS) $(UBSAN_FLAGS)' LDFLAGS='$(LDFLAGS) $(UBSAN_FLAGS)'
	./$(TEST_UBSAN_NAME)

.PHONY: bench
bench: $(BENCH_NAME)
	./$(BENCH_NAME)

format:
	clang-format -i $(FORMAT_FILES)

format-check:
	clang-format --dry-run --Werror $(FORMAT_FILES)

lint:
	$(CLANG_TIDY) $(CLANG_TIDY_FLAGS) -p . $(LINT_FILES)

lint-strict: CLANG_TIDY_FLAGS := --warnings-as-errors=*
lint-strict: lint

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPFLAGS) ecs main.c

tidy: lint

$(NAME): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_NAME): $(LIB_OBJ) $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(LIB_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS) $(TEST_LDLIBS)

$(BENCH_NAME): $(LIB_OBJ) $(BENCH_OBJ) | $(BIN_DIR)
	$(CC) $(LIB_OBJ) $(BENCH_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

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
