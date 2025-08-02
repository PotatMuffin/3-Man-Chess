#define NOB_IMPLEMENTATION
#include "nob.h"

#define RAYLIB_SRC_DIR "./dep/raylib/src/"
#define GLFW_DIR RAYLIB_SRC_DIR"external/glfw/include/"
#define BUILD_DIR "./build/"
#define CLIENT_OUTPUT_PATH BUILD_DIR"3_man_chess"
#define SERVER_OUTPUT_PATH BUILD_DIR"3_man_chess_server"
#define RAYLIB_BUILD_DIR BUILD_DIR"raylib/"
#define LIBRAYLIB_A "libraylib.a"
#define SRC_DIR "./src/"
#define CLIENT_PATH SRC_DIR"client.c"
#define SERVER_PATH SRC_DIR"server.c"
#define COMMON_A "common.a" 

char *raylib_modules[] = {
    "rcore",
    "rglfw",
    "utils",
    "rtext",
    "raudio",
    "rtextures",
    "rshapes",
    "rmodels",
};

char *common_files[] = {
    "movegen",
    "board",
    "fen",
    "sockets"
};

#include "build_src/nob_linux.c"
#include "build_src/nob_mingw.c"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./build_src/nob_linux.c", "./build_src/nob_mingw.c");
    if(!nob_mkdir_if_not_exists(BUILD_DIR)) return 1;
    if(!nob_mkdir_if_not_exists(RAYLIB_BUILD_DIR)) return 1;

    if(!build_raylib_linux()) return 1;
    if(!build_common_linux()) return 1;
    if(!build_client_linux()) return 1;
    if(!build_server_linux()) return 1;

    if(!build_raylib_mingw()) return 1;
    if(!build_common_mingw()) return 1;
    if(!build_client_mingw()) return 1;
    if(!build_server_mingw()) return 1;

    return 0;
}