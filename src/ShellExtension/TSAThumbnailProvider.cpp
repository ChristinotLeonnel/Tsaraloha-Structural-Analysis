// TSAThumbnailProvider.dll — miniatures des fichiers .tsa dans l'Explorateur Windows.
//
// Extension Shell (IThumbnailProvider + IInitializeWithStream) volontairement minimale :
//  - aucune dépendance Qt / OpenCASCADE / solveur ; runtime C statique : fonctionne sur tout poste ;
//  - lit uniquement l'en-tête (272 o) et le bloc d'aperçu PNG non compressé écrit par TSA
//    (TSAPreviewBlock.h) : temps constant, quelle que soit la taille du modèle ;
//  - l'image est celle du viewport TSA à l'enregistrement : aucun second moteur de rendu ;
//  - toute anomalie (fichier corrompu, ancien, d'une version future, chiffré…) renvoie E_FAIL :
//    l'Explorateur affiche alors l'icône TSA. Aucune exception ne sort de la DLL.
//
// Enregistrement : DllRegisterServer / DllUnregisterServer (utilisateur courant, HKCU, sans droits
// administrateur) ; DllInstall(…, L"machine") pour tous les utilisateurs (HKLM, installeur).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <olectl.h>
#include <thumbcache.h>
#include <wincodec.h>

#include "../IO/TSAPreviewBlock.h"

#include <atomic>
#include <new>
#include <string>
#include <vector>

namespace
{

// {5BA6698A-ED79-442D-92EE-31DA2704C07D}
constexpr CLSID CLSID_TSAThumbnailProvider = { 0x5ba6698a, 0xed79, 0x442d, { 0x92, 0xee, 0x31, 0xda, 0x27, 0x04, 0xc0, 0x7d } };
constexpr wchar_t kClsidString[] = L"{5BA6698A-ED79-442D-92EE-31DA2704C07D}";
constexpr wchar_t kThumbnailHandlerKey[] = L"{e357fccd-a995-4576-b01f-234630154e96}"; // IThumbnailProvider
constexpr wchar_t kExtension[] = L".tsa";
constexpr wchar_t kProgId[] = L"TSA.Project";

HINSTANCE g_module = nullptr;
std::atomic<long> g_objects{ 0 };
std::atomic<long> g_locks{ 0 };

// -----------------------------------------------------------------------------
// Lecture du bloc d'aperçu depuis le flux fourni par l'Explorateur
// -----------------------------------------------------------------------------

bool readExact(IStream* stream, uint64_t offset, void* buffer, ULONG size)
{
    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (FAILED(stream->Seek(pos, STREAM_SEEK_SET, nullptr))) return false;
    ULONG done = 0;
    auto* out = static_cast<BYTE*>(buffer);
    while (done < size)
    {
        ULONG n = 0;
        const HRESULT hr = stream->Read(out + done, size - done, &n);
        if (FAILED(hr) || n == 0) return false;
        done += n;
    }
    return true;
}

bool readPreviewPng(IStream* stream, std::vector<BYTE>& png)
{
    STATSTG stat{};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) return false;
    const uint64_t length = stat.cbSize.QuadPart;
    if (length < sizeof(TSA::IO::TSAFileHeader)) return false;

    TSA::IO::TSAFileHeader header;
    if (!readExact(stream, 0, &header, sizeof(header))) return false;
    uint64_t blockOffset = 0;
    if (!TSA::IO::locatePreviewBlock(header, length, &blockOffset)) return false;

    TSA::IO::TSAPreviewBlockHeader block;
    if (!readExact(stream, blockOffset, &block, sizeof(block))) return false;
    TSA::IO::TSAPreviewLocation where;
    if (!TSA::IO::validatePreviewBlock(block, blockOffset, length, &where)) return false;

    png.resize(where.imageSize);
    return readExact(stream, where.imageOffset, png.data(), where.imageSize);
}

