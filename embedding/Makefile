.NOTPARALLEL:

UNAME_OS := $(shell uname -s)
ifneq (,$(findstring NT,$(UNAME_OS)))
	OS := win
endif
ifeq ($(UNAME_OS), Linux)
	OS := linux
endif
ifeq ($(UNAME_OS), Darwin)
	OS := darwin
endif

DEST_ARCH ?= x64
ifeq ($(DEST_ARCH), x64)
	DEST_ARCH_X64 := 1
endif
ifeq ($(DEST_ARCH), x86)
	DEST_ARCH_X86 := 1
endif

MKFILE_DIR := $(abspath $(lastword $(MAKEFILE_LIST))/..)
NODE_DIR := $(dir $(MKFILE_DIR))

VERSION := $(shell python $(NODE_DIR)/tools/getnodeversion.py)

STAGE_DIR := $(MKFILE_DIR)/stage

BUILD_CONFIG := --without-intl --without-node-options

HEADERS := $(STAGE_DIR)/include.stamp
HEADERS_DIR := $(STAGE_DIR)/include

STATIC_LIBS := $(STAGE_DIR)/static_libs.stamp
STATIC_LIBS_DIR := $(STAGE_DIR)/static_libs

CMAKELISTS_FILE := $(MKFILE_DIR)/cmake/CMakeLists.txt

CMAKE_DIR := $(STAGE_DIR)/cmake
CMAKE := $(CMAKE_DIR).stamp

all: $(CMAKE)

.PHONY: clean
clean:
	rm -r $(STAGE_DIR)
	mkdir $(STAGE_DIR)
	touch $(STAGE_DIR)/.gitkeep


$(HEADERS):
	rm -rf $(HEADERS_DIR) && mkdir $(HEADERS_DIR)
	cd $(NODE_DIR) && HEADERS_ONLY=1 ./tools/install.py install "" "headers"
	mv $(NODE_DIR)/headers/include $(STAGE_DIR)
	touch $@

$(STATIC_LIBS):
	rm -rf $(STATIC_LIBS_DIR) && mkdir $(STATIC_LIBS_DIR)

ifeq ($(OS), Windows)
	cd $(NODE_DIR) && config_flags="$(BUILD_CONFIG)" ./vcbuild.bat static $(if $(DEST_ARCH_X86),x86,x64)
	cp $(NODE_DIR)/out/Release/lib/*.lib $(STATIC_LIBS_DIR)
	cp $(NODE_DIR)/out/Release/obj/mkcodecache/node_*.obj $(STATIC_LIBS_DIR)
else
	cd $(NODE_DIR) && ./configure --enable-static --ninja $(BUILD_CONFIG)
	cd $(NODE_DIR) && ninja -C out/Release

	cp $(NODE_DIR)/out/Release/obj/src/mkcodecache.*.o $(STATIC_LIBS_DIR)
ifeq ($(OS), Linux)
	@echo Converting thin archives...
	@for f in $(shell find $(NODE_DIR)/out/Release/obj -type f -name '*.a'); do \
		ar -t $$f | xargs ar rs $(STATIC_LIBS_DIR)/$$(basename $$f) ; \
	done
else
	@echo Stripping archives...
	@for f in $(foreach f, $(wildcard $(NODE_DIR)/out/Release/*.a), ${notdir $f}); do \
		strip -x $(NODE_DIR)/out/Release/$$f -o ${STATIC_LIBS_DIR}/$$f; \
	done
endif

endif
	touch $@


$(CMAKE): $(CMAKELISTS_FILE) $(HEADERS) $(STATIC_LIBS)
	rm -rf $(CMAKE_DIR) && mkdir $(CMAKE_DIR)
	cp $(CMAKELISTS_FILE) $(CMAKE_DIR)
	cp -r $(HEADERS_DIR) $(CMAKE_DIR)
	cp -r $(STATIC_LIBS_DIR) $(CMAKE_DIR)
	echo "int node_dummy_func() { return 0; }" > $(CMAKE_DIR)/dummy.c
	touch $@

.PHONY: test
test: $(CMAKE)
ifeq ($(OS), Windows)
	cmake -DNODE_CMAKE_DIR=$(CMAKE_DIR) -A $(if $(DEST_ARCH_X86),Win32,x64) -S $(MKFILE_DIR)/cmake-test -B $(STAGE_DIR)/test-build
	cd $(STAGE_DIR)/test-build && cmake --build . --config Release
	cd $(STAGE_DIR)/test-build && ctest -C Release
else
	cmake -DNODE_CMAKE_DIR=$(CMAKE_DIR) -GNinja -S $(MKFILE_DIR)/cmake-test -B $(STAGE_DIR)/test-build
	cd $(STAGE_DIR)/test-build && ninja
	cd $(STAGE_DIR)/test-build && ctest
endif


TAR_PATH := $(STAGE_DIR)/$(TAR_FILENAME)

.PHONY: tar
tar: $(CMAKE)
	@cd $(CMAKE_DIR) && tar -czf $(TAR_PATH) *
	@echo $(TAR_PATH)
