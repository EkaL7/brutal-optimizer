#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "resource.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <d3d11.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <wbemidl.h>
#include <winhttp.h>
#include <wincodec.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0;
static UINT g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static std::filesystem::path g_AppRoot;
static ID3D11ShaderResourceView* g_LogoTexture = nullptr;
static int g_LogoWidth = 0;
static int g_LogoHeight = 0;
static HWND g_Hwnd = nullptr;
static ImFont* g_FontRegular = nullptr;
static ImFont* g_FontBold = nullptr;
static std::atomic_bool g_SpecServerRunning{ false };
static SOCKET g_SpecServerSocket = INVALID_SOCKET;
static std::thread g_SpecServerThread;
static std::mutex g_SpecMutex;
static std::mutex g_ProcessSampleMutex;
static std::filesystem::path g_ExtractedOptimizerScript;
static HANDLE g_OptimizerProcess = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct AppConfig {
    std::string appVersion = "0.1.0";
    std::string authEndpoint = "https://YOUR_BACKEND_HOST/verify";
    std::string discordInviteUrl = "https://discord.gg/YOUR_INVITE_CODE";
    std::string guildId = "YOUR_GUILD_ID";
};

struct SystemInfo {
    std::string cpu = "Scanning...";
    std::string gpu = "Scanning...";
    std::vector<std::string> gpus;
    std::string motherboard = "Scanning...";
    std::string bios = "Scanning...";
    std::string os = "Scanning...";
    std::string ram = "Scanning...";
    std::string storage = "Scanning...";
    std::vector<std::string> storageDevices;
    std::string cpuThreads = "Scanning...";
    std::string architecture = "Scanning...";
    std::string display = "Scanning...";
    std::vector<std::string> displays;
    std::string temperature = "N/A";
    std::string computerName = "Scanning...";
    std::string windowsUser = "Scanning...";
    std::string machineHash = "Scanning...";
    int memoryLoadPercent = 0;
    double storageFreePercent = 0.0;
};

struct ProcessInfo {
    DWORD pid = 0;
    std::string name;
    std::string path;
    double cpuPercent = 0.0;
    double ioMbps = 0.0;
    uint64_t memoryBytes = 0;
    int impactScore = 0;
    std::string impactLabel = "BAIXO";
    bool canClose = false;
    bool isBrowser = false;
};

struct ProcessSample {
    ULONGLONG cpuTime100ns = 0;
    uint64_t readBytes = 0;
    uint64_t writeBytes = 0;
    std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now();
};

struct AuthResult {
    bool allowed = false;
    bool previewMode = false;
    int httpStatus = 0;
    int guildMemberCount = -1;
    int totalRuns = -1;
    int activeUsers = -1;
    std::string message;
};

enum class Screen {
    Loading,
    Login,
    Terms,
    Main
};

struct AppState {
    AppConfig config;
    SystemInfo system;
    AuthResult auth;
    Screen screen = Screen::Loading;
    std::array<char, 32> discordId{};
    bool systemReady = false;
    bool authInFlight = false;
    bool acceptedTerms = false;
    bool optimizeLaunched = false;
    bool phantomFace = false;
    bool specSiteOpened = false;
    std::string authStatus = "AGUARDANDO";
    std::string optimizeStatus = "PRONTO";
    std::future<SystemInfo> systemFuture;
    std::future<AuthResult> authFuture;
    std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
};

static SystemInfo g_SpecInfo;
static std::unordered_map<DWORD, ProcessSample> g_ProcessSamples;

static std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size > 0 ? size - 1 : 0, '\0');
    if (size > 0)
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(size > 0 ? size - 1 : 0, L'\0');
    if (size > 0)
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

static std::string Trim(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
    return value;
}

static std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::filesystem::path ExecutableDirectory()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0)
        return std::filesystem::current_path();
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

static std::filesystem::path FindAppRoot()
{
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path(),
        ExecutableDirectory()
    };

    for (auto candidate : candidates) {
        for (int depth = 0; depth < 8 && !candidate.empty(); ++depth) {
            if (std::filesystem::exists(candidate / "config" / "kittycore.json") &&
                std::filesystem::exists(candidate / "scripts"))
                return candidate;
            candidate = candidate.parent_path();
        }
    }

    return std::filesystem::current_path();
}

static std::string ExtractJsonString(const std::string& json, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1)
        return match[1].str();
    return {};
}

static int ExtractJsonInt(const std::string& json, const std::string& key, int fallback = -1)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1)
        return std::stoi(match[1].str());
    return fallback;
}

static bool ExtractJsonBool(const std::string& json, const std::string& key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*true");
    return std::regex_search(json, pattern);
}

static AppConfig LoadConfig()
{
    AppConfig config;
    auto path = g_AppRoot / "config" / "kittycore.json";
    std::string json = ReadTextFile(path);
    if (json.empty())
        json = ReadTextFile(g_AppRoot / "config" / "kittycore.example.json");

    std::string appVersion = ExtractJsonString(json, "app_version");
    std::string endpoint = ExtractJsonString(json, "auth_endpoint");
    std::string invite = ExtractJsonString(json, "discord_invite_url");
    std::string guild = ExtractJsonString(json, "guild_id");

    if (!appVersion.empty())
        config.appVersion = appVersion;
    if (!endpoint.empty())
        config.authEndpoint = endpoint;
    if (!invite.empty())
        config.discordInviteUrl = invite;
    if (!guild.empty())
        config.guildId = guild;
    return config;
}

static bool LoadTextureFromFile(const std::filesystem::path& path, ID3D11ShaderResourceView** outSrv, int* outWidth, int* outHeight)
{
    if (!std::filesystem::exists(path))
        return false;

    HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = SUCCEEDED(coHr);
    if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
        return false;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        if (shouldUninitialize)
            CoUninitialize();
        return false;
    }

    hr = factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr))
        hr = converter->GetSize(&width, &height);

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (SUCCEEDED(hr))
        hr = converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data());

    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA subResource{};
        subResource.pSysMem = pixels.data();
        subResource.SysMemPitch = width * 4u;

        ID3D11Texture2D* texture = nullptr;
        hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &texture);
        if (SUCCEEDED(hr)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, outSrv);
            texture->Release();
        }
    }

    if (converter)
        converter->Release();
    if (frame)
        frame->Release();
    if (decoder)
        decoder->Release();
    if (factory)
        factory->Release();

    if (SUCCEEDED(hr)) {
        *outWidth = static_cast<int>(width);
        *outHeight = static_cast<int>(height);
        if (shouldUninitialize)
            CoUninitialize();
        return true;
    }
    if (shouldUninitialize)
        CoUninitialize();
    return false;
}

static bool LoadTextureFromResource(int resourceId, ID3D11ShaderResourceView** outSrv, int* outWidth, int* outHeight)
{
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource)
        return false;

    HGLOBAL loaded = LoadResource(module, resource);
    DWORD resourceSize = SizeofResource(module, resource);
    void* resourceData = loaded ? LockResource(loaded) : nullptr;
    if (!resourceData || resourceSize == 0)
        return false;

    HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = SUCCEEDED(coHr);
    if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
        return false;

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr))
        hr = stream->InitializeFromMemory(static_cast<BYTE*>(resourceData), resourceSize);
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr))
        hr = converter->GetSize(&width, &height);

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (SUCCEEDED(hr))
        hr = converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data());

    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA subResource{};
        subResource.pSysMem = pixels.data();
        subResource.SysMemPitch = width * 4u;

        ID3D11Texture2D* texture = nullptr;
        hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &texture);
        if (SUCCEEDED(hr)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, outSrv);
            texture->Release();
        }
    }

    if (converter)
        converter->Release();
    if (frame)
        frame->Release();
    if (decoder)
        decoder->Release();
    if (stream)
        stream->Release();
    if (factory)
        factory->Release();

    if (SUCCEEDED(hr)) {
        *outWidth = static_cast<int>(width);
        *outHeight = static_cast<int>(height);
        if (shouldUninitialize)
            CoUninitialize();
        return true;
    }

    if (shouldUninitialize)
        CoUninitialize();
    return false;
}

static bool WriteResourceToFile(int resourceId, const std::filesystem::path& outputPath)
{
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource)
        return false;

    HGLOBAL loaded = LoadResource(module, resource);
    DWORD resourceSize = SizeofResource(module, resource);
    void* resourceData = loaded ? LockResource(loaded) : nullptr;
    if (!resourceData || resourceSize == 0)
        return false;

    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec)
        return false;

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;

    file.write(static_cast<const char*>(resourceData), resourceSize);
    return static_cast<bool>(file);
}

static std::filesystem::path ProgramDataPath()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    DWORD length = GetEnvironmentVariableW(L"ProgramData", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(std::wstring(buffer.data(), length));
    return std::filesystem::path(L"C:\\ProgramData");
}

static void RemoveFileQuietly(const std::filesystem::path& path)
{
    if (path.empty())
        return;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return;

    {
        std::ofstream wipe(path, std::ios::binary | std::ios::trunc);
        wipe << "EKAL OPTIMZER CODE PRIV\r\n";
    }
    SetFileAttributesW(path.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
    std::filesystem::remove(path, ec);
}

static void CleanupExtractedOptimizerScript()
{
    if (g_OptimizerProcess) {
        DWORD wait = WaitForSingleObject(g_OptimizerProcess, 0);
        if (wait == WAIT_TIMEOUT)
            return;
        CloseHandle(g_OptimizerProcess);
        g_OptimizerProcess = nullptr;
    }

    RemoveFileQuietly(g_ExtractedOptimizerScript);
    RemoveFileQuietly(ProgramDataPath() / "KittyCore" / "KittyCoreInit.ps1");
}

static std::string JsonEscape(const std::string& value)
{
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(ch));
            else
                out << ch;
            break;
        }
    }
    return out.str();
}

