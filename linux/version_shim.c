/*
 * Wine/Proton compatibility shim for the DLSSG-SM86 `version.dll` proxy.
 *
 * Why this exists
 * ---------------
 * The mod's `version.dll` is a proxy: it forwards every export of the real
 * Windows `version.dll` and its DllMain refuses to initialize (returns FALSE)
 * unless each of those exports resolves on the system DLL it finds. One of
 * them -- `GetFileVersionInfoByHandle` -- is implemented by no Wine build, so
 * under Wine/Proton the proxy dies with STATUS_DLL_INIT_FAILED (0xc0000142)
 * before it can even open its own log file.
 *
 * This shim is installed as the Wine prefix's `system32/version.dll`, with
 * Wine's real implementation renamed to `version_orig.dll` next to it:
 *   - the 16 exports Wine does implement are forwarded to `version_orig.dll`;
 *   - `GetFileVersionInfoByHandle` is provided as a stub returning FALSE.
 * The proxy only probes for that export at load time, so the stub is enough
 * to get DllMain past the check.
 *
 * Built with -nostdlib (see build.sh) so the result imports nothing but
 * KERNEL32 and needs no CRT/UCRT DLLs to exist in the prefix.
 *
 * Diagnosis and the original shim come from tB0nE:
 *   https://github.com/sdli1995/dlssg_for_sm86/issues/10
 *   https://github.com/tB0nE/dlssg_for_sm86 (linux-proton-fix)
 */
#include <windows.h>

/*
 * Every forwarder takes 5 generic 64-bit argument slots, the maximum any real
 * version.dll export uses (GetFileVersionInfoExA/W). This matches the Windows
 * x64 calling convention, where every integer/pointer argument -- whatever its
 * real type -- occupies one register or stack slot; unused slots are ignored
 * by the callee.
 */
typedef LONG_PTR (WINAPI *FN5)(LONG_PTR, LONG_PTR, LONG_PTR, LONG_PTR, LONG_PTR);

static HMODULE hOrig;
static FN5 pGetFileVersionInfoA;
static FN5 pGetFileVersionInfoW;
static FN5 pGetFileVersionInfoExA;
static FN5 pGetFileVersionInfoExW;
static FN5 pGetFileVersionInfoSizeA;
static FN5 pGetFileVersionInfoSizeW;
static FN5 pGetFileVersionInfoSizeExA;
static FN5 pGetFileVersionInfoSizeExW;
static FN5 pVerFindFileA;
static FN5 pVerFindFileW;
static FN5 pVerInstallFileA;
static FN5 pVerInstallFileW;
static FN5 pVerLanguageNameA;
static FN5 pVerLanguageNameW;
static FN5 pVerQueryValueA;
static FN5 pVerQueryValueW;

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        /* Loads the on-disk (native) Wine version.dll renamed beside us. */
        hOrig = LoadLibraryA("version_orig.dll");
        if (hOrig)
        {
            pGetFileVersionInfoA       = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoA");
            pGetFileVersionInfoW       = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoW");
            pGetFileVersionInfoExA     = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoExA");
            pGetFileVersionInfoExW     = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoExW");
            pGetFileVersionInfoSizeA   = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoSizeA");
            pGetFileVersionInfoSizeW   = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoSizeW");
            pGetFileVersionInfoSizeExA = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoSizeExA");
            pGetFileVersionInfoSizeExW = (FN5)GetProcAddress(hOrig, "GetFileVersionInfoSizeExW");
            pVerFindFileA             = (FN5)GetProcAddress(hOrig, "VerFindFileA");
            pVerFindFileW             = (FN5)GetProcAddress(hOrig, "VerFindFileW");
            pVerInstallFileA          = (FN5)GetProcAddress(hOrig, "VerInstallFileA");
            pVerInstallFileW          = (FN5)GetProcAddress(hOrig, "VerInstallFileW");
            pVerLanguageNameA         = (FN5)GetProcAddress(hOrig, "VerLanguageNameA");
            pVerLanguageNameW         = (FN5)GetProcAddress(hOrig, "VerLanguageNameW");
            pVerQueryValueA           = (FN5)GetProcAddress(hOrig, "VerQueryValueA");
            pVerQueryValueW           = (FN5)GetProcAddress(hOrig, "VerQueryValueW");
        }
    }
    return TRUE;
}

LONG_PTR WINAPI fwd_GetFileVersionInfoA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoA ? pGetFileVersionInfoA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoW ? pGetFileVersionInfoW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoExA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoExA ? pGetFileVersionInfoExA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoExW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoExW ? pGetFileVersionInfoExW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoSizeA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoSizeA ? pGetFileVersionInfoSizeA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoSizeW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoSizeW ? pGetFileVersionInfoSizeW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoSizeExA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoSizeExA ? pGetFileVersionInfoSizeExA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_GetFileVersionInfoSizeExW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pGetFileVersionInfoSizeExW ? pGetFileVersionInfoSizeExW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerFindFileA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerFindFileA ? pVerFindFileA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerFindFileW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerFindFileW ? pVerFindFileW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerInstallFileA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerInstallFileA ? pVerInstallFileA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerInstallFileW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerInstallFileW ? pVerInstallFileW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerLanguageNameA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerLanguageNameA ? pVerLanguageNameA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerLanguageNameW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerLanguageNameW ? pVerLanguageNameW(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerQueryValueA(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerQueryValueA ? pVerQueryValueA(a, b, c, d, e) : 0; }

LONG_PTR WINAPI fwd_VerQueryValueW(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ return pVerQueryValueW ? pVerQueryValueW(a, b, c, d, e) : 0; }

/*
 * Not implemented by any Wine build. Stubbed to FALSE (the Win32 BOOL
 * "operation failed" convention) so a caller that actually invokes it gets a
 * defined failure instead of jumping into garbage. The proxy only checks that
 * this export exists.
 */
BOOL WINAPI fwd_GetFileVersionInfoByHandle(LONG_PTR a, LONG_PTR b, LONG_PTR c, LONG_PTR d, LONG_PTR e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; return FALSE; }

/*
 * Loader entry point. -nostdlib drops mingw's CRT startup (and with it every
 * CRT import), so the DLL entry has to be provided here.
 */
BOOL WINAPI DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    return DllMain(inst, reason, reserved);
}
