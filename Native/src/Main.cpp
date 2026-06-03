#include <Services.hpp>
#include <Util.hpp>

static void SetupConsole();

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID _)
{
    if (reason == DLL_PROCESS_ATTACH) {
#ifdef _DEBUG
        SetupConsole();
#endif

        LOG_INFO("DLSS_Native loaded!\n");
    }

    return TRUE;
}

static void SetupConsole()
{
    AllocConsole();

    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);

    HANDLE handle = CHECK(CreateFile(L"CONOUT$",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));

    DISCARD(SetStdHandle(STD_OUTPUT_HANDLE, handle));
    DISCARD(SetStdHandle(STD_ERROR_HANDLE, handle));

    DWORD orig = 0;
    DISCARD(GetConsoleMode(handle, &orig));

    if (FAIL(SetConsoleMode(handle, orig | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN))) {
        DISCARD(SetConsoleMode(handle, orig | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    }
}