static std::string JsonStringArray(const std::vector<std::string>& values)
{
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            out << ",";
        out << "\"" << JsonEscape(values[i]) << "\"";
    }
    out << "]";
    return out.str();
}

static std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static std::string JoinStrings(const std::vector<std::string>& values, const std::string& separator)
{
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            out << separator;
        out << values[i];
    }
    return out.str();
}

static std::wstring RegistryString(HKEY root, const wchar_t* subkey, const wchar_t* value)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return L"";

    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, value, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(key);
        return L"";
    }

    std::wstring data(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, value, nullptr, nullptr, reinterpret_cast<LPBYTE>(data.data()), &bytes) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return L"";
    }

    RegCloseKey(key);
    while (!data.empty() && data.back() == L'\0')
        data.pop_back();
    return data;
}

static std::string Fnv1a64Hex(const std::wstring& value)
{
    uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) {
        hash ^= static_cast<uint16_t>(ch) & 0xffu;
        hash *= 1099511628211ull;
        hash ^= (static_cast<uint16_t>(ch) >> 8u) & 0xffu;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

static std::wstring VariantToString(VARIANT& value)
{
    if (value.vt == VT_BSTR && value.bstrVal)
        return value.bstrVal;
    if (value.vt == VT_I4)
        return std::to_wstring(value.lVal);
    if (value.vt == VT_UI4)
        return std::to_wstring(value.ulVal);
    if (value.vt == VT_I8)
        return std::to_wstring(value.llVal);
    if (value.vt == VT_UI8)
        return std::to_wstring(value.ullVal);
    return L"";
}

static std::wstring WmiQueryFirstString(const wchar_t* query, const wchar_t* property, const wchar_t* wmiNamespace = L"ROOT\\CIMV2")
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return L"";

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        if (shouldUninitialize)
            CoUninitialize();
        return L"";
    }

    IWbemLocator* locator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
    if (FAILED(hr)) {
        if (shouldUninitialize)
            CoUninitialize();
        return L"";
    }

    IWbemServices* services = nullptr;
    BSTR ns = SysAllocString(wmiNamespace);
    hr = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    SysFreeString(ns);
    if (FAILED(hr)) {
        locator->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return L"";
    }

    hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        services->Release();
        locator->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return L"";
    }

    IEnumWbemClassObject* enumerator = nullptr;
    BSTR language = SysAllocString(L"WQL");
    BSTR queryBstr = SysAllocString(query);
    hr = services->ExecQuery(language, queryBstr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(language);
    SysFreeString(queryBstr);
    if (FAILED(hr)) {
        services->Release();
        locator->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return L"";
    }

    IWbemClassObject* object = nullptr;
    ULONG returned = 0;
    std::wstring result;
    hr = enumerator->Next(3500, 1, &object, &returned);
    if (SUCCEEDED(hr) && returned && object) {
        VARIANT vt;
        VariantInit(&vt);
        if (SUCCEEDED(object->Get(property, 0, &vt, nullptr, nullptr)))
            result = VariantToString(vt);
        VariantClear(&vt);
        object->Release();
    }

    enumerator->Release();
    services->Release();
    locator->Release();
    if (shouldUninitialize)
        CoUninitialize();
    return result;
}

static std::vector<std::vector<std::wstring>> WmiQueryRows(
    const wchar_t* query,
    const std::vector<const wchar_t*>& properties,
    const wchar_t* wmiNamespace = L"ROOT\\CIMV2")
{
    std::vector<std::vector<std::wstring>> rows;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return rows;

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        if (shouldUninitialize)
            CoUninitialize();
        return rows;
    }

    IWbemLocator* locator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
    if (FAILED(hr)) {
        if (shouldUninitialize)
            CoUninitialize();
        return rows;
    }

    IWbemServices* services = nullptr;
    BSTR ns = SysAllocString(wmiNamespace);
    hr = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    SysFreeString(ns);
    if (FAILED(hr)) {
        locator->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return rows;
    }

    hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        services->Release();
        locator->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return rows;
    }

    IEnumWbemClassObject* enumerator = nullptr;
    BSTR language = SysAllocString(L"WQL");
    BSTR queryBstr = SysAllocString(query);
    hr = services->ExecQuery(language, queryBstr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(language);
    SysFreeString(queryBstr);
    if (SUCCEEDED(hr)) {
        for (;;) {
            IWbemClassObject* object = nullptr;
            ULONG returned = 0;
            hr = enumerator->Next(3500, 1, &object, &returned);
            if (FAILED(hr) || returned == 0 || !object)
                break;

            std::vector<std::wstring> row;
            row.reserve(properties.size());
            for (const wchar_t* property : properties) {
                VARIANT vt;
                VariantInit(&vt);
                std::wstring value;
                if (SUCCEEDED(object->Get(property, 0, &vt, nullptr, nullptr)))
                    value = VariantToString(vt);
                VariantClear(&vt);
                row.push_back(value);
            }
            rows.push_back(row);
            object->Release();
        }
        enumerator->Release();
    }

    services->Release();
    locator->Release();
    if (shouldUninitialize)
        CoUninitialize();
    return rows;
}

static std::string FormatBytes(uint64_t bytes)
{
    const double gb = static_cast<double>(bytes) / 1024.0 / 1024.0 / 1024.0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << gb << " GB";
    return out.str();
}

static std::string FormatBytesAdaptive(uint64_t bytes)
{
    std::ostringstream out;
    double mb = static_cast<double>(bytes) / 1024.0 / 1024.0;
    if (mb < 1024.0) {
        out << std::fixed << std::setprecision(0) << mb << " MB";
        return out.str();
    }

    out << std::fixed << std::setprecision(1) << (mb / 1024.0) << " GB";
    return out.str();
}

static uint64_t ParseUInt64(const std::wstring& value)
{
    try {
        if (value.empty())
            return 0;
        return static_cast<uint64_t>(std::stoull(value));
    }
    catch (...) {
        return 0;
    }
}

static std::string FormatGpuMemory(const std::wstring& adapterRam)
{
    uint64_t bytes = ParseUInt64(adapterRam);
    if (bytes == 0)
        return {};
    return FormatBytes(bytes) + " VRAM";
}

static bool LooksVirtualAdapter(const std::string& name)
{
    std::string lower = ToLowerAscii(name);
    return lower.find("virtual") != std::string::npos ||
        lower.find("parsec") != std::string::npos ||
        lower.find("remote") != std::string::npos ||
        lower.find("basic display") != std::string::npos;
}

static std::vector<std::string> QueryDxgiGpuList()
{
    std::vector<std::string> result;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
        return result;

    for (UINT index = 0;; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        HRESULT hr = factory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;
        if (FAILED(hr) || !adapter)
            continue;

        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            std::string name = WideToUtf8(desc.Description);
            if (!name.empty()) {
                std::vector<std::string> parts{ name };
                if (desc.DedicatedVideoMemory > 0)
                    parts.push_back(FormatBytes(static_cast<uint64_t>(desc.DedicatedVideoMemory)) + " dedicated");
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    parts.push_back("software");
                result.push_back(JoinStrings(parts, " | "));
            }
        }
        adapter->Release();
    }

    factory->Release();
    return result;
}

static bool GpuAlreadyListed(const std::vector<std::string>& gpus, const std::string& name)
{
    std::string lowerName = ToLowerAscii(name);
    for (const std::string& gpu : gpus) {
        if (ToLowerAscii(gpu).find(lowerName) != std::string::npos)
            return true;
    }
    return false;
}

static std::vector<std::string> QueryGpuList()
{
    std::vector<std::string> result = QueryDxgiGpuList();
    auto rows = WmiQueryRows(
        L"SELECT Name, AdapterRAM, DriverVersion FROM Win32_VideoController",
        { L"Name", L"AdapterRAM", L"DriverVersion" });

    for (const auto& row : rows) {
        if (row.empty() || row[0].empty())
            continue;

        std::string name = WideToUtf8(row[0]);
        std::vector<std::string> parts{ name };
        std::string vram = row.size() > 1 ? FormatGpuMemory(row[1]) : "";
        if (!vram.empty())
            parts.push_back(vram);
        if (row.size() > 2 && !row[2].empty())
            parts.push_back("driver " + WideToUtf8(row[2]));
        if (LooksVirtualAdapter(name))
            parts.push_back("virtual");
        if (!GpuAlreadyListed(result, name))
            result.push_back(JoinStrings(parts, " | "));
    }

    return result;
}

static std::vector<std::string> QueryStorageDevices()
{
    std::vector<std::string> result;
    auto rows = WmiQueryRows(
        L"SELECT Model, Size, Status FROM Win32_DiskDrive",
        { L"Model", L"Size", L"Status" });

    for (const auto& row : rows) {
        if (row.empty() || row[0].empty())
            continue;

        std::vector<std::string> parts{ WideToUtf8(row[0]) };
        if (row.size() > 1) {
            uint64_t bytes = ParseUInt64(row[1]);
            if (bytes > 0)
                parts.push_back(FormatBytes(bytes));
        }
        if (row.size() > 2 && !row[2].empty())
            parts.push_back("status " + WideToUtf8(row[2]));
        result.push_back(JoinStrings(parts, " | "));
    }

    return result;
}

static std::string SystemArchitecture()
{
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return "x64";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return "ARM64";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return "x86";
    default:
        return "Unknown";
    }
}

