webserver: webserver.c libuv/out/Debug/libuv.a http-parser/http_parser.o
	$(CC) -I libuv/include \
	-o webserver webserver.c http_connection.c \
	libuv/out/Debug/libuv.a http-parser/http_parser.o \
	-lpthread

libuv/out/Debug/libuv.a:
	$(MAKE) -C libuv/out

http-parser/http_parser.o:
	$(MAKE) -C http-parser http_parser.o

clean:
	$(MAKE) -C libuv clean
	$(MAKE) -C http-parser clean
	-rm libuv/uv.a
	-rm http-parser/http_parser.o
	-rm webserver