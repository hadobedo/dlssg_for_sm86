/*
 * Headless smoke test used by CI (linux/smoke-test.sh) and useful locally.
 *
 * It plays the part of a game: load version.dll as a native proxy from the
 * application directory, then use its exports. With the Wine fix installed,
 * the proxy initializes and its forwarders reach Wine's real version.dll.
 *
 * Exit codes: 0 pass, 1 proxy failed to load (the bug), 2..6 failed assertion.
 */
#include <windows.h>
#include <stdio.h>

typedef DWORD (WINAPI *PFN_SIZE)(LPCWSTR, LPDWORD);
typedef UINT  (WINAPI *PFN_LANG)(DWORD, LPWSTR, UINT);

int main(void)
{
    HMODULE h;
    FARPROC p;
    WCHAR buf[128];
    DWORD dummy = 0;
    UINT n;

    setvbuf(stdout, NULL, _IONBF, 0);

    h = LoadLibraryA("version.dll");
    if (!h) {
        printf("FAIL: LoadLibraryA(version.dll) err=%lu\n", GetLastError());
        return 1;
    }
    printf("LOADED version.dll\n");

    /* The export whose absence aborts the proxy's DllMain under Wine. */
    if (!GetProcAddress(h, "GetFileVersionInfoByHandle")) {
        printf("FAIL: GetFileVersionInfoByHandle missing\n");
        return 2;
    }
    printf("EXPORT GetFileVersionInfoByHandle present\n");

    /* Forwarding, on a call that needs no file: 0x0409 = English. */
    p = GetProcAddress(h, "VerLanguageNameW");
    if (!p) { printf("FAIL: VerLanguageNameW missing\n"); return 3; }
    n = ((PFN_LANG)p)(0x0409, buf, 128);
    printf("FORWARD VerLanguageNameW(0x0409) = %u\n", n);
    if (!n) { printf("FAIL: VerLanguageNameW forward returned 0\n"); return 4; }

    /* Forwarding, file-backed: Wine's own version.dll copy has a version resource. */
    p = GetProcAddress(h, "GetFileVersionInfoSizeW");
    if (!p) { printf("FAIL: GetFileVersionInfoSizeW missing\n"); return 5; }
    n = ((PFN_SIZE)p)(L"C:\\windows\\system32\\version_orig.dll", &dummy);
    printf("FORWARD GetFileVersionInfoSizeW(version_orig.dll) = %u\n", n);
    if (!n) { printf("FAIL: GetFileVersionInfoSizeW forward returned 0\n"); return 6; }

    printf("PASS\n");
    return 0;
}