static std::vector<std::string> QueryDisplayList()
{
    std::vector<std::string> displays;
    for (DWORD index = 0;; ++index) {
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        if (!EnumDisplayDevicesW(nullptr, index, &device, 0))
            break;
        if ((device.StateFlags & DISPLAY_DEVICE_ACTIVE) == 0)
            continue;

        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsW(device.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
            continue;

        std::ostringstream display;
        display << WideToUtf8(device.DeviceString) << " | "
            << mode.dmPelsWidth << "x" << mode.dmPelsHeight;
        if (mode.dmDisplayFrequency > 1)
            display << " @ " << mode.dmDisplayFrequency << "Hz";
        if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            display << " | primary";
        displays.push_back(display.str());
    }
    return displays;
}

static std::string WindowsVersion()
{
    using RtlGetVersionPtr = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (!rtlGetVersion)
        return "Windows";

    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) != 0)
        return "Windows";

    std::ostringstream out;
    out << "Windows " << info.dwMajorVersion << "." << info.dwMinorVersion << " build " << info.dwBuildNumber;
    return out.str();
}

static std::string QueryAcpiTemperature()
{
    std::wstring raw = WmiQueryFirstString(
        L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature",
        L"CurrentTemperature",
        L"ROOT\\WMI");

    if (raw.empty())
        return "N/A";

    try {
        double kelvinTenths = std::stod(raw);
        double celsius = (kelvinTenths / 10.0) - 273.15;
        if (celsius < -20.0 || celsius > 150.0)
            return "N/A";

        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << celsius << " C";
        return out.str();
    }
    catch (...) {
        return "N/A";
    }
}

static std::string SystemDriveStorage(double* freePercent = nullptr)
{
    ULARGE_INTEGER freeAvailable{};
    ULARGE_INTEGER totalBytes{};
    ULARGE_INTEGER totalFree{};
    if (!GetDiskFreeSpaceExW(L"C:\\", &freeAvailable, &totalBytes, &totalFree)) {
        if (freePercent)
            *freePercent = 0.0;
        return "N/A";
    }

    if (freePercent) {
        *freePercent = totalBytes.QuadPart == 0 ? 0.0 :
            (static_cast<double>(totalFree.QuadPart) / static_cast<double>(totalBytes.QuadPart)) * 100.0;
    }

    std::ostringstream out;
    out << FormatBytes(totalBytes.QuadPart) << " total / " << FormatBytes(totalFree.QuadPart) << " free";
    return out.str();
}

static SystemInfo CollectSystemInfo()
{
    SystemInfo info;

    wchar_t computer[256]{};
    DWORD computerLen = static_cast<DWORD>(std::size(computer));
    if (GetComputerNameW(computer, &computerLen))
        info.computerName = WideToUtf8(computer);

    wchar_t user[256]{};
    DWORD userLen = static_cast<DWORD>(std::size(user));
    if (GetUserNameW(user, &userLen))
        info.windowsUser = WideToUtf8(user);

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        info.ram = FormatBytes(mem.ullTotalPhys);
        info.memoryLoadPercent = static_cast<int>(mem.dwMemoryLoad);
    }

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    info.cpuThreads = std::to_string(sysInfo.dwNumberOfProcessors) + " logical threads";
    info.architecture = SystemArchitecture();
    info.displays = QueryDisplayList();
    info.display = info.displays.empty() ? "N/A" : info.displays.front();
    info.storage = SystemDriveStorage(&info.storageFreePercent);
    info.storageDevices = QueryStorageDevices();
    info.temperature = QueryAcpiTemperature();
    info.os = WindowsVersion();

    std::wstring cpu = WmiQueryFirstString(L"SELECT Name FROM Win32_Processor", L"Name");
    std::wstring boardManufacturer = WmiQueryFirstString(L"SELECT Manufacturer FROM Win32_BaseBoard", L"Manufacturer");
    std::wstring boardProduct = WmiQueryFirstString(L"SELECT Product FROM Win32_BaseBoard", L"Product");
    std::wstring bios = WmiQueryFirstString(L"SELECT SMBIOSBIOSVersion FROM Win32_BIOS", L"SMBIOSBIOSVersion");

    info.cpu = cpu.empty() ? "Unknown CPU" : WideToUtf8(cpu);
    info.gpus = QueryGpuList();
    info.gpu = info.gpus.empty() ? "Unknown GPU" : JoinStrings(info.gpus, "; ");
    std::wstring board = boardManufacturer + L" " + boardProduct;
    if (board == L" ")
        info.motherboard = "Unknown motherboard";
    else
        info.motherboard = WideToUtf8(board);
    info.bios = bios.empty() ? "Unknown BIOS" : WideToUtf8(bios);

    std::wstring machineGuid = RegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");
    std::wstring hashInput = machineGuid + L"|" + Utf8ToWide(info.computerName);
    info.machineHash = Fnv1a64Hex(hashInput);
    return info;
}

struct HttpResult {
    bool ok = false;
    DWORD status = 0;
    std::string body;
    std::string error;
};