// PNG → HBITMAP 32 bpp (DIB top-down), mis à l'échelle dans un carré cx×cx sans déformation.
HRESULT decodeToBitmap(const std::vector<BYTE>& png, UINT cx, HBITMAP* out)
{
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    IWICStream* wicStream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    HBITMAP bitmap = nullptr;

    hr = factory->CreateStream(&wicStream);
    if (SUCCEEDED(hr)) hr = wicStream->InitializeFromMemory(const_cast<BYTE*>(png.data()), static_cast<DWORD>(png.size()));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(wicStream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);

    UINT srcW = 0, srcH = 0;
    if (SUCCEEDED(hr)) hr = frame->GetSize(&srcW, &srcH);
    if (SUCCEEDED(hr) && (srcW == 0 || srcH == 0)) hr = E_FAIL;

    UINT dstW = srcW, dstH = srcH;
    if (SUCCEEDED(hr))
    {
        const double scale = static_cast<double>(cx) / static_cast<double>(srcW > srcH ? srcW : srcH);
        dstW = static_cast<UINT>(srcW * scale + 0.5);
        dstH = static_cast<UINT>(srcH * scale + 0.5);
        if (dstW == 0) dstW = 1;
        if (dstH == 0) dstH = 1;
        hr = factory->CreateBitmapScaler(&scaler);
    }
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, dstW, dstH, WICBitmapInterpolationModeHighQualityCubic);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);

    if (SUCCEEDED(hr))
    {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = static_cast<LONG>(dstW);
        bmi.bmiHeader.biHeight = -static_cast<LONG>(dstH); // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) hr = E_OUTOFMEMORY;
        if (SUCCEEDED(hr)) hr = converter->CopyPixels(nullptr, dstW * 4, dstW * 4 * dstH, static_cast<BYTE*>(bits));
        if (FAILED(hr) && bitmap)
        {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (wicStream) wicStream->Release();
    factory->Release();

    if (SUCCEEDED(hr)) *out = bitmap;
    return hr;
}

// -----------------------------------------------------------------------------
// Objet COM
// -----------------------------------------------------------------------------

class ThumbnailProvider final : public IInitializeWithStream, public IThumbnailProvider
{
public:
    ThumbnailProvider() { ++g_objects; }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IInitializeWithStream)
            *ppv = static_cast<IInitializeWithStream*>(this);
        else if (riid == IID_IThumbnailProvider)
            *ppv = static_cast<IThumbnailProvider*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(++m_refs); }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        const long refs = --m_refs;
        if (refs == 0) delete this;
        return static_cast<ULONG>(refs);
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* stream, DWORD /*grfMode*/) override
    {
        if (!stream) return E_INVALIDARG;
        if (m_stream) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        m_stream = stream;
        m_stream->AddRef();
        return S_OK;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override
    {
        if (!phbmp || !pdwAlpha) return E_POINTER;
        *phbmp = nullptr;
        *pdwAlpha = WTSAT_UNKNOWN;
        if (!m_stream || cx == 0) return E_UNEXPECTED;
        try
        {
            std::vector<BYTE> png;
            if (!readPreviewPng(m_stream, png)) return E_FAIL; // icône TSA par défaut
            const HRESULT hr = decodeToBitmap(png, cx, phbmp);
            if (SUCCEEDED(hr)) *pdwAlpha = WTSAT_RGB; // capture opaque du viewport
            return hr;
        }
        catch (...)
        {
            return E_FAIL; // jamais d'exception vers l'Explorateur
        }
    }

private:
    ~ThumbnailProvider()
    {
        if (m_stream) m_stream->Release();
        --g_objects;
    }

    std::atomic<long> m_refs{ 1 };
    IStream* m_stream = nullptr;
};

class ClassFactory final : public IClassFactory
{
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(++m_refs); }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        const long refs = --m_refs;
        if (refs == 0) delete this;
        return static_cast<ULONG>(refs);
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* provider = new (std::nothrow) ThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        const HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override
    {
        if (lock) ++g_locks;
        else --g_locks;
        return S_OK;
    }

private:
    std::atomic<long> m_refs{ 1 };
};

// -----------------------------------------------------------------------------
// Enregistrement (HKCU par défaut, HKLM pour une installation machine)
// -----------------------------------------------------------------------------

bool setString(HKEY root, const std::wstring& subKey, const wchar_t* name, const std::wstring& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const LSTATUS st = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                      static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

std::wstring readDefault(HKEY root, const std::wstring& subKey)
{
    wchar_t buf[256] = { 0 };
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, subKey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

HRESULT registerServer(HKEY root)
{
    wchar_t path[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(g_module, path, MAX_PATH) == 0) return HRESULT_FROM_WIN32(GetLastError());

    const std::wstring classes = L"Software\\Classes\\";
    const std::wstring clsidKey = classes + L"CLSID\\" + kClsidString;
    bool ok = setString(root, clsidKey, nullptr, L"TSA Thumbnail Provider")
           && setString(root, clsidKey + L"\\InprocServer32", nullptr, path)
           && setString(root, clsidKey + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    // Gestionnaire sur l'extension et sur le ProgID (selon la façon dont l'Explorateur résout le type).
    ok = ok && setString(root, classes + kExtension + L"\\ShellEx\\" + kThumbnailHandlerKey, nullptr, kClsidString);
    ok = ok && setString(root, classes + kProgId + L"\\ShellEx\\" + kThumbnailHandlerKey, nullptr, kClsidString);
    if (!ok) return SELFREG_E_CLASS;
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

HRESULT unregisterServer(HKEY root)
{
    const std::wstring classes = L"Software\\Classes\\";
    // Ne retire les gestionnaires que s'ils pointent encore vers cette extension.
    for (const std::wstring owner : { std::wstring(kExtension), std::wstring(kProgId) })
    {
        const std::wstring key = classes + owner + L"\\ShellEx\\" + kThumbnailHandlerKey;
        if (_wcsicmp(readDefault(root, key).c_str(), kClsidString) == 0) RegDeleteTreeW(root, key.c_str());
    }
    RegDeleteTreeW(root, (classes + L"CLSID\\" + kClsidString).c_str());
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

} // namespace

// -----------------------------------------------------------------------------
// Exports
// -----------------------------------------------------------------------------

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid != CLSID_TSAThumbnailProvider) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return (g_objects.load() == 0 && g_locks.load() == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    return registerServer(HKEY_CURRENT_USER);
}

STDAPI DllUnregisterServer()
{
    return unregisterServer(HKEY_CURRENT_USER);
}

// regsvr32 /i:machine TSAThumbnailProvider.dll   (installeur, droits administrateur)
// regsvr32 /u /i:machine TSAThumbnailProvider.dll
STDAPI DllInstall(BOOL install, PCWSTR cmdLine)
{
    const bool machine = cmdLine && _wcsicmp(cmdLine, L"machine") == 0;
    const HKEY root = machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    return install ? registerServer(root) : unregisterServer(root);
}
