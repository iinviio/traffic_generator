FROM gcc:latest as builder
COPY . /usr/src
WORKDIR /usr/src
RUN gcc -o main -static main.c

FROM ubuntu:latest as runtime
COPY --from=builder /usr/src/main /main
ENTRYPOINT ["./main"]


