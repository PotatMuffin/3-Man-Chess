#define COMPILER "gcc"
#define PLATFORM_PREFIX "linux_"
#define LIBRAYLIB_PATH RAYLIB_BUILD_DIR PLATFORM_PREFIX LIBRAYLIB_A
#define COMMON_PATH BUILD_DIR PLATFORM_PREFIX COMMON_A

bool build_raylib_linux()
{
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths object_files = {0};

    for(int i = 0; i < NOB_ARRAY_LEN(raylib_modules); i++)
    {
        char *source_file = nob_temp_sprintf("%s%s.c", RAYLIB_SRC_DIR,   raylib_modules[i]);
        char *object_file = nob_temp_sprintf("%s"PLATFORM_PREFIX"%s.o", RAYLIB_BUILD_DIR, raylib_modules[i]);
        nob_da_append(&object_files, object_file);

        bool rebuild = nob_needs_rebuild1(object_file, source_file);
        if(!rebuild) continue;

        nob_cmd_append(&cmd, COMPILER, "-DPLATFORM_DESKTOP", "-D_GLFW_X11", "-fPIC", "-I"GLFW_DIR);
        nob_cmd_append(&cmd, "-c", source_file);
        nob_cmd_append(&cmd, "-o", object_file);
        nob_cmd_append(&cmd, "-O3", "-ggdb");

        Nob_Proc proc = nob_cmd_run_async_and_reset(&cmd);
        nob_da_append(&procs, proc);
    }

    if(!nob_procs_wait(procs)) return false;

    if(nob_needs_rebuild(LIBRAYLIB_PATH, object_files.items, object_files.count))
    {
        cmd.count = 0;
        nob_cmd_append(&cmd, COMPILER"-ar", "-crs");
        nob_cmd_append(&cmd, LIBRAYLIB_PATH);

        for(int i = 0; i < object_files.count; i++)
        {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        if(!nob_cmd_run_sync(cmd)) return false;
    }
    return true;
}

bool build_common_linux()
{
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths object_files = {0};

    for(int i = 0; i < NOB_ARRAY_LEN(common_files); i++)
    {
        char *source_file = nob_temp_sprintf("%s%s.c", SRC_DIR"common/",            common_files[i]);
        char *object_file = nob_temp_sprintf("%s"PLATFORM_PREFIX"%s.o", BUILD_DIR,  common_files[i]);
        nob_da_append(&object_files, object_file);

        bool rebuild = nob_needs_rebuild1(object_file, source_file);
        if(!rebuild) continue;

        nob_cmd_append(&cmd, COMPILER, "-fPIC");
        nob_cmd_append(&cmd, "-c", source_file);
        nob_cmd_append(&cmd, "-o", object_file);
        nob_cmd_append(&cmd, "-O3", "-ggdb");

        Nob_Proc proc = nob_cmd_run_async_and_reset(&cmd);
        nob_da_append(&procs, proc);
    }

    if(!nob_procs_wait(procs)) return false;

    if(nob_needs_rebuild(COMMON_PATH, object_files.items, object_files.count))
    {
        cmd.count = 0;
        nob_cmd_append(&cmd, COMPILER"-ar", "-crs");
        nob_cmd_append(&cmd, COMMON_PATH);

        for(int i = 0; i < object_files.count; i++)
        {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        if(!nob_cmd_run_sync(cmd)) return false;
    }
    return true;
}

bool build_client_linux()
{
    Nob_Cmd cmd = {0};
    Nob_File_Paths deps = {0};
    nob_da_append(&deps, CLIENT_PATH);
    nob_da_append(&deps, LIBRAYLIB_PATH);
    nob_da_append(&deps, COMMON_PATH);

    bool rebuild = nob_needs_rebuild(CLIENT_OUTPUT_PATH, deps.items, deps.count);
    if (!rebuild) return true;

    nob_cmd_append(&cmd, COMPILER, "-o", CLIENT_OUTPUT_PATH);

    for(int i = 0; i < deps.count; i++)
    {
        nob_cmd_append(&cmd, deps.items[i]);
    }

    nob_cmd_append(&cmd, "-I"RAYLIB_SRC_DIR);
    nob_cmd_append(&cmd, "-lm", "-O1", "-ggdb");

    if(!nob_cmd_run_sync(cmd)) return false;

    return true;
}

bool build_server_linux()
{
    Nob_Cmd cmd = {0};
    Nob_File_Paths deps = {0};
    nob_da_append(&deps, SERVER_PATH);
    nob_da_append(&deps, LIBRAYLIB_PATH);
    nob_da_append(&deps, COMMON_PATH);

    bool rebuild = nob_needs_rebuild(SERVER_OUTPUT_PATH, deps.items, deps.count);
    if (!rebuild) return true;

    nob_cmd_append(&cmd, COMPILER, "-o", SERVER_OUTPUT_PATH);

    for(int i = 0; i < deps.count; i++)
    {
        nob_cmd_append(&cmd, deps.items[i]);
    }

    nob_cmd_append(&cmd, "-I"RAYLIB_SRC_DIR);
    nob_cmd_append(&cmd, "-lm", "-O1", "-ggdb");

    if(!nob_cmd_run_sync(cmd)) return false;

    return true;
}

#undef COMPILER
#undef PLATFORM_PREFIX
#undef LIBRAYLIB_PATH
#undef COMMON_PATH
