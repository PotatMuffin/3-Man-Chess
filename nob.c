#define NOB_IMPLEMENTATION
#include "nob.h"

#include "version.h"

#define THREE_MAN_CHESS "3_man_chess"
#define RAYLIB_SRC_DIR "./dep/raylib/src/"
#define GLFW_DIR RAYLIB_SRC_DIR"external/glfw/include/"
#define BUILD_DIR "./build/"
#define CLIENT_NAME "3_man_chess"
#define SERVER_NAME "3_man_chess_server"
#define CLIENT_OUTPUT_PATH BUILD_DIR CLIENT_NAME
#define SERVER_OUTPUT_PATH BUILD_DIR SERVER_NAME
#define RAYLIB_BUILD_DIR BUILD_DIR"raylib/"
#define LIBRAYLIB_A "libraylib.a"
#define SRC_DIR "./src/"
#define COMMON_DIR SRC_DIR"common/"
#define CLIENT_PATH SRC_DIR"client.c"
#define SERVER_PATH SRC_DIR"server.c"
#define COMMON_A "common.a" 
#define ASSETS_DIR "./assets/"
#define BUNDLE_H_PATH BUILD_DIR"bundle.h"
#define SHIP_DIR "./ship/"
#define COMMON_H_PATH COMMON_DIR"common.h"

typedef struct {
    char   *file;
    size_t offset;
    size_t length;
} Asset;

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
    "sockets",
    "movenotation",
};

Asset assets[] = {
    { .file = "pieces.png",    .offset = 0, .length = 0 },
    { .file = "move.mp3",      .offset = 0, .length = 0 },
    { .file = "check.mp3",     .offset = 0, .length = 0 },
    { .file = "castle.mp3",    .offset = 0, .length = 0 },
    { .file = "promote.mp3",   .offset = 0, .length = 0 },
    { .file = "capture.mp3",   .offset = 0, .length = 0 },
    { .file = "illegal.mp3",   .offset = 0, .length = 0 },
};

#include "build_src/nob_linux.c"
#include "build_src/nob_mingw.c"

