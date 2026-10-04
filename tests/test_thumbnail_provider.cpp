// Suite « thumbnail » : bloc d'aperçu du format .tsa 1.2 et extension Explorateur
// TSAThumbnailProvider.dll (tests 127-129). La DLL est chargée directement (LoadLibrary +
// DllGetClassObject), exactement comme le fait le Shell, sans dépendre de l'enregistrement.
// Fichier exclu du PCH (CMakeLists) : inclut <windows.h> avec ses propres réglages.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlwapi.h>
#include <thumbcache.h>

#include "test_common.h"

#include "IO/TSAFile.h"
#include "IO/TSAPreviewBlock.h"
#include "Model/Model.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>

#include <fstream>

namespace
{

QImage viewportLike(int w, int h)
{
    // Image opaque reconnaissable (dégradé + diagonale), comme une capture de viewport.
    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixelColor(x, y, QColor(40 + x * 120 / w, 60 + y * 120 / h, 150));
    for (int i = 0; i < std::min(w, h); ++i) img.setPixelColor(i, i, Qt::white);
    return img;
}

void buildFrame(TSA::Model::Model& m, int bays)
{
    int prev = m.addNode(0, 0, 0);
    for (int i = 1; i <= bays; ++i)
    {
        const int n = m.addNode(i * 5.0, 0, 3.0 * (i % 2));
        m.addBeam(prev, n);
        prev = n;
    }
}

bool saveWithThumbnail(const QString& path, const TSA::Model::Model& m, const QImage& img)
{
    return TSA::IO::TSAProjectIO::saveProject(path, m, nullptr, "Test", "TSA", true, img);
}

// Réécrit un fichier 1.2 comme un fichier 1.1 : sans bloc d'aperçu (fichier « ancien »).
bool downgradeToV11(const QString& path)
{
    std::fstream f(path.toStdString(), std::ios::in | std::ios::out | std::ios::binary);
    TSA::IO::TSAFileHeader h;
    if (!f.read(reinterpret_cast<char*>(&h), sizeof(h))) return false;
    h.versionMinor = 1;
    h.flags &= ~TSA::IO::FLAG_HAS_PREVIEW_BLOCK;
    f.seekp(0);
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    f.close();
    return QFile::resize(path, static_cast<qint64>(h.fileSize));
}

// --- Appel direct de l'extension, comme le Shell -------------------------------------------
struct ThumbResult
{
    HRESULT hr = E_FAIL;
    int width = 0;
    int height = 0;
    double ms = 0.0;
};

ThumbResult thumbnailFromDll(HMODULE dll, const QString& file, UINT cx)
{
    ThumbResult r;
    using GetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
    auto getClassObject = reinterpret_cast<GetClassObjectFn>(GetProcAddress(dll, "DllGetClassObject"));
    if (!getClassObject) return r;
    static const CLSID clsid = { 0x5ba6698a, 0xed79, 0x442d, { 0x92, 0xee, 0x31, 0xda, 0x27, 0x04, 0xc0, 0x7d } };

    QElapsedTimer timer;
    timer.start();
    IClassFactory* factory = nullptr;
    r.hr = getClassObject(clsid, IID_PPV_ARGS(&factory));
    if (FAILED(r.hr)) return r;
    IInitializeWithStream* init = nullptr;
    r.hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(&init));
    factory->Release();
    if (FAILED(r.hr)) return r;

    IStream* stream = nullptr;
    r.hr = SHCreateStreamOnFileEx(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(file).utf16()),
                                  STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
    if (SUCCEEDED(r.hr)) r.hr = init->Initialize(stream, STGM_READ);
    IThumbnailProvider* provider = nullptr;
    if (SUCCEEDED(r.hr)) r.hr = init->QueryInterface(IID_PPV_ARGS(&provider));
    HBITMAP bmp = nullptr;
    WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
    if (SUCCEEDED(r.hr)) r.hr = provider->GetThumbnail(cx, &bmp, &alpha);
    r.ms = timer.nsecsElapsed() / 1e6;
    if (bmp)
    {
        BITMAP info{};
        GetObject(bmp, sizeof(info), &info);
        r.width = info.bmWidth;
        r.height = std::abs(info.bmHeight);
        DeleteObject(bmp);
    }
    if (provider) provider->Release();
    if (stream) stream->Release();
    init->Release();
    return r;
}

} // namespace