static HttpResult PostJson(const std::string& urlUtf8, const std::string& jsonBody)
{
    HttpResult result;
    std::wstring url = Utf8ToWide(urlUtf8);

    URL_COMPONENTSW parts{};
    std::array<wchar_t, 256> host{};
    std::array<wchar_t, 4096> path{};
    std::array<wchar_t, 2048> extra{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = static_cast<DWORD>(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = static_cast<DWORD>(path.size());
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        result.error = "Invalid auth endpoint URL.";
        return result;
    }

    std::wstring target(path.data(), parts.dwUrlPathLength);
    target += std::wstring(extra.data(), parts.dwExtraInfoLength);
    if (target.empty())
        target = L"/";

    HINTERNET session = WinHttpOpen(L"KittyCore/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.error = "WinHTTP session failed.";
        return result;
    }

    std::wstring hostName(host.data(), parts.dwHostNameLength);
    HINTERNET connect = WinHttpConnect(session, hostName.c_str(), parts.nPort, 0);
    if (!connect) {
        result.error = "WinHTTP connect failed.";
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", target.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        result.error = "WinHTTP request failed.";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD timeout = 10000;
    WinHttpSetTimeouts(request, timeout, timeout, timeout, timeout);

    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
        reinterpret_cast<LPVOID>(const_cast<char*>(jsonBody.data())),
        static_cast<DWORD>(jsonBody.size()), static_cast<DWORD>(jsonBody.size()), 0);
    if (sent)
        sent = WinHttpReceiveResponse(request, nullptr);

    if (!sent) {
        result.error = "HTTP request failed.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD statusSize = sizeof(result.status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &result.status, &statusSize, nullptr);

    DWORD available = 0;
    do {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
            break;
        if (available == 0)
            break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read))
            break;
        chunk.resize(read);
        result.body += chunk;
    } while (available > 0);

    result.ok = result.status >= 200 && result.status < 300;
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

static AuthResult VerifyAccess(AppConfig config, SystemInfo system, std::string discordId)
{
    AuthResult auth;
    discordId = Trim(discordId);

    if (!std::regex_match(discordId, std::regex("^\\d{17,20}$"))) {
        auth.message = "Invalid Discord ID. Use numeric user ID only.";
        return auth;
    }

    if (config.authEndpoint.empty()) {
        auth.allowed = true;
        auth.previewMode = true;
        auth.message = "Preview mode: auth_endpoint is empty. Configure backend for real Discord check.";
        auth.guildMemberCount = -1;
        auth.totalRuns = -1;
        auth.activeUsers = -1;
        return auth;
    }

    std::ostringstream body;
    body << "{";
    body << "\"discord_id\":\"" << JsonEscape(discordId) << "\",";
    body << "\"machine_hash\":\"" << JsonEscape(system.machineHash) << "\",";
    body << "\"app_version\":\"" << JsonEscape(config.appVersion) << "\",";
    body << "\"cpu\":\"" << JsonEscape(system.cpu) << "\",";
    body << "\"cpu_threads\":\"" << JsonEscape(system.cpuThreads) << "\",";
    body << "\"architecture\":\"" << JsonEscape(system.architecture) << "\",";
    body << "\"gpu\":\"" << JsonEscape(system.gpu) << "\",";
    body << "\"gpus\":" << JsonStringArray(system.gpus) << ",";
    body << "\"motherboard\":\"" << JsonEscape(system.motherboard) << "\",";
    body << "\"bios\":\"" << JsonEscape(system.bios) << "\",";
    body << "\"ram\":\"" << JsonEscape(system.ram) << "\",";
    body << "\"memory_load_percent\":" << system.memoryLoadPercent << ",";
    body << "\"storage\":\"" << JsonEscape(system.storage) << "\",";
    body << "\"storage_free_percent\":" << std::fixed << std::setprecision(1) << system.storageFreePercent << ",";
    body << "\"storage_devices\":" << JsonStringArray(system.storageDevices) << ",";
    body << "\"display\":\"" << JsonEscape(system.display) << "\",";
    body << "\"displays\":" << JsonStringArray(system.displays) << ",";
    body << "\"temperature\":\"" << JsonEscape(system.temperature) << "\",";
    body << "\"os\":\"" << JsonEscape(system.os) << "\",";
    body << "\"computer_name\":\"" << JsonEscape(system.computerName) << "\",";
    body << "\"windows_user\":\"" << JsonEscape(system.windowsUser) << "\"";
    body << "}";

    HttpResult response = PostJson(config.authEndpoint, body.str());
    auth.httpStatus = static_cast<int>(response.status);
    if (!response.error.empty()) {
        auth.message = response.error;
        return auth;
    }

    auth.allowed = ExtractJsonBool(response.body, "allowed");
    auth.message = ExtractJsonString(response.body, "message");
    auth.guildMemberCount = ExtractJsonInt(response.body, "guild_member_count");
    auth.totalRuns = ExtractJsonInt(response.body, "total_runs");
    auth.activeUsers = ExtractJsonInt(response.body, "active_users");

    if (auth.message.empty())
        auth.message = auth.allowed ? "Access granted." : "Access denied.";
    return auth;
}

static std::string CompactAuthStatus(const AuthResult& auth)
{
    if (auth.allowed)
        return "OK";

    std::string msg = auth.message;
    std::transform(msg.begin(), msg.end(), msg.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (msg.find("DISCORD_BOT_TOKEN") != std::string::npos)
        return "SEM TOKEN";
    if (msg.find("HTTP REQUEST FAILED") != std::string::npos || msg.find("WINHTTP") != std::string::npos)
        return "SERVIDOR OFF";
    if (msg.find("INVALID_ID") != std::string::npos || msg.find("INVALID DISCORD ID") != std::string::npos)
        return "ID INVALIDO";
    if (msg.find("TOKEN_INVALIDO") != std::string::npos)
        return "TOKEN INVALIDO";
    if (msg.find("BOT_SEM_ACESSO") != std::string::npos)
        return "BOT SEM ACESSO";
    if (msg.find("NAO_MEMBRO") != std::string::npos)
        return "NAO MEMBRO";
    if (msg.find("DISCORD_") != std::string::npos)
        return msg.substr(0, 20);

    return msg.empty() ? "NEGADO" : msg.substr(0, 20);
}

static void UpdateSpecSnapshot(const SystemInfo& info)
{
    std::lock_guard<std::mutex> lock(g_SpecMutex);
    g_SpecInfo = info;
}

static SystemInfo CurrentSpecSnapshot()
{
    std::lock_guard<std::mutex> lock(g_SpecMutex);
    return g_SpecInfo;
}

static ULONGLONG FileTimeToUInt64(const FILETIME& value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

static bool NameMatches(const std::string& lowerName, const std::vector<const char*>& names)
{
    return std::find(names.begin(), names.end(), lowerName) != names.end();
}

static bool IsBrowserProcessName(const std::string& processName)
{
    static const std::vector<const char*> browsers = {
        "chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "opera.exe",
        "opera_gx.exe", "vivaldi.exe", "iexplore.exe", "arc.exe"
    };
    return NameMatches(ToLowerAscii(processName), browsers);
}

static bool IsCriticalProcessName(const std::string& processName)
{
    static const std::vector<const char*> critical = {
        "system", "registry", "smss.exe", "csrss.exe", "wininit.exe", "winlogon.exe",
        "services.exe", "lsass.exe", "svchost.exe", "fontdrvhost.exe", "dwm.exe",
        "explorer.exe", "sihost.exe", "searchhost.exe", "startmenuexperiencehost.exe",
        "securityhealthsystray.exe", "runtimebroker.exe", "taskmgr.exe", "powershell.exe",
        "pwsh.exe", "cmd.exe", "conhost.exe", "kittycore.exe", "codex.exe"
    };
    return NameMatches(ToLowerAscii(processName), critical);
}

static bool IsCloseWhitelistedProcessName(const std::string& processName)
{
    static const std::vector<const char*> safeApps = {
        "discord.exe", "discordptb.exe", "discordcanary.exe", "telegram.exe",
        "whatsapp.exe", "slack.exe", "teams.exe", "spotify.exe", "steam.exe",
        "steamwebhelper.exe", "epicgameslauncher.exe", "eadesktop.exe", "battle.net.exe",
        "obs64.exe", "obs32.exe", "medal.exe", "overwolf.exe"
    };
    return NameMatches(ToLowerAscii(processName), safeApps);
}

static bool CanCloseProcess(DWORD pid, const std::string& processName)
{
    if (pid == 0 || pid == 4 || pid == GetCurrentProcessId())
        return false;
    if (IsBrowserProcessName(processName) || IsCriticalProcessName(processName))
        return false;
    return IsCloseWhitelistedProcessName(processName);
}

static int CountProcesses()
{
    int count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return count;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            ++count;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

static std::vector<ProcessInfo> EnumerateProcesses()
{
    std::vector<ProcessInfo> processes;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return processes;

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    double cpuCount = std::max<DWORD>(1, sysInfo.dwNumberOfProcessors);
    auto now = std::chrono::steady_clock::now();

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            ProcessInfo info;
            info.pid = entry.th32ProcessID;
            info.name = WideToUtf8(entry.szExeFile);
            info.isBrowser = IsBrowserProcessName(info.name);
            info.canClose = CanCloseProcess(info.pid, info.name);

            ULONGLONG processCpu100ns = 0;
            uint64_t readBytes = 0;
            uint64_t writeBytes = 0;

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
            if (process) {
                std::array<wchar_t, 4096> path{};
                DWORD pathLen = static_cast<DWORD>(path.size());
                if (QueryFullProcessImageNameW(process, 0, path.data(), &pathLen))
                    info.path = WideToUtf8(std::wstring(path.data(), pathLen));

                PROCESS_MEMORY_COUNTERS_EX memory{};
                if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
                    info.memoryBytes = static_cast<uint64_t>(memory.WorkingSetSize);

                FILETIME created{}, exited{}, kernel{}, user{};
                if (GetProcessTimes(process, &created, &exited, &kernel, &user))
                    processCpu100ns = FileTimeToUInt64(kernel) + FileTimeToUInt64(user);

                IO_COUNTERS io{};
                if (GetProcessIoCounters(process, &io)) {
                    readBytes = io.ReadTransferCount;
                    writeBytes = io.WriteTransferCount;
                }

                CloseHandle(process);
            }

            {
                std::lock_guard<std::mutex> lock(g_ProcessSampleMutex);
                auto previous = g_ProcessSamples.find(info.pid);
                if (previous != g_ProcessSamples.end()) {
                    double elapsed = std::chrono::duration<double>(now - previous->second.at).count();
                    if (elapsed > 0.10) {
                        ULONGLONG cpuDelta = processCpu100ns > previous->second.cpuTime100ns ?
                            processCpu100ns - previous->second.cpuTime100ns : 0;
                        info.cpuPercent = std::clamp((static_cast<double>(cpuDelta) / (elapsed * 10000000.0 * cpuCount)) * 100.0, 0.0, 100.0);

                        uint64_t previousIo = previous->second.readBytes + previous->second.writeBytes;
                        uint64_t currentIo = readBytes + writeBytes;
                        uint64_t ioDelta = currentIo > previousIo ? currentIo - previousIo : 0;
                        info.ioMbps = static_cast<double>(ioDelta) / elapsed / 1024.0 / 1024.0;
                    }
                }
                g_ProcessSamples[info.pid] = { processCpu100ns, readBytes, writeBytes, now };
            }

            double memoryMb = static_cast<double>(info.memoryBytes) / 1024.0 / 1024.0;
            int score = static_cast<int>(std::round(info.cpuPercent * 1.6 + memoryMb / 35.0 + info.ioMbps * 8.0));
            info.impactScore = std::clamp(score, 0, 100);
            if (info.impactScore >= 65)
                info.impactLabel = "ALTO";
            else if (info.impactScore >= 30)
                info.impactLabel = "MEDIO";

            processes.push_back(info);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        if (a.impactScore != b.impactScore)
            return a.impactScore > b.impactScore;
        return a.memoryBytes > b.memoryBytes;
    });
    if (processes.size() > 120)
        processes.resize(120);
    return processes;
}

static std::string BuildProcessesJson()
{
    auto processes = EnumerateProcesses();
    std::ostringstream json;
    json << "{\"processes\":[";
    for (size_t i = 0; i < processes.size(); ++i) {
        const ProcessInfo& process = processes[i];
        if (i)
            json << ",";
        json << "{";
        json << "\"pid\":" << process.pid << ",";
        json << "\"name\":\"" << JsonEscape(process.name) << "\",";
        json << "\"path\":\"" << JsonEscape(process.path) << "\",";
        json << "\"cpu\":" << std::fixed << std::setprecision(1) << process.cpuPercent << ",";
        json << "\"memory_bytes\":" << process.memoryBytes << ",";
        json << "\"memory\":\"" << JsonEscape(FormatBytesAdaptive(process.memoryBytes)) << "\",";
        json << "\"io_mbps\":" << std::fixed << std::setprecision(2) << process.ioMbps << ",";
        json << "\"impact_score\":" << process.impactScore << ",";
        json << "\"impact\":\"" << JsonEscape(process.impactLabel) << "\",";
        json << "\"can_close\":" << (process.canClose ? "true" : "false") << ",";
        json << "\"browser\":" << (process.isBrowser ? "true" : "false");
        json << "}";
    }
    json << "]}";
    return json.str();
}

static double ParseTemperatureCelsius(const std::string& temperature)
{
    try {
        size_t consumed = 0;
        double value = std::stod(temperature, &consumed);
        return consumed > 0 ? value : -1000.0;
    }
    catch (...) {
        return -1000.0;
    }
}

static std::string BuildHealthJson()
{
    SystemInfo info = CurrentSpecSnapshot();

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    int memoryLoad = info.memoryLoadPercent;
    if (GlobalMemoryStatusEx(&mem))
        memoryLoad = static_cast<int>(mem.dwMemoryLoad);

    double storageFreePercent = 0.0;
    std::string driveText = SystemDriveStorage(&storageFreePercent);
    int processCount = CountProcesses();
    double tempC = ParseTemperatureCelsius(info.temperature);

    int score = 100;
    std::vector<std::string> notes;
    if (memoryLoad >= 88) {
        score -= 22;
        notes.push_back("RAM em uso alto");
    }
    else if (memoryLoad >= 72) {
        score -= 10;
        notes.push_back("RAM em uso moderado");
    }

    if (storageFreePercent > 0.0 && storageFreePercent < 10.0) {
        score -= 24;
        notes.push_back("Pouco espaco livre no disco do sistema");
    }
    else if (storageFreePercent > 0.0 && storageFreePercent < 20.0) {
        score -= 12;
        notes.push_back("Disco do sistema chegando no limite");
    }

    if (tempC > 85.0) {
        score -= 25;
        notes.push_back("Temperatura alta");
    }
    else if (tempC > 75.0) {
        score -= 12;
        notes.push_back("Temperatura elevada");
    }

    if (processCount > 260) {
        score -= 8;
        notes.push_back("Muitos processos ativos");
    }

    score = std::clamp(score, 0, 100);
    std::string status = score >= 85 ? "EXCELENTE" : score >= 70 ? "BOM" : score >= 50 ? "ATENCAO" : "CRITICO";
    if (notes.empty())
        notes.push_back("Sem alerta forte no snapshot atual");

    std::ostringstream json;
    json << "{";
    json << "\"score\":" << score << ",";
    json << "\"status\":\"" << JsonEscape(status) << "\",";
    json << "\"memory_load\":" << memoryLoad << ",";
    json << "\"storage\":\"" << JsonEscape(driveText) << "\",";
    json << "\"storage_free_percent\":" << std::fixed << std::setprecision(1) << storageFreePercent << ",";
    json << "\"temperature\":\"" << JsonEscape(info.temperature) << "\",";
    json << "\"process_count\":" << processCount << ",";
    json << "\"notes\":" << JsonStringArray(notes);
    json << "}";
    return json.str();
}

static std::string BuildSpecJson()
{
    SystemInfo info = CurrentSpecSnapshot();
    std::ostringstream json;
    json << "{";
    json << "\"app\":\"KittyCore\",";
    json << "\"version\":\"1.0.0-beta\",";
    json << "\"cpu\":\"" << JsonEscape(info.cpu) << "\",";
    json << "\"cpu_threads\":\"" << JsonEscape(info.cpuThreads) << "\",";
    json << "\"architecture\":\"" << JsonEscape(info.architecture) << "\",";
    json << "\"gpu\":\"" << JsonEscape(info.gpu) << "\",";
    json << "\"gpus\":" << JsonStringArray(info.gpus) << ",";
    json << "\"motherboard\":\"" << JsonEscape(info.motherboard) << "\",";
    json << "\"bios\":\"" << JsonEscape(info.bios) << "\",";
    json << "\"ram\":\"" << JsonEscape(info.ram) << "\",";
    json << "\"memory_load_percent\":" << info.memoryLoadPercent << ",";
    json << "\"storage\":\"" << JsonEscape(info.storage) << "\",";
    json << "\"storage_free_percent\":" << std::fixed << std::setprecision(1) << info.storageFreePercent << ",";
    json << "\"storage_devices\":" << JsonStringArray(info.storageDevices) << ",";
    json << "\"display\":\"" << JsonEscape(info.display) << "\",";
    json << "\"displays\":" << JsonStringArray(info.displays) << ",";
    json << "\"temperature\":\"" << JsonEscape(info.temperature) << "\",";
    json << "\"os\":\"" << JsonEscape(info.os) << "\",";
    json << "\"computer_name\":\"" << JsonEscape(info.computerName) << "\",";
    json << "\"windows_user\":\"" << JsonEscape(info.windowsUser) << "\",";
    json << "\"machine_hash\":\"" << JsonEscape(info.machineHash) << "\"";
    json << "}";
    return json.str();
}

static const char* SpecPageHtml()
{
    return R"HTML(<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>KittyCore PC</title>
  <style>
    :root{color-scheme:dark;--bg:#101012;--panel:#18181c;--panel2:#202026;--soft:#26262d;--line:#383840;--text:#f0f0f2;--muted:#a3a3ad;--pink:#e85f90;--green:#82d19a;--yellow:#e5bd5d;--red:#e06f6f}
    *{box-sizing:border-box}body{margin:0;background:linear-gradient(180deg,#151519,#0f0f11 52%,#0c0c0e);font:13px/1.42 Segoe UI,Arial,sans-serif;color:var(--text)}
    main{width:min(1180px,calc(100vw - 28px));margin:22px auto 28px}
    header{display:flex;justify-content:space-between;gap:16px;align-items:flex-end;border-bottom:1px solid var(--line);padding:0 0 14px;margin-bottom:14px}
    h1{margin:0;font-size:24px;font-weight:900;letter-spacing:.08em}.sub,.label{color:var(--muted);font-size:11px;font-weight:800;letter-spacing:.12em;text-transform:uppercase}.top{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
    .pill{border:1px solid var(--line);background:#17171b;border-radius:6px;padding:7px 10px;font-weight:800;color:#ddd}.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:10px}
    .card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);border-radius:8px;padding:13px;min-height:84px}.span3{grid-column:span 3}.span4{grid-column:span 4}.span5{grid-column:span 5}.span6{grid-column:span 6}.span7{grid-column:span 7}.span8{grid-column:span 8}.span12{grid-column:span 12}
    .value{font-size:17px;font-weight:800;word-break:break-word;margin-top:7px}.small{font-size:12px;color:#d5d5da;word-break:break-word}.list{display:grid;gap:8px;margin-top:8px}.item{border:1px solid #303038;background:#16161a;border-radius:6px;padding:9px 10px;font-weight:700}.muted{color:var(--muted)}
    .score{display:grid;grid-template-columns:88px 1fr;gap:12px;align-items:center}.ring{width:84px;height:84px;border-radius:50%;display:grid;place-items:center;background:conic-gradient(var(--pink) calc(var(--score)*1%),#303038 0);position:relative}.ring:after{content:"";position:absolute;inset:8px;background:#18181c;border-radius:50%}.ring b{position:relative;z-index:1;font-size:22px}
    .bar{height:8px;border-radius:999px;background:#101014;border:1px solid #303038;overflow:hidden;margin-top:10px}.bar i{display:block;height:100%;width:0;background:linear-gradient(90deg,var(--green),var(--yellow),var(--pink))}
    table{width:100%;border-collapse:collapse;margin-top:9px}th,td{padding:9px 8px;border-bottom:1px solid #303038;text-align:left;vertical-align:middle}th{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.1em}td{font-weight:650}.num{text-align:right;font-variant-numeric:tabular-nums}.tag{display:inline-flex;border:1px solid #3c3c45;border-radius:999px;padding:3px 7px;font-size:11px;font-weight:900;color:#ddd}.tag.ok{border-color:#355c40;color:var(--green)}.tag.warn{border-color:#695932;color:var(--yellow)}.tag.hot{border-color:#6b373d;color:var(--red)}
    button{height:26px;border:1px solid #484850;background:#24242a;color:#f3f3f4;border-radius:6px;font-weight:900;font-size:11px;letter-spacing:.08em;cursor:pointer}button:hover{border-color:#777780;background:#2b2b32}button:disabled{opacity:.38;cursor:not-allowed}.toast{min-height:18px;color:var(--muted);font-weight:800;text-align:right;margin-top:8px}
    footer{color:var(--muted);font-size:12px;text-align:right;margin-top:12px}@media(max-width:860px){.span3,.span4,.span5,.span6,.span7,.span8{grid-column:span 12}header{align-items:flex-start;flex-direction:column}.top{justify-content:flex-start}.score{grid-template-columns:76px 1fr}.ring{width:72px;height:72px}th:nth-child(5),td:nth-child(5),th:nth-child(2),td:nth-child(2){display:none}}
  </style>
</head>
<body>
<main>
  <header>
    <div><div class="sub">local diagnostics</div><h1>KITTYCORE PC</h1></div>
    <div class="top">
      <div class="pill" id="temp">TEMP: N/A</div>
      <div class="pill" id="procCount">PROCESSOS: --</div>
      <div class="pill">127.0.0.1:7777</div>
    </div>
  </header>
  <section class="grid">
    <div class="card span5">
      <div class="label">estado geral</div>
      <div class="score"><div class="ring" id="ring"><b id="score">--</b></div><div><div class="value" id="status">ANALISANDO</div><div class="small" id="notes">...</div></div></div>
    </div>
    <div class="card span4"><div class="label">memoria</div><div class="value" id="ram">...</div><div class="bar"><i id="memBar"></i></div><div class="small" id="memLoad">...</div></div>
    <div class="card span3"><div class="label">armazenamento c:</div><div class="value" id="storage">...</div><div class="bar"><i id="diskBar"></i></div></div>
    <div class="card span8"><div class="label">processador</div><div class="value" id="cpu">...</div><div class="small" id="threads"></div></div>
    <div class="card span4"><div class="label">sistema</div><div class="value" id="os">...</div><div class="small" id="pc"></div></div>
    <div class="card span6"><div class="label">graphics</div><div class="list" id="gpuList"><div class="item">...</div></div></div>
    <div class="card span6"><div class="label">discos</div><div class="list" id="diskList"><div class="item">...</div></div></div>
    <div class="card span6"><div class="label">motherboard</div><div class="value" id="board">...</div><div class="small" id="bios"></div></div>
    <div class="card span3"><div class="label">usuario</div><div class="value" id="winUser">...</div></div>
    <div class="card span3"><div class="label">machine hash</div><div class="value small" id="hash">...</div></div>
    <div class="card span12">
      <div class="label">processos ativos</div>
      <table>
        <thead><tr><th>exe</th><th>pid</th><th class="num">cpu</th><th class="num">ram</th><th class="num">io/s</th><th>impacto</th><th class="num">acao</th></tr></thead>
        <tbody id="processRows"><tr><td colspan="7" class="muted">...</td></tr></tbody>
      </table>
      <div class="toast" id="toast"></div>
    </div>
  </section>
  <footer>KITTYCORE V1.0.0 BETA</footer>
</main>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function json(url,opt){const r=await fetch(url,{cache:'no-store',...(opt||{})});return r.json()}
function list(id,items){$(id).innerHTML=(items&&items.length?items:['N/A']).map(x=>`<div class="item">${esc(x)}</div>`).join('')}
function clsImpact(x){return x==='ALTO'?'hot':x==='MEDIO'?'warn':'ok'}
async function loadSystem(){
  const d=await json('/api/system');
  $('cpu').textContent=d.cpu;$('threads').textContent=d.cpu_threads;$('ram').textContent=d.ram;$('storage').textContent=d.storage;
  $('board').textContent=d.motherboard;$('bios').textContent='BIOS '+d.bios;$('os').textContent=d.os;$('pc').textContent=d.computer_name;
  $('winUser').textContent=d.windows_user;$('hash').textContent=d.machine_hash;$('temp').textContent='TEMP: '+(d.temperature||'N/A');
  list('gpuList',d.gpus&&d.gpus.length?d.gpus:[d.gpu]);list('diskList',d.storage_devices);
}
async function loadHealth(){
  const h=await json('/api/health');const score=Math.max(0,Math.min(100,h.score||0));
  $('ring').style.setProperty('--score',score);$('score').textContent=score;$('status').textContent=h.status||'ANALISANDO';
  $('notes').textContent=(h.notes||[]).join(' / ');$('memLoad').textContent=(h.memory_load??0)+'% em uso';
  $('memBar').style.width=(h.memory_load??0)+'%';$('diskBar').style.width=(100-(h.storage_free_percent??0))+'%';
  $('procCount').textContent='PROCESSOS: '+(h.process_count??'--');$('temp').textContent='TEMP: '+(h.temperature||'N/A');
}
async function closeProc(pid){
  $('toast').textContent='FECHANDO '+pid+'...';
  const d=await json('/api/process/close?pid='+encodeURIComponent(pid),{method:'POST',headers:{'X-KittyCore':'1'}});
  $('toast').textContent=(d.name?d.name+' - ':'')+(d.message||'OK');
  setTimeout(loadProcesses,500);
}
async function loadProcesses(){
  const d=await json('/api/processes');const rows=(d.processes||[]).map(p=>{
    const action=p.can_close?`<button onclick="closeProc(${p.pid})">FECHAR</button>`:(p.browser?'<span class="muted">NAVEGADOR</span>':'<span class="muted">-</span>');
    return `<tr><td>${esc(p.name)}</td><td>${p.pid}</td><td class="num">${Number(p.cpu).toFixed(1)}%</td><td class="num">${esc(p.memory)}</td><td class="num">${Number(p.io_mbps).toFixed(2)}</td><td><span class="tag ${clsImpact(p.impact)}">${esc(p.impact)} ${p.impact_score}</span></td><td class="num">${action}</td></tr>`;
  }).join('');
  $('processRows').innerHTML=rows||'<tr><td colspan="7" class="muted">N/A</td></tr>';
}
async function loadAll(){
  try{await Promise.all([loadSystem(),loadHealth(),loadProcesses()])}catch(e){$('toast').textContent='ERRO AO ATUALIZAR'}
}
loadAll();setInterval(loadAll,3000);
</script>
</body>
</html>)HTML";
}

static std::string ProcessNameByPid(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return {};

    std::string name;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                name = WideToUtf8(entry.szExeFile);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return name;
}

static bool IsProcessRunning(DWORD pid)
{
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process)
        return false;
    DWORD wait = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return wait == WAIT_TIMEOUT;
}

struct CloseWindowContext {
    DWORD pid = 0;
    int sent = 0;
};

static BOOL CALLBACK CloseWindowProc(HWND hwnd, LPARAM param)
{
    auto* context = reinterpret_cast<CloseWindowContext*>(param);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != context->pid)
        return TRUE;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
        return TRUE;

    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    ++context->sent;
    return TRUE;
}

static std::string CloseProcessJson(DWORD pid)
{
    std::string name = ProcessNameByPid(pid);
    if (name.empty())
        return "{\"ok\":false,\"message\":\"PROCESSO_NAO_ENCONTRADO\"}";
    if (!CanCloseProcess(pid, name))
        return "{\"ok\":false,\"message\":\"BLOQUEADO\",\"name\":\"" + JsonEscape(name) + "\"}";

    CloseWindowContext context;
    context.pid = pid;
    EnumWindows(CloseWindowProc, reinterpret_cast<LPARAM>(&context));

    if (context.sent > 0) {
        Sleep(900);
        if (!IsProcessRunning(pid)) {
            return "{\"ok\":true,\"message\":\"FECHADO\",\"name\":\"" + JsonEscape(name) + "\"}";
        }
    }

    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!process)
        return "{\"ok\":false,\"message\":\"SEM_PERMISSAO\",\"name\":\"" + JsonEscape(name) + "\"}";

    bool terminated = TerminateProcess(process, 0) != 0;
    if (terminated)
        WaitForSingleObject(process, 1200);
    CloseHandle(process);

    return std::string("{\"ok\":") + (terminated ? "true" : "false") +
        ",\"message\":\"" + (terminated ? "FECHADO_FORCADO" : "FALHA") +
        "\",\"name\":\"" + JsonEscape(name) + "\"}";
}

static std::string RequestTarget(const std::string& request)
{
    size_t firstSpace = request.find(' ');
    if (firstSpace == std::string::npos)
        return "/";
    size_t secondSpace = request.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos)
        return "/";
    return request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
}

static DWORD PidFromTarget(const std::string& target)
{
    size_t marker = target.find("pid=");
    if (marker == std::string::npos)
        return 0;
    marker += 4;
    size_t end = marker;
    while (end < target.size() && std::isdigit(static_cast<unsigned char>(target[end])))
        ++end;
    if (end == marker)
        return 0;

    try {
        return static_cast<DWORD>(std::stoul(target.substr(marker, end - marker)));
    }
    catch (...) {
        return 0;
    }
}

static bool SendAll(SOCKET client, const std::string& data)
{
    const char* ptr = data.data();
    int remaining = static_cast<int>(data.size());
    while (remaining > 0) {
        int sent = send(client, ptr, remaining, 0);
        if (sent <= 0)
            return false;
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

static void SendHttp(SOCKET client, const std::string& contentType, const std::string& body, const char* status = "200 OK")
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "Cache-Control: no-store\r\n\r\n";
    response << body;
    SendAll(client, response.str());
}

static void HandleSpecClient(SOCKET client)
{
    char buffer[2048]{};
    int received = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
    if (received <= 0) {
        closesocket(client);
        return;
    }

    std::string request(buffer, received);
    std::string target = RequestTarget(request);
    if (request.rfind("GET /api/system", 0) == 0) {
        SendHttp(client, "application/json; charset=utf-8", BuildSpecJson());
    }
    else if (request.rfind("GET /api/health", 0) == 0) {
        SendHttp(client, "application/json; charset=utf-8", BuildHealthJson());
    }
    else if (request.rfind("GET /api/processes", 0) == 0) {
        SendHttp(client, "application/json; charset=utf-8", BuildProcessesJson());
    }
    else if (request.rfind("POST /api/process/close", 0) == 0) {
        if (request.find("\r\nX-KittyCore: 1\r\n") == std::string::npos) {
            SendHttp(client, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"HEADER_INVALIDO\"}", "403 Forbidden");
            closesocket(client);
            return;
        }
        DWORD pid = PidFromTarget(target);
        if (pid == 0)
            SendHttp(client, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"PID_INVALIDO\"}", "400 Bad Request");
        else
            SendHttp(client, "application/json; charset=utf-8", CloseProcessJson(pid));
    }
    else if (request.rfind("GET /", 0) == 0) {
        SendHttp(client, "text/html; charset=utf-8", SpecPageHtml());
    }
    else {
        SendHttp(client, "text/plain; charset=utf-8", "Not found", "404 Not Found");
    }
    closesocket(client);
}

static void SpecServerLoop()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        g_SpecServerRunning = false;
        return;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        g_SpecServerRunning = false;
        WSACleanup();
        return;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7777);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listener);
        g_SpecServerRunning = false;
        WSACleanup();
        return;
    }

    g_SpecServerSocket = listener;
    while (g_SpecServerRunning) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            break;
        HandleSpecClient(client);
    }

    if (g_SpecServerSocket != INVALID_SOCKET) {
        closesocket(g_SpecServerSocket);
        g_SpecServerSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

static void StartSpecServer(const SystemInfo& info)
{
    UpdateSpecSnapshot(info);
    if (g_SpecServerRunning.exchange(true))
        return;
    g_SpecServerThread = std::thread(SpecServerLoop);
}

static void StopSpecServer()
{
    g_SpecServerRunning = false;
    if (g_SpecServerSocket != INVALID_SOCKET) {
        closesocket(g_SpecServerSocket);
        g_SpecServerSocket = INVALID_SOCKET;
    }
    if (g_SpecServerThread.joinable())
        g_SpecServerThread.join();
}

static bool LaunchOptimizerScript(std::string& status)
{
    CleanupExtractedOptimizerScript();

    std::ostringstream name;
    name << "kc-" << GetCurrentProcessId() << "-" << GetTickCount64() << ".ps1";
    std::filesystem::path script = ProgramDataPath() / "KittyCore" / name.str();
    if (!WriteResourceToFile(IDR_INIT_SCRIPT, script)) {
        status = "ERRO";
        return false;
    }
    g_ExtractedOptimizerScript = script;
    SetFileAttributesW(script.wstring().c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);

    std::wstring params = L"-NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() + L"\"";
    SHELLEXECUTEINFOW exec{};
    exec.cbSize = sizeof(exec);
    exec.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec.lpVerb = L"runas";
    exec.lpFile = L"powershell.exe";
    exec.lpParameters = params.c_str();
    exec.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&exec)) {
        status = "BLOQUEADO";
        RemoveFileQuietly(script);
        return false;
    }

    HANDLE monitorProcess = nullptr;
    DuplicateHandle(GetCurrentProcess(), exec.hProcess, GetCurrentProcess(), &monitorProcess, SYNCHRONIZE, FALSE, 0);
    if (g_OptimizerProcess)
        CloseHandle(g_OptimizerProcess);
    g_OptimizerProcess = exec.hProcess;

    if (monitorProcess) {
        std::thread([monitorProcess, script]() {
            WaitForSingleObject(monitorProcess, INFINITE);
            CloseHandle(monitorProcess);
            Sleep(900);
            RemoveFileQuietly(script);
        }).detach();
    }

    status = "ABERTO";
    return true;
}

static ImU32 ColorU32(int r, int g, int b, int a = 255)
{
    return IM_COL32(r, g, b, a);
}

static void ApplyKittyStyle(float scale)
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f * scale;
    style.FrameRounding = 4.0f * scale;
    style.PopupRounding = 4.0f * scale;
    style.ScrollbarRounding = 4.0f * scale;
    style.GrabRounding = 4.0f * scale;
    style.TabRounding = 4.0f * scale;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(7.0f * scale, 6.0f * scale);
    style.WindowPadding = ImVec2(12.0f * scale, 10.0f * scale);
    style.FramePadding = ImVec2(8.0f * scale, 5.0f * scale);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.86f, 0.86f, 0.88f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.055f, 0.065f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.075f, 0.088f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.125f, 0.125f, 0.145f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.155f, 0.155f, 0.180f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.180f, 0.180f, 0.205f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.145f, 0.145f, 0.165f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.190f, 0.190f, 0.215f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.235f, 0.235f, 0.260f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.76f, 0.76f, 0.80f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.075f, 0.075f, 0.088f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
}

static void DrawBackground(ImDrawList* draw, ImVec2 pos, ImVec2 size, bool phantom)
{
    ImU32 base = ColorU32(14, 14, 17);
    ImU32 edge = phantom ? ColorU32(96, 88, 112, 70) : ColorU32(78, 78, 86, 70);
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), base);
    draw->AddRect(ImVec2(pos.x + 0.5f, pos.y + 0.5f), ImVec2(pos.x + size.x - 0.5f, pos.y + size.y - 0.5f), edge, 0.0f, 0, 1.0f);
}

