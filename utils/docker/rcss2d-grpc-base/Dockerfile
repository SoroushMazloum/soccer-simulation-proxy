#------------------------------------------------------
# build stage
#------------------------------------------------------

FROM ubuntu:20.04 AS BUILD

WORKDIR /app

ENV GRPC_LIB_PATH=/root/.local/grpc
ENV RCSC_LIB_PATH=/root/.local/rcsc
ENV LIBRCSC_VERSION=19175f339dcb

# install dependencies
RUN apt-get clean && apt-get update --allow-insecure-repositories && \
    DEBIAN_FRONTEND="noninteractive" apt-get -y install \
    tzdata \    
    sudo \
    gcc \
    g++ \
    wget \
    flex \
    bison \
    libboost-all-dev \
    automake \
    make \
    cmake \
    iputils-ping \
    build-essential \
    automake \
    autoconf \
    libtool \
    git 



# Install librcsc from source
RUN mkdir -p $RCSC_LIB_PATH \
    && git clone https://github.com/helios-base/librcsc.git \
    && cd librcsc \
    && git checkout $LIBRCSC_VERSION \
    && ./bootstrap \
    && ./configure --prefix=$RCSC_LIB_PATH \
    && make -j11 \
    && make install


# Install grpc from source
RUN git clone --recurse-submodules -b v1.60.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc grpc-git \
    && mkdir -p $GRPC_LIB_PATH \
    && cd grpc-git \
    && mkdir -p cmake/build \
    && cd cmake/build \
    && cmake -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DCMAKE_CXX_FLAGS=-std=c++17 \
    -DCMAKE_INSTALL_PREFIX=$GRPC_LIB_PATH \
    ../.. \
    && make -j 11 \ 
    && make install \
    && cd ../.. 


# remove packages
RUN apt-get clean && rm -rf /var/lib/apt/lists/*


#------------------------------------------------------
FROM ubuntu:20.04
WORKDIR /app
COPY --from=BUILD /root/.local /root/.local

# install dependencies
RUN apt-get clean && apt-get update --allow-insecure-repositories && \
    DEBIAN_FRONTEND="noninteractive" apt-get -y install \
    tzdata \    
    sudo \
    gcc \
    g++ \
    wget \
    flex \
    bison \
    libboost-all-dev \
    automake \
    make \
    cmake \
    iputils-ping \
    build-essential \
    automake \
    autoconf \
    libtool \
    git \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

