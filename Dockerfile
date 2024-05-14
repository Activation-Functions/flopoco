FROM debian:latest

ARG SCALP_BACKEND
ENV backend=$SCALP_BACKEND

RUN apt update
RUN yes | apt install git make sudo
RUN git clone https://gitlab.com/flopoco/flopoco
RUN cd flopoco && git checkout dev/cmake && make SCALP_BACKEND=${backend}
