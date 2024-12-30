FROM gcc:11.5.0 AS builder

RUN apt-get update && apt-get install cmake -y

WORKDIR /MMKV

COPY . .

## 设置环境变量
#ENV CGO_ENABLED='1'
#ENV CGO_CFLAGS='-static -O2 -g'
#ENV CGO_LDFLAGS='-static -O2 -g'

WORKDIR /MMKV/POSIX/golang
RUN mkdir -p build && cd build && rm -rf ./* && \
    cmake .. -DCMAKE_INSTALL_PREFIX=. \
    -DCMAKE_BUILD_TYPE=Release && \
    make -j8 install

RUN uname -a > /MMKV/POSIX/golang/build/tencent.com/build_info.txt 2>&1 && \
    gcc -v >> /MMKV/POSIX/golang/build/tencent.com/build_info.txt 2>&1 && \
    cmake --version >> /MMKV/POSIX/golang/build/tencent.com/build_info.txt 2>&1

FROM scratch AS export-stage
COPY --from=builder /MMKV/POSIX/golang/build/tencent.com /tencent.com
