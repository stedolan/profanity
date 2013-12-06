mongoose/mongoose.o: mongoose/mongoose.c
	cc -c -O2 $^ -o $@ -DNO_SSL_DL -DNO_SSL -DUSE_WEBSOCKET

prof.so: server.c override.c mongoose/mongoose.o
	cc -ggdb $^ -Imongoose -o $@ -pthread -shared -fpic -ldl -Wall

test: test.c
	cc $^ -o $@