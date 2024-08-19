#------------------------------------------------------
# build stage
#------------------------------------------------------

FROM sadraiiali/rcss2d-grpc-base:ubuntu-20.04 AS BUILD

WORKDIR /app

ENV GRPC_LIB_PATH=/root/.local/grpc
ENV RCSC_LIB_PATH=/root/.local/rcsc

COPY . /app/code
RUN cd /app/code \
    && export PATH="$GRPC_LIB_PATH/bin:$RCSC_LIB_PATH/bin:$PATH" \
    && export LD_LIBRARY_PATH=$GRPC_LIB_PATH/lib:$RCSC_LIB_PATH/lib:$LD_LIBRARY_PATH \
    && export PROTOBUF_DIR=$GRPC_LIB_PATH/include/google/protobuf/ \
    && protoc --proto_path=./grpc/protos --cpp_out=./src/grpc ./grpc/protos/service.proto \
    && mkdir -p build \
    && cd build \
    && cmake -DCMAKE_CXX_FLAGS=-std=c++17 .. \
    && make -j11

RUN mkdir -p /app/bin \
    && cp -r /app/code/build/bin/* /app/bin \
    && mkdir -p /app/bin/lib \
    && cp $RCSC_LIB_PATH/lib/librcsc.so.18 /app/bin/lib \
    && cp /lib/x86_64-linux-gnu/libstdc++.so.6 /app/bin/lib \
    && cp /lib/x86_64-linux-gnu/libm.so.6 /app/bin/lib \ 
    && cp /lib/x86_64-linux-gnu/libgcc_s.so.1 /app/bin/lib \
    && cp $GRPC_LIB_PATH/lib/libz.so.1 /app/bin/lib 



# DEPLOY STAGE
#------------------------------------------------------
FROM ubuntu:20.04

WORKDIR /app
ENV TEAM_NAME=teamname
ENV HOST=127.0.0.1
ENV PORT=6000
ENV G_IP=0.0.0.0
ENV G_PORT=50051

COPY --from=BUILD /app/bin /app/bin
COPY images/rcss2d-grpc-agent/docker-entrypoint.sh /app
CMD ["bash", "/app/docker-entrypoint.sh"]


