FROM alpine:3.7

RUN apk update
RUN apk add --update cmake make gcc g++ git zlib-dev openssl-dev

RUN mkdir -p /usr/src/spectacles
WORKDIR /usr/src/spectacles

COPY ./src /usr/src/spectacles/src
COPY ./include /usr/src/spectacles/include
COPY ./tools /usr/src/spectacles/tools
COPY ./CMakeLists.txt /usr/src/spectacles/CMakeLists.txt

RUN mkdir build
WORKDIR build

RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RELEASE
RUN make install

RUN cp /usr/lib64/librabbitmq.so.4.2.0 /usr/lib/librabbitmq.so.4.2.0
RUN ln -s /usr/lib/librabbitmq.so.4.2.0 /usr/lib/librabbitmq.so.4
RUN ln -s /usr/lib/librabbitmq.so.4.2.0 /usr/lib/librabbitmq.so

CMD ["./tools/docker"]