static void DrawLogoImage(ImVec2 size, float alpha = 0.68f)
{
    if (!g_LogoTexture) {
        ImGui::Dummy(size);
        return;
    }

    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > size.x)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - size.x) * 0.5f);

    ImGui::ImageWithBg(reinterpret_cast<ImTextureID>(g_LogoTexture), size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1.0f, 1.0f, 1.0f, alpha));
}

static void DrawKittyCoreLogo(ImDrawList* draw, ImVec2 center, float size, bool phantom)
{
    float s = size;
    ImU32 ghost = phantom ? ColorU32(122, 53, 255, 210) : ColorU32(122, 53, 255, 145);
    ImU32 ghostDark = ColorU32(53, 23, 94, 210);
    ImU32 cat = ColorU32(255, 247, 251, 255);
    ImU32 catEdge = ColorU32(255, 122, 191, 255);
    ImU32 bow = ColorU32(255, 122, 191, 255);
    ImU32 eye = ColorU32(17, 16, 24, 255);
    ImU32 mint = ColorU32(67, 255, 176, 255);

    ImVec2 ghostCenter(center.x + s * 0.20f, center.y + s * 0.02f);
    draw->AddCircleFilled(ghostCenter, s * 0.48f, ghost, 48);
    draw->AddTriangleFilled(ImVec2(ghostCenter.x - s * 0.33f, ghostCenter.y + s * 0.25f),
        ImVec2(ghostCenter.x - s * 0.08f, ghostCenter.y + s * 0.58f),
        ImVec2(ghostCenter.x + s * 0.08f, ghostCenter.y + s * 0.28f), ghost);
    draw->AddTriangleFilled(ImVec2(ghostCenter.x + s * 0.03f, ghostCenter.y + s * 0.29f),
        ImVec2(ghostCenter.x + s * 0.28f, ghostCenter.y + s * 0.60f),
        ImVec2(ghostCenter.x + s * 0.39f, ghostCenter.y + s * 0.25f), ghost);
    draw->AddCircleFilled(ImVec2(ghostCenter.x - s * 0.14f, ghostCenter.y - s * 0.05f), s * 0.045f, ghostDark, 16);
    draw->AddCircleFilled(ImVec2(ghostCenter.x + s * 0.12f, ghostCenter.y - s * 0.05f), s * 0.045f, ghostDark, 16);
    draw->AddBezierCubic(ImVec2(ghostCenter.x - s * 0.12f, ghostCenter.y + s * 0.13f),
        ImVec2(ghostCenter.x - s * 0.04f, ghostCenter.y + s * 0.22f),
        ImVec2(ghostCenter.x + s * 0.08f, ghostCenter.y + s * 0.22f),
        ImVec2(ghostCenter.x + s * 0.16f, ghostCenter.y + s * 0.11f), ghostDark, 3.0f, 18);

    draw->AddTriangleFilled(ImVec2(center.x - s * 0.45f, center.y - s * 0.16f),
        ImVec2(center.x - s * 0.25f, center.y - s * 0.56f),
        ImVec2(center.x - s * 0.04f, center.y - s * 0.17f), cat);
    draw->AddTriangleFilled(ImVec2(center.x + s * 0.06f, center.y - s * 0.17f),
        ImVec2(center.x + s * 0.27f, center.y - s * 0.56f),
        ImVec2(center.x + s * 0.48f, center.y - s * 0.16f), cat);
    draw->AddCircleFilled(center, s * 0.48f, cat, 64);
    draw->AddCircle(center, s * 0.48f, catEdge, 64, 3.0f);

    draw->AddCircleFilled(ImVec2(center.x - s * 0.17f, center.y - s * 0.02f), s * 0.045f, eye, 16);
    draw->AddCircleFilled(ImVec2(center.x + s * 0.17f, center.y - s * 0.02f), s * 0.045f, eye, 16);
    draw->AddCircleFilled(ImVec2(center.x, center.y + s * 0.08f), s * 0.035f, bow, 16);
    draw->AddLine(ImVec2(center.x, center.y + s * 0.11f), ImVec2(center.x - s * 0.08f, center.y + s * 0.18f), eye, 2.0f);
    draw->AddLine(ImVec2(center.x, center.y + s * 0.11f), ImVec2(center.x + s * 0.08f, center.y + s * 0.18f), eye, 2.0f);

    ImVec2 bowC(center.x - s * 0.33f, center.y - s * 0.36f);
    draw->AddTriangleFilled(ImVec2(bowC.x, bowC.y), ImVec2(bowC.x - s * 0.19f, bowC.y - s * 0.11f), ImVec2(bowC.x - s * 0.17f, bowC.y + s * 0.12f), bow);
    draw->AddTriangleFilled(ImVec2(bowC.x, bowC.y), ImVec2(bowC.x + s * 0.19f, bowC.y - s * 0.11f), ImVec2(bowC.x + s * 0.17f, bowC.y + s * 0.12f), bow);
    draw->AddCircleFilled(bowC, s * 0.065f, mint, 18);
}

