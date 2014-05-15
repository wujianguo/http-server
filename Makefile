SRC_FILES := \
	http_parser/http_parser.c \
	webserver.c \
	http_client/http_response.c \
	http_client/http_request.c \
	common/rbtree.c \
	http_handler/http_handler.c \
	http_handler/main_handler.c \
	http_handler/not_found_handler.c \

OBJECTS =  $(subst .c,.o,$(SRC_FILES))
DESPS = $(subst .c,.d,$(SRC_FILES))
LOCAL_MODULE = webserver
CFLAGS = -luv -I./
CC = gcc
RM = rm

$(LOCAL_MODULE):$(OBJECTS)
	$(CC) -o $(LOCAL_MODULE) $(OBJECTS) $(CFLAGS)

$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	$(RM) $(OBJECTS) $(LOCAL_MODULE) $(DESPS)

include $(SRC_FILES:.c=.d)

%.d: %.c
	set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$