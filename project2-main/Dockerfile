FROM --platform=linux/arm64 ubuntu:22.04
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yy --no-install-recommends gcc clang make git valgrind && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*