static void CenterText(const char* text)
{
    float width = ImGui::CalcTextSize(text).x;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    ImGui::TextUnformatted(text);
}

static void PushBold()
{
    if (g_FontBold)
        ImGui::PushFont(g_FontBold);
}

static void PopBold()
{
    if (g_FontBold)
        ImGui::PopFont();
}

static bool SmallButton(const char* label, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.145f, 0.145f, 0.165f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.190f, 0.190f, 0.215f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.235f, 0.235f, 0.260f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.28f, 0.31f, 1.0f));
    PushBold();
    bool clicked = ImGui::Button(label, size);
    PopBold();
    ImGui::PopStyleColor(4);
    return clicked;
}

static bool IconButton(const char* id, bool closeIcon)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(24.0f, 20.0f);
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    ImU32 bg = hovered ? ColorU32(48, 48, 54, 255) : ColorU32(28, 28, 33, 255);
    ImU32 fg = ColorU32(188, 188, 194, 255);
    draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 4.0f);

    float cx = p.x + size.x * 0.5f;
    float cy = p.y + size.y * 0.5f;
    if (closeIcon) {
        draw->AddLine(ImVec2(cx - 4.0f, cy - 4.0f), ImVec2(cx + 4.0f, cy + 4.0f), fg, 1.6f);
        draw->AddLine(ImVec2(cx + 4.0f, cy - 4.0f), ImVec2(cx - 4.0f, cy + 4.0f), fg, 1.6f);
    }
    else {
        draw->AddLine(ImVec2(cx - 5.0f, cy + 3.0f), ImVec2(cx + 5.0f, cy + 3.0f), fg, 1.6f);
    }
    return clicked;
}

