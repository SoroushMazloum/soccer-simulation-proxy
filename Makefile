# Directories
BUILD_DIR = build
LIB_RCSC_GITHUB = https://github.com/helios-base/librcsc.git
LIB_RCSC_INSTALL_DIR = $(PWD)/$(BUILD_DIR)/librcsc-install	
LIB_RCSC_PATH = $(BUILD_DIR)/librcsc-install/lib/librcsc.so
GRPC_GITHUB = https://github.com/grpc/grpc.git
GRPC_TAG = v1.60.0
GRPC_INSTALL_DIR = $(PWD)/$(BUILD_DIR)/grpc-install
GRPC_SRC_DIR = $(BUILD_DIR)/grpc-git
GRPC_BUILD_DIR = $(GRPC_SRC_DIR)/cmake/build

.PHONY: all build clean

# The default target
all: build-all

build-src: 
	@echo "Running CMake..."
	cd $(BUILD_DIR) && \
	export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$(LIB_RCSC_INSTALL_DIR)/lib:$(GRPC_INSTALL_DIR)/lib && \
	export PATH=$$PATH:$(GRPC_INSTALL_DIR)/bin && \
	cmake -D LIBRCSC_INSTALL_DIR=$(LIB_RCSC_INSTALL_DIR) \
		  -D LIBRCSC_LIB=$(LIB_RCSC_PATH) \
		  -D GRPC_INSTALL_DIR=$(GRPC_INSTALL_DIR) \
		  -D GRPC_LIB_PATH=$(GRPC_INSTALL_DIR)/lib \
		  -D GRPC_BIN_PATH=$(GRPC_INSTALL_DIR)/bin  .. && \
	make 

build-all: build-folder clone-build-librcsc clone-build-grpc
	@echo "Running CMake..."
	cd $(BUILD_DIR) && cmake -D LIBRCSC_INSTALL_DIR=$(LIB_RCSC_INSTALL_DIR) -D LIBRCSC_LIB=$(LIB_RCSC_PATH) ..


# ------------------- librcsc -------------------
build-folder:
	if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); fi
	
	@echo "Creating build directory..."
	mkdir -p $(BUILD_DIR)

clone-build-librcsc:
	@echo "Cloning librcsc..."
	git clone $(LIB_RCSC_GITHUB) $(BUILD_DIR)/librcsc
	mkdir -p $(LIB_RCSC_INSTALL_DIR)
	cd $(BUILD_DIR)/librcsc && \
		bash $(PWD)/$(BUILD_DIR)/librcsc/bootstrap && \
		./configure --prefix=$(LIB_RCSC_INSTALL_DIR) && \
		make -j$(shell nproc) && \
		make install

clean-librcsc-source:
	@echo "Cleaning up librcsc sources..."
	rm -rf $(BUILD_DIR)/librcsc

# ------------------- gRPC -------------------
clone-build-grpc:
	@echo "Cloning gRPC..."
	git clone --recurse-submodules -b $(GRPC_TAG) --depth 1 --shallow-submodules $(GRPC_GITHUB) $(GRPC_SRC_DIR)
	mkdir -p $(GRPC_INSTALL_DIR)
	mkdir -p $(GRPC_BUILD_DIR)
	cd $(GRPC_BUILD_DIR) && \
		cmake -DgRPC_INSTALL=ON \
			  -DgRPC_BUILD_TESTS=OFF \
			  -DCMAKE_CXX_FLAGS=-std=c++17 \
			  -DCMAKE_INSTALL_PREFIX=$(GRPC_INSTALL_DIR) ../.. &&\
		make -j$(shell nproc) &&\
		make install

clean-grpc-source:
	@echo "Cleaning up gRPC sources..."
	rm -rf $(GRPC_SRC_DIR)



# ------------------- Clean -------------------
clean-sources: clean-librcsc-source clean-grpc-source


clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)