bool bundle_assets()
{
    Nob_String_Builder bundle = { 0 };
    bool rebuild = false;

    for(int i = 0; i < NOB_ARRAY_LEN(assets); i++)
    {
        const char *path = nob_temp_sprintf("%s%s", ASSETS_DIR, assets[i].file);
        if(nob_needs_rebuild(BUNDLE_H_PATH, &path, 1)) rebuild = true;
        nob_temp_reset();
    }

    if(!rebuild) return true;
    nob_log(NOB_INFO, "bundling assets!");

    for(int i = 0; i < NOB_ARRAY_LEN(assets); i++)
    {
        char *path = nob_temp_sprintf("%s%s", ASSETS_DIR, assets[i].file);
        assets[i].offset = bundle.count;
        nob_read_entire_file(path, &bundle);
        assets[i].length = bundle.count - assets[i].offset;
        nob_temp_reset();
    }

    Nob_String_View bundle_view = nob_sb_to_sv(bundle);
    Nob_String_Builder bundle_h_content = {0};

    nob_sb_appendf(&bundle_h_content, "#include <stdint.h>\n");
    nob_sb_appendf(&bundle_h_content, "typedef struct {\n\tchar *file;\n\tuint32_t offset;\n\tuint32_t length;\n} Asset;\n");
    nob_sb_appendf(&bundle_h_content, "Asset assets[] = {\n");


    for(int i = 0; i < NOB_ARRAY_LEN(assets); i++)
    {
        nob_sb_appendf(&bundle_h_content, "\t{ .file = \"%s\", .offset = %ld, .length = %ld },\n", assets[i].file, assets[i].offset, assets[i].length);
    }
    nob_sb_appendf(&bundle_h_content, "};\n");
    nob_sb_appendf(&bundle_h_content, "extern const char bundle[];\n");

    nob_sb_appendf(&bundle_h_content, "#ifdef BUNDLE_CONTENT\n");
    nob_sb_appendf(&bundle_h_content, "const char bundle[] = {\n");

    int i = 0;
    while(i < bundle_view.count)
    {
        nob_sb_appendf(&bundle_h_content, "\t");
        for(int j = 0; j < 20 && i < bundle_view.count; j++, i++)
        {
            nob_sb_appendf(&bundle_h_content, "0x%02hhX, ", bundle_view.data[i]);
        }
        nob_sb_appendf(&bundle_h_content, "\n");
    }
    nob_sb_appendf(&bundle_h_content, "};\n");
    nob_sb_appendf(&bundle_h_content, "#endif // BUNDLE_CONTENT\n");

    nob_write_entire_file(BUNDLE_H_PATH, bundle_h_content.items, bundle_h_content.count);

    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./build_src/nob_linux.c", "./build_src/nob_mingw.c");
    if(!nob_mkdir_if_not_exists(BUILD_DIR)) return 1;
    if(!nob_mkdir_if_not_exists(RAYLIB_BUILD_DIR)) return 1;

    if(!bundle_assets()) return 1;

    if(!build_raylib_linux()) return 1;
    if(!build_common_linux()) return 1;
    if(!build_client_linux()) return 1;
    if(!build_server_linux()) return 1;

    if(!build_raylib_mingw()) return 1;
    if(!build_common_mingw()) return 1;
    if(!build_client_mingw()) return 1;
    if(!build_server_mingw()) return 1;

    char *program = nob_shift_args(&argc, &argv);

    if(argc > 0)
    {
        char *option = nob_shift_args(&argc, &argv);
        if(strcmp(option, "--help") == 0)
        {
            printf("usage: %s\n", program);
            printf("options:\n");
            printf("\t--help: print this message\n");
            printf("\t--ship: make files ready for shipping\n");
        }
        else if(strcmp(option, "--ship") == 0)
        {
            if(!nob_mkdir_if_not_exists(SHIP_DIR)) return 1;

            printf("preparing files for shipping!\n");
            const char *linux_archive = nob_temp_sprintf(SHIP_DIR"3_man_chess_%d.%d.%d_linux.zip", MAJOR, MINOR, PATCH);
            const char *windows_archive = nob_temp_sprintf(SHIP_DIR"3_man_chess_%d.%d.%d_windows.zip", MAJOR, MINOR, PATCH);

            const char *linux_client_ship_path   = nob_temp_sprintf("%s/%s_%d.%d.%d",     THREE_MAN_CHESS, CLIENT_NAME, MAJOR, MINOR, PATCH);
            const char *linux_server_ship_path   = nob_temp_sprintf("%s/%s_%d.%d.%d",     THREE_MAN_CHESS, SERVER_NAME, MAJOR, MINOR, PATCH);
            const char *windows_client_ship_path = nob_temp_sprintf("%s/%s_%d.%d.%d.exe", THREE_MAN_CHESS, CLIENT_NAME, MAJOR, MINOR, PATCH);
            const char *windows_server_ship_path = nob_temp_sprintf("%s/%s_%d.%d.%d.exe", THREE_MAN_CHESS, SERVER_NAME, MAJOR, MINOR, PATCH);

            if(!nob_mkdir_if_not_exists(THREE_MAN_CHESS)) return 1;
            if(!nob_copy_file(CLIENT_OUTPUT_PATH,       linux_client_ship_path))   return 1;
            if(!nob_copy_file(SERVER_OUTPUT_PATH,       linux_server_ship_path))   return 1;
            if(!nob_copy_file(CLIENT_OUTPUT_PATH".exe", windows_client_ship_path)) return 1;
            if(!nob_copy_file(SERVER_OUTPUT_PATH".exe", windows_server_ship_path)) return 1;

            Nob_Cmd cmd = { 0 };
            nob_cmd_append(&cmd, "zip", "-q", linux_archive);
            nob_cmd_append(&cmd, linux_client_ship_path, linux_server_ship_path);
            nob_cmd_run_sync_and_reset(&cmd);

            nob_cmd_append(&cmd, "zip", "-q", windows_archive);
            nob_cmd_append(&cmd, windows_client_ship_path, windows_server_ship_path);
            nob_cmd_run_sync_and_reset(&cmd);
        }
    }

    return 0;
}