static void RenderTitleBar()
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    draw->AddRectFilled(pos, ImVec2(pos.x + io.DisplaySize.x, pos.y + 30.0f), ColorU32(18, 18, 22, 255));
    draw->AddLine(ImVec2(pos.x, pos.y + 30.0f), ImVec2(pos.x + io.DisplaySize.x, pos.y + 30.0f), ColorU32(52, 52, 58, 255), 1.0f);

    ImGui::SetCursorPos(ImVec2(12.0f, 7.0f));
    PushBold();
    ImGui::TextUnformatted("KITTYCORE V1.0.0 ( BETA )");
    PopBold();

    ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 58.0f, 5.0f));
    if (IconButton("##minimize", false))
        ::ShowWindow(g_Hwnd, SW_MINIMIZE);
    ImGui::SameLine(0.0f, 4.0f);
    if (IconButton("##close", true))
        ::PostMessageW(g_Hwnd, WM_CLOSE, 0, 0);
}

static void DrawInnerPanel(ImVec2 localPos, ImVec2 localSize)
{
    ImVec2 origin = ImGui::GetWindowPos();
    ImVec2 p0(origin.x + localPos.x, origin.y + localPos.y);
    ImVec2 p1(p0.x + localSize.x, p0.y + localSize.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p0, p1, ColorU32(18, 18, 22, 255), 5.0f);
    draw->AddRect(p0, p1, ColorU32(35, 35, 40, 255), 5.0f, 0, 1.0f);
}

