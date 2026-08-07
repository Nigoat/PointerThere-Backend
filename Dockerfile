# ── Stage 1: Build Drogon & PointerThere Backend ──────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies & libraries
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    git \
    make \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    libpq-dev \
    libcurl4-openssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Clone and install Drogon framework
WORKDIR /tmp
RUN git clone --recursive https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf drogon

# Install header-only libraries (jwt-cpp & bcrypt)
RUN mkdir -p /usr/local/include/jwt-cpp /usr/local/include/bcrypt && \
    wget -O /usr/local/include/jwt-cpp/jwt.h https://raw.githubusercontent.com/Thalhammer/jwt-cpp/master/include/jwt-cpp/jwt.h && \
    wget -O /usr/local/include/bcrypt/BCrypt.hpp https://raw.githubusercontent.com/tristanpenman/HeaderOnlyBCrypt/master/include/BCrypt.hpp

# Build PointerThere Backend binary
WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ── Stage 2: Runtime Image ────────────────────────────────
FROM ubuntu:22.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime shared libraries only
RUN apt-get update && apt-get install -y \
    libjsoncpp25 \
    uuid-runtime \
    zlib1g \
    libssl3 \
    libpq5 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/pointerthere_backend /app/pointerthere_backend

EXPOSE 8080

CMD ["./pointerthere_backend"]
