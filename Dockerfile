FROM ubuntu:latest

WORKDIR /

COPY . /
COPY /usr/lib64/libncurses.so.5.9 /usr/lib64
COPY /usr/lib64/libtinfo.so.5.9 /usr/lib64

EXPOSE 1977/udp
EXPOSE 1978/udp
