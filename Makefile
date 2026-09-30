# Simple developer entry points for pdf-core.

CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build
BUILD_TYPE ?= Debug
GENERATOR ?=
DEPS_DIR ?= .deps
FORMAT_TARGET ?= pdf_ripper_core_format
FORMAT_CHECK_TARGET ?= pdf_ripper_core_format_check
TIDY_TARGET ?= pdf_ripper_core_tidy

CMAKE_CONFIGURE_ARGS := -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
ifneq ($(strip $(GENERATOR)),)
CMAKE_CONFIGURE_ARGS += -G "$(GENERATOR)"
endif

.PHONY: help configure build test format format-check tidy clean rebuild install install-check depclean

help:
	@echo "Available targets:"
	@echo "  make configure     - Configure CMake in $(BUILD_DIR)"
	@echo "  make build         - Configure and build library + tests"
	@echo "  make test          - Build and run CTest suite"
	@echo "  make format        - Apply clang-format to project sources"
	@echo "  make format-check  - Verify clang-format compliance"
	@echo "  make tidy          - Run clang-tidy static analysis"
	@echo "  make install       - Install from $(BUILD_DIR)"
	@echo "  make install-check - Install to a temp prefix and verify an external CMake consumer"
	@echo "  make clean         - Remove $(BUILD_DIR) and $(DEPS_DIR)"
	@echo "  make rebuild       - Clean then build"
	@echo "  make depclean      - Remove $(DEPS_DIR) only"
	@echo "Variables: BUILD_DIR, BUILD_TYPE, GENERATOR, DEPS_DIR, FORMAT_TARGET, FORMAT_CHECK_TARGET, TIDY_TARGET"

configure:
	@mkdir -p $(BUILD_DIR)
	$(CMAKE) $(CMAKE_CONFIGURE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

lint: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(FORMAT_TARGET)

format: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(FORMAT_TARGET)

format-check: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(FORMAT_CHECK_TARGET)

tidy: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(TIDY_TARGET)

install: build
	$(CMAKE) --install $(BUILD_DIR)

install-check:
	./scripts/verify_install.sh

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR) $(DEPS_DIR)

depclean:
	@echo "Removing $(DEPS_DIR)..."
	$(CMAKE) -E rm -rf $(DEPS_DIR)

rebuild: clean build
