gcc src/client.c src/common/*.c -o build/client dep/raylib/raylib.a -lm -O1
gcc src/server.c src/common/*.c -o build/server -O1