static void InfoRow(const char* label, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.66f, 0.76f, 1.0f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value.c_str());
}

static void RenderLoading(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size = io.DisplaySize;
    ImGui::SetCursorPos(ImVec2(0.0f, 58.0f));
    DrawLogoImage(ImVec2(122.0f, 122.0f), 0.70f);
    ImGui::SetCursorPosY(184.0f);
    PushBold();
    CenterText("KITTYCORE");
    PopBold();

    float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - app.startedAt).count();
    float progress = std::min(1.0f, elapsed / 1.35f);
    float barW = std::min(180.0f, size.x - 120.0f);
    ImGui::SetCursorPos(ImVec2((size.x - barW) * 0.5f, 226.0f));
    ImGui::ProgressBar(progress, ImVec2(barW, 9.0f), "");

    if (app.systemFuture.valid() && app.systemFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        app.system = app.systemFuture.get();
        app.systemReady = true;
    }

    if (elapsed > 1.1f)
        app.screen = Screen::Login;
}

static void PollSystemInfo(AppState& app)
{
    if (app.systemFuture.valid() && app.systemFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        app.system = app.systemFuture.get();
        app.systemReady = true;
        if (g_SpecServerRunning)
            UpdateSpecSnapshot(app.system);
    }
}

static void RenderLogin(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size = io.DisplaySize;
    DrawInnerPanel(ImVec2(18.0f, 40.0f), ImVec2(size.x - 36.0f, size.y - 50.0f));
    ImGui::SetCursorPos(ImVec2(26.0f, 44.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("login_panel", ImVec2(size.x - 52.0f, size.y - 56.0f), false, ImGuiWindowFlags_NoScrollbar);

    DrawLogoImage(ImVec2(112.0f, 112.0f), 0.70f);
    PushBold();
    CenterText("KITTYCORE");
    PopBold();

    ImGui::Dummy(ImVec2(1.0f, 6.0f));

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##discord_id", "DISCORD ID", app.discordId.data(), app.discordId.size(), ImGuiInputTextFlags_CharsDecimal);

    if (app.authInFlight) {
        ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1.0f, 8.0f), "");
    }
    else {
        float enterW = 112.0f;
        float discordW = 88.0f;
        float totalW = enterW + discordW + 8.0f;
        float availW = ImGui::GetContentRegionAvail().x;
        if (availW > totalW)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - totalW) * 0.5f);
        if (SmallButton("ENTRAR", ImVec2(enterW, 30.0f))) {
            std::string discord = app.discordId.data();
            app.authStatus = "VALIDANDO";
            app.authInFlight = true;
            AppConfig config = app.config;
            SystemInfo system = app.system;
            app.authFuture = std::async(std::launch::async, [config, system, discord]() {
                return VerifyAccess(config, system, discord);
            });
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (SmallButton("DISCORD", ImVec2(discordW, 30.0f)))
            ShellExecuteA(nullptr, "open", app.config.discordInviteUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    if (app.authInFlight && app.authFuture.valid() && app.authFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        app.auth = app.authFuture.get();
        app.authInFlight = false;
        app.authStatus = CompactAuthStatus(app.auth);
        if (app.auth.allowed) {
            if (!app.specSiteOpened) {
                StartSpecServer(app.system);
                ::Sleep(150);
                ShellExecuteA(nullptr, "open", "http://127.0.0.1:7777/", nullptr, nullptr, SW_SHOWNORMAL);
                app.specSiteOpened = true;
            }
            app.screen = Screen::Terms;
        }
    }

    PushBold();
    ImGui::TextWrapped("%s", app.authStatus.c_str());
    PopBold();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderTerms(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size = io.DisplaySize;
    DrawInnerPanel(ImVec2(18.0f, 40.0f), ImVec2(size.x - 36.0f, size.y - 50.0f));
    ImGui::SetCursorPos(ImVec2(26.0f, 44.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("terms_panel", ImVec2(size.x - 52.0f, size.y - 56.0f), false);

    PushBold();
    ImGui::TextUnformatted("TERMOS");
    PopBold();
    ImGui::Separator();
    ImGui::BeginChild("terms_scroll", ImVec2(0, 116.0f), true);
    ImGui::TextWrapped("CHECAGEM DO PC");
    ImGui::TextWrapped("LOGS DE USO");
    ImGui::TextWrapped("ALTERACOES INTERNAS");
    ImGui::TextWrapped("REINICIO PODE SER NECESSARIO");
    ImGui::EndChild();

    ImGui::Checkbox("ACEITO", &app.acceptedTerms);
    ImGui::BeginDisabled(!app.acceptedTerms);
    float continueW = 126.0f;
    float termsAvail = ImGui::GetContentRegionAvail().x;
    if (termsAvail > continueW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (termsAvail - continueW) * 0.5f);
    if (SmallButton("CONTINUAR", ImVec2(continueW, 30.0f))) {
        app.screen = Screen::Main;
        app.phantomFace = true;
    }
    ImGui::EndDisabled();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderMain(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size = io.DisplaySize;
    DrawInnerPanel(ImVec2(18.0f, 40.0f), ImVec2(size.x - 36.0f, size.y - 50.0f));
    ImGui::SetCursorPos(ImVec2(26.0f, 48.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("main_panel", ImVec2(size.x - 52.0f, size.y - 62.0f), false, ImGuiWindowFlags_NoScrollbar);

    DrawLogoImage(ImVec2(120.0f, 120.0f), 0.70f);
    PushBold();
    CenterText("KITTYCORE");
    PopBold();

    ImGui::Dummy(ImVec2(1.0f, 12.0f));
    float optimizeW = 132.0f;
    float mainAvail = ImGui::GetContentRegionAvail().x;
    if (mainAvail > optimizeW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (mainAvail - optimizeW) * 0.5f);
    bool optimize = SmallButton("OTIMIZAR", ImVec2(optimizeW, 32.0f));

    if (optimize && !app.optimizeLaunched) {
        app.optimizeLaunched = LaunchOptimizerScript(app.optimizeStatus);
    }
    else if (optimize && app.optimizeLaunched) {
        app.optimizeStatus = "ABERTO";
    }

    PushBold();
    ImGui::TextWrapped("%s", app.optimizeStatus.c_str());
    PopBold();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderApp(AppState& app)
{
    ImGuiIO& io = ImGui::GetIO();
    PollSystemInfo(app);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("KittyCoreRoot", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    DrawBackground(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize(), app.phantomFace);
    RenderTitleBar();

    switch (app.screen) {
    case Screen::Loading:
        RenderLoading(app);
        break;
    case Screen::Login:
        RenderLogin(app);
        break;
    case Screen::Terms:
        RenderTerms(app);
        break;
    case Screen::Main:
        RenderMain(app);
        break;
    }

    ImGui::End();
}

int main(int, char**)
{
    g_AppRoot = FindAppRoot();

    ImGui_ImplWin32_EnableDpiAwareness();
    float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    HINSTANCE instance = GetModuleHandle(nullptr);
    HICON appIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance, appIcon, nullptr, nullptr, nullptr, L"KittyCoreWindow", appIcon };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"KittyCore v1.0.0 ( beta )", WS_POPUP,
        160, 160, static_cast<int>(400 * mainScale), static_cast<int>(300 * mainScale), nullptr, nullptr, wc.hInstance, nullptr);
    g_Hwnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    g_FontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 13.5f * mainScale);
    g_FontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 13.5f * mainScale);
    if (!g_FontRegular)
        io.Fonts->AddFontDefault();

    ApplyKittyStyle(mainScale);
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleDpi = mainScale;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    if (!LoadTextureFromFile(g_AppRoot / "final.png", &g_LogoTexture, &g_LogoWidth, &g_LogoHeight))
        if (!LoadTextureFromFile(g_AppRoot / "gentty.png", &g_LogoTexture, &g_LogoWidth, &g_LogoHeight))
            LoadTextureFromResource(IDB_LOGIN_LOGO, &g_LogoTexture, &g_LogoWidth, &g_LogoHeight);

    AppState app;
    app.config = LoadConfig();
    if (!app.config.discordInviteUrl.empty())
        ShellExecuteA(nullptr, "open", app.config.discordInviteUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    app.systemFuture = std::async(std::launch::async, CollectSystemInfo);

    ImVec4 clearColor = ImVec4(0.07f, 0.06f, 0.09f, 1.00f);
    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderApp(app);

        ImGui::Render();
        const float clearColorWithAlpha[4] = {
            clearColor.x * clearColor.w,
            clearColor.y * clearColor.w,
            clearColor.z * clearColor.w,
            clearColor.w
        };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColorWithAlpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    StopSpecServer();
    CleanupExtractedOptimizerScript();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_LogoTexture) {
        g_LogoTexture->Release();
        g_LogoTexture = nullptr;
    }

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
    backBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCHITTEST) {
        POINT cursor{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rect{};
        ::GetWindowRect(hWnd, &rect);
        const int titleHeight = 30;
        const int controlWidth = 72;
        if (cursor.y >= rect.top && cursor.y < rect.top + titleHeight) {
            if (cursor.x < rect.right - controlWidth)
                return HTCAPTION;
            return HTCLIENT;
        }
        return HTCLIENT;
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = LOWORD(lParam);
        g_ResizeHeight = HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
