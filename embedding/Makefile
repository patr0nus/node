.NOTPARALLEL:

MKFILE_DIR := $(abspath $(lastword $(MAKEFILE_LIST))/..)
NODE_DIR := $(dir $(MKFILE_DIR))

VERSION=$(shell python $(NODE_DIR)/tools/getnodeversion.py)

STAGE_DIR := $(MKFILE_DIR)/stage

HEADERS=$(STAGE_DIR)/include.stamp
HEADERS_DIR=$(STAGE_DIR)/include

STATIC_LIBS=$(STAGE_DIR)/static_libs.stamp
STATIC_LIBS_DIR=$(STAGE_DIR)/static_libs

CMAKELISTS_FILE=$(MKFILE_DIR)/cmake/CMakeLists.txt

CMAKE_DIR=$(STAGE_DIR)/cmake
CMAKE=$(CMAKE_DIR).stamp

all: $(CMAKE)

.PHONY: clean
clean:
	rm -r $(STAGE_DIR)
	mkdir $(STAGE_DIR)
	touch $(STAGE_DIR)/.gitkeep


$(HEADERS):
	rm -rf $(HEADERS_DIR) && mkdir $(HEADERS_DIR)
	cd $(NODE_DIR) && make tar-headers
	tar -xf $(NODE_DIR)/node-v$(VERSION)-headers.tar.xz -C $(STAGE_DIR) --strip-components=1
	rm $(NODE_DIR)/node-v$(VERSION)-headers.*
	touch $@

$(STATIC_LIBS):
	rm -rf $(STATIC_LIBS_DIR) && mkdir $(STATIC_LIBS_DIR)

	cd $(NODE_DIR) && ./configure --without-intl --enable-static --without-node-options --ninja
	cd $(NODE_DIR) && ninja -C out/Release

	@for f in $(foreach f, $(wildcard $(NODE_DIR)/out/Release/*.a), ${notdir $f}); do \
		strip -x $(NODE_DIR)/out/Release/$$f -o ${STATIC_LIBS_DIR}/$$f; \
	done

	cp $(NODE_DIR)/out/Release/obj/src/mkcodecache.*.o $(STATIC_LIBS_DIR)

	touch $@


$(CMAKE): $(CMAKELISTS_FILE) $(HEADERS) $(STATIC_LIBS)
	rm -rf $(CMAKE_DIR) && mkdir $(CMAKE_DIR)
	cp $(CMAKELISTS_FILE) $(CMAKE_DIR)
	cp -r $(HEADERS_DIR) $(CMAKE_DIR)
	cp -r $(STATIC_LIBS_DIR) $(CMAKE_DIR)
	echo "int node_dummy_func() { return 0; }" > $(CMAKE_DIR)/dummy.c
	touch $@

.PHONY: test
test: $(CMAKE) FORCE
	cmake -DNODE_CMAKE_DIR=$(CMAKE_DIR) -S $(MKFILE_DIR)/cmake-test -B stage/test-build
	make --directory stage/test-build all test
FORCE: