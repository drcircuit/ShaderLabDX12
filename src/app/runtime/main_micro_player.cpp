using UINT = unsigned int;
using DWORD = unsigned long;
using BOOL = int;
using LONG = long;
using HANDLE = void*;
using HWND = void*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPVOID = void*;

constexpr DWORD GENERIC_READ = 0x80000000u;
constexpr DWORD FILE_SHARE_READ = 0x00000001u;
constexpr DWORD OPEN_EXISTING = 3u;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080u;
constexpr DWORD FILE_END = 2u;
constexpr DWORD INVALID_FILE_SIZE = 0xFFFFFFFFu;

static HANDLE const INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(static_cast<long long>(-1));

extern "C" __declspec(dllimport) HANDLE __stdcall CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
extern "C" __declspec(dllimport) BOOL __stdcall ReadFile(HANDLE, LPVOID, DWORD, DWORD*, void*);
extern "C" __declspec(dllimport) DWORD __stdcall SetFilePointer(HANDLE, LONG, LONG*, DWORD);
extern "C" __declspec(dllimport) DWORD __stdcall GetFileSize(HANDLE, DWORD*);
extern "C" __declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
extern "C" __declspec(dllimport) DWORD __stdcall GetModuleFileNameA(void*, LPSTR, DWORD);
extern "C" __declspec(dllimport) int __stdcall MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);

namespace {
constexpr DWORD kEmbeddedTrackMagic = 0x4B52544Du; // 'MTRK'

struct EmbeddedTrackFooter {
    DWORD magic;
    DWORD payloadSize;
    DWORD version;
};

struct TrackHeader {
    unsigned short magic0;
    unsigned short magic1;
    unsigned short bpmQ8;
    unsigned short lengthBeats;
    unsigned short rowCount;
};

bool ReadExact(HANDLE file, void* dst, DWORD bytes) {
    DWORD readBytes = 0;
    if (!ReadFile(file, dst, bytes, &readBytes, nullptr)) {
        return false;
    }
    return readBytes == bytes;
}

bool LoadEmbeddedTrackHeader(TrackHeader& outHeader) {
    char exePath[260];
    DWORD pathLen = GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    if (pathLen == 0 || pathLen >= sizeof(exePath)) {
        return false;
    }

    HANDLE file = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize < sizeof(EmbeddedTrackFooter) + sizeof(TrackHeader)) {
        CloseHandle(file);
        return false;
    }

    if (SetFilePointer(file, -static_cast<LONG>(sizeof(EmbeddedTrackFooter)), nullptr, FILE_END) == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return false;
    }

    EmbeddedTrackFooter footer;
    if (!ReadExact(file, &footer, sizeof(footer))) {
        CloseHandle(file);
        return false;
    }

    if (footer.magic != kEmbeddedTrackMagic || footer.payloadSize < sizeof(TrackHeader)) {
        CloseHandle(file);
        return false;
    }

    const DWORD payloadPlusFooter = footer.payloadSize + static_cast<DWORD>(sizeof(EmbeddedTrackFooter));
    if (payloadPlusFooter > fileSize) {
        CloseHandle(file);
        return false;
    }

    if (SetFilePointer(file, -static_cast<LONG>(payloadPlusFooter), nullptr, FILE_END) == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return false;
    }

    bool ok = ReadExact(file, &outHeader, sizeof(outHeader));
    CloseHandle(file);
    if (!ok) {
        return false;
    }

    return outHeader.magic0 == 0x4B54u && outHeader.magic1 == 0x3252u;
}
} // namespace

extern "C" void __stdcall WinMainCRTStartup() {
    TrackHeader header;
    if (LoadEmbeddedTrackHeader(header)) {
        MessageBoxA(nullptr,
                    "MicroPlayer loaded embedded compact timeline payload from executable.",
                    "ShaderLab MicroPlayer",
                    0u);
    } else {
        MessageBoxA(nullptr,
                    "MicroPlayer running, but embedded compact timeline payload was not found.\nBuild with Restricted compact track mode enabled.",
                    "ShaderLab MicroPlayer",
                    0u);
    }

    ExitProcess(0u);
}
