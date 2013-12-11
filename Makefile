all: libprofanity.a

mongoose/mongoose.o: mongoose/mongoose.c
	gcc -c -O2 $^ -o $@ -DNO_SSL_DL -DNO_SSL -DUSE_WEBSOCKET

profanity.o: profanity.c
	gcc -std=c99 -c $< -o $@ -O -ggdb -Wall

libprofanity.a: mongoose/mongoose.o profanity.o
	rm -f $@; ar cr $@ $^; ranlib $@




# Compile programs using profanity like this:

profanity_test.o: profanity_test.c
	c99 -c $< -o $@ `./flags c`

profanity_test: profanity_test.o libprofanity.a
	c99 $< -o $@ `./flags ld`