bool runSuite_Thumbnail(int& passed)
{
    // TEST 127 : format 1.2 — bloc d'aperçu, compatibilité, robustesse
    {
        QTemporaryDir dir;
        TSA::Model::Model m;
        buildFrame(m, 4);
        const QString path = dir.filePath("Building.tsa");
        TEST_CHECK(saveWithThumbnail(path, m, viewportLike(640, 480)), "Test 127: enregistrement");

        TSA::IO::TSAFileHeader h;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(path.toStdString(), h), "Test 127: en-tête");
        TEST_CHECK(h.versionMinor == 2 && (h.flags & TSA::IO::FLAG_HAS_PREVIEW_BLOCK), "Test 127: format 1.2 avec bloc d'aperçu");
        QImage preview;
        TEST_CHECK(TSA::IO::TSAFileReader::extractPreviewBlock(path.toStdString(), preview) && preview.size() == QSize(640, 480),
                   "Test 127: aperçu lu sans décompression");

        TSA::Model::Model reloaded;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadProject(path, reloaded) && reloaded.beams().size() == 4,
                   "Test 127: le modèle se relit malgré le bloc ajouté après le payload");

        // Fichier ancien (1.1, sans bloc) : se relit ; miniature via le chunk THMB.
        TEST_CHECK(downgradeToV11(path), "Test 127: fabrication d'un fichier 1.1");
        TSA::Model::Model old;
        QImage oldThumb;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadProject(path, old) && old.beams().size() == 4, "Test 127: fichier 1.1 relu");
        TEST_CHECK(!TSA::IO::TSAFileReader::extractPreviewBlock(path.toStdString(), oldThumb), "Test 127: pas de bloc dans un fichier 1.1");
        TEST_CHECK(TSA::IO::TSAProjectIO::extractThumbnail(path, oldThumb) && !oldThumb.isNull(), "Test 127: repli THMB");

        // Fichiers invalides : jamais d'exception, simple échec.
        const QString junk = dir.filePath("junk.tsa");
        { QFile f(junk); TEST_CHECK(f.open(QIODevice::WriteOnly), "Test 127: fichier"); f.write(QByteArray(5000, '\x5A')); }
        TEST_CHECK(!TSA::IO::TSAFileReader::extractPreviewBlock(junk.toStdString(), preview), "Test 127: fichier non TSA");
        TSA::IO::TSAPreviewBlockHeader evil;
        evil.width = 640; evil.height = 480; evil.dataSize = 0xFFFFFFF0u;
        TSA::IO::TSAPreviewLocation loc;
        TEST_CHECK(!TSA::IO::validatePreviewBlock(evil, 1000, 2000, &loc), "Test 127: taille d'image aberrante refusée");
        evil.dataSize = 100;
        TEST_CHECK(!TSA::IO::validatePreviewBlock(evil, 1950, 2000, &loc), "Test 127: image tronquée refusée");
        TSA::IO::TSAFileHeader future = h;
        future.versionMajor = TSA::IO::TSA_FORMAT_VERSION_MAJOR + 1;
        uint64_t off = 0;
        TEST_CHECK(!TSA::IO::locatePreviewBlock(future, 1u << 20, &off), "Test 127: version majeure future refusée");
        // Fichier protégé par mot de passe : aucun aperçu en clair.
        const QString secret = dir.filePath("Secret.tsa");
        TSA::IO::TSAFileWriter writer;
        writer.setThumbnail(viewportLike(640, 480));
        writer.setEncryptionEnabled(true, "motdepasse");
        TEST_CHECK(writer.saveToFile(secret.toStdString(), m, nullptr, "Secret", "TSA"), "Test 127: enregistrement chiffré");
        TSA::IO::TSAFileHeader hs;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(secret.toStdString(), hs) && (hs.flags & TSA::IO::FLAG_ENCRYPTED)
                       && !(hs.flags & TSA::IO::FLAG_HAS_PREVIEW_BLOCK),
                   "Test 127: fichier chiffré sans bloc d'aperçu");
        TEST_CHECK(!TSA::IO::TSAFileReader::extractPreviewBlock(secret.toStdString(), preview), "Test 127: aucune image en clair");
        std::cout << "[PASS] Test 127: Format 1.2 — bloc d'aperçu" << std::endl;
        ++passed;
    }

    // TEST 128 : extension Explorateur chargée comme par le Shell (tailles, robustesse, vitesse)
    {
        const QString dllPath = QCoreApplication::applicationDirPath() + "/TSAThumbnailProvider.dll";
        HMODULE dll = LoadLibraryExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(dllPath).utf16()), nullptr,
                                     LOAD_WITH_ALTERED_SEARCH_PATH);
        TEST_CHECK(dll != nullptr, "Test 128: TSAThumbnailProvider.dll chargée");
        const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        QTemporaryDir dir;
        TSA::Model::Model smallModel, emptyModel, largeModel;
        buildFrame(smallModel, 3);
        buildFrame(largeModel, 20000); // grand modèle : 20 000 barres
        const QString pSmall = dir.filePath("Small.tsa"), pEmpty = dir.filePath("Empty.tsa"), pLarge = dir.filePath("Large.tsa");
        TEST_CHECK(saveWithThumbnail(pSmall, smallModel, viewportLike(640, 480)), "Test 128: petit modèle");
        TEST_CHECK(saveWithThumbnail(pEmpty, emptyModel, viewportLike(640, 480)), "Test 128: modèle vide");
        TEST_CHECK(saveWithThumbnail(pLarge, largeModel, viewportLike(640, 480)), "Test 128: grand modèle");

        // Tailles demandées par l'Explorateur : petites (16/32) à très grandes icônes (256), haute DPI (1024).
        for (UINT cx : { 16u, 32u, 48u, 96u, 256u, 1024u })
        {
            const auto r = thumbnailFromDll(dll, pSmall, cx);
            TEST_CHECK(SUCCEEDED(r.hr), "Test 128: miniature produite");
            TEST_CHECK(r.width == static_cast<int>(cx) && std::abs(r.height - static_cast<int>(cx * 3 / 4)) <= 1,
                       "Test 128: dans le carré demandé, proportions 4:3 conservées");
        }
        TEST_CHECK(SUCCEEDED(thumbnailFromDll(dll, pEmpty, 256).hr), "Test 128: modèle vide");

        const auto tSmall = thumbnailFromDll(dll, pSmall, 256);
        const auto tLarge = thumbnailFromDll(dll, pLarge, 256);
        TEST_CHECK(SUCCEEDED(tLarge.hr), "Test 128: grand modèle");
        std::cout << "  Miniature 256 px : petit modèle " << tSmall.ms << " ms, grand modèle (" << QFileInfo(pLarge).size() / 1024
                  << " Ko) " << tLarge.ms << " ms" << std::endl;
        TEST_CHECK(tLarge.ms < 200.0, "Test 128: temps indépendant de la taille du modèle");

        // Fichiers que l'Explorateur peut rencontrer : jamais de plantage, échec propre → icône TSA.
        const QString junk = dir.filePath("Corrupt.tsa"), zero = dir.filePath("Zero.tsa"), old = dir.filePath("Old.tsa");
        { QFile f(junk); TEST_CHECK(f.open(QIODevice::WriteOnly), "Test 128: fichier"); f.write(QByteArray(70000, '\x13')); }
        { QFile f(zero); TEST_CHECK(f.open(QIODevice::WriteOnly), "Test 128: fichier"); }
        TEST_CHECK(QFile::copy(pSmall, old) && downgradeToV11(old), "Test 128: fichier 1.1");
        const QString cut = dir.filePath("Truncated.tsa");
        TEST_CHECK(QFile::copy(pSmall, cut) && QFile::resize(cut, QFileInfo(pSmall).size() - 1000), "Test 128: fichier tronqué");
        for (const QString& bad : { junk, zero, old, cut })
            TEST_CHECK(thumbnailFromDll(dll, bad, 256).hr == E_FAIL, "Test 128: fichier invalide → échec propre (icône TSA)");

        using CanUnloadFn = HRESULT(STDAPICALLTYPE*)();
        auto canUnload = reinterpret_cast<CanUnloadFn>(GetProcAddress(dll, "DllCanUnloadNow"));
        TEST_CHECK(canUnload && canUnload() == S_OK, "Test 128: aucun objet COM qui fuit");
        FreeLibrary(dll);
        if (SUCCEEDED(coHr)) CoUninitialize();
        std::cout << "[PASS] Test 128: Extension Explorateur (tailles, robustesse, performance)" << std::endl;
        ++passed;
    }

    // TEST 129 : la miniature suit la dernière vue enregistrée (vue A puis vue B)
    {
        QTemporaryDir dir;
        TSA::Model::Model m;
        buildFrame(m, 2);
        const QString path = dir.filePath("Views.tsa");
        QImage viewA(640, 480, QImage::Format_RGB888);
        viewA.fill(QColor(200, 30, 30));
        QImage viewB(640, 480, QImage::Format_RGB888);
        viewB.fill(QColor(30, 30, 200));
        QImage read;
        TEST_CHECK(saveWithThumbnail(path, m, viewA), "Test 129: vue A enregistrée");
        TEST_CHECK(TSA::IO::TSAFileReader::extractPreviewBlock(path.toStdString(), read) && read.pixelColor(5, 5).red() > 150,
                   "Test 129: miniature A");
        TEST_CHECK(saveWithThumbnail(path, m, viewB), "Test 129: vue B enregistrée");
        TEST_CHECK(TSA::IO::TSAFileReader::extractPreviewBlock(path.toStdString(), read) && read.pixelColor(5, 5).blue() > 150
                       && read.pixelColor(5, 5).red() < 100,
                   "Test 129: miniature B remplace A");
        std::cout << "[PASS] Test 129: Miniature = dernière vue enregistrée" << std::endl;
        ++passed;
    }
    return true;
}
