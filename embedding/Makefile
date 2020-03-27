.NOTPARALLEL:

MKFILE_DIR := $(abspath $(lastword $(MAKEFILE_LIST))/..)
NODE_DIR := $(dir $(MKFILE_DIR))

VERSION=$(shell python $(NODE_DIR)/tools/getnodeversion.py)

STAGE_DIR := $(MKFILE_DIR)/stage
HEADERS=$(STAGE_DIR)/headers.tar.xz
STATIC_LIBS=$(STAGE_DIR)/static_libs.tar

all: $(HEADERS) $(STATIC_LIBS)

.PHONY: clean
clean:
	rm -r $(STAGE_DIR)
	mkdir $(STAGE_DIR)
	touch $(STAGE_DIR)/.gitkeep


$(HEADERS):
	cd $(NODE_DIR) && make tar-headers
	mv $(NODE_DIR)/node-v$(VERSION)-headers.tar.xz $@
	rm $(NODE_DIR)/node-v$(VERSION)-headers.*

$(STATIC_LIBS):
	cd $(NODE_DIR) && ./configure --without-intl --enable-static --without-node-options --ninja
	cd $(NODE_DIR) && ninja -C out/Release/

	rm -rf $(STAGE_DIR)/static_libs && mkdir $(STAGE_DIR)/static_libs
	cp $(NODE_DIR)/out/Release/*.a $(STAGE_DIR)/static_libs
	cp $(NODE_DIR)/out/Release/obj/src/mkcodecache.*.o $(STAGE_DIR)/static_libs

	strip -x $(STAGE_DIR)/static_libs/*.a
	tar -cf $@ -C $(STAGE_DIR)/static_libs .

