#include "ThemedIcon.h"

#include "ThemeManager.h"

#include <QFile>
#include <QIconEngine>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QRegularExpression>
#include <QSvgRenderer>

#include <cmath>

namespace TSA::UI
{

namespace
{
QHash<QString, QString> loadPalette(const char* theme)
{
    QHash<QString, QString> table;
    QFile f(QStringLiteral(":/icons/palette.json"));
    if (!f.open(QIODevice::ReadOnly)) return table;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object().value(QLatin1String(theme)).toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) table.insert(it.key().toUpper(), it.value().toString().toUpper());
    return table;
}

const QByteArray& rawSvg(const QString& path)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.find(path);
    if (it == cache.end())
    {
        QFile f(path);
        it = cache.insert(path, f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
    }
    return it.value();
}

/// Rendu désactivé : niveaux de gris, opacité réduite (la forme reste lisible sur les deux thèmes).
void toDisabled(QImage& img, bool dark)
{
    // Gris et opacité choisis pour ~3:1 (sombre) et ~2,3:1 (clair) avec le fond du ruban : atténué mais lisible
    const int target = dark ? 170 : 100;
    for (int y = 0; y < img.height(); ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const int a = qAlpha(line[x]);
            if (!a) continue;
            const int na = a * 55 / 100;
            const int g = target * na / 255;   // prémultiplié
            line[x] = qRgba(g, g, g, na);
        }
    }
}

class ThemedSvgIconEngine : public QIconEngine
{
public:
    explicit ThemedSvgIconEngine(const QString& path) : m_path(path) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State /*state*/, qreal scale) override
    {
        const bool dark = ThemeManager::instance().isDarkMode();
        const QSize px(std::max(1, static_cast<int>(std::lround(size.width() * scale))),
                       std::max(1, static_cast<int>(std::lround(size.height() * scale))));
        const QString key = QStringLiteral("tsaicon|%1|%2x%3|%4|%5").arg(m_path).arg(px.width()).arg(px.height())
                                .arg(mode == QIcon::Disabled ? 1 : 0).arg(dark ? 1 : 0);
        QPixmap pm;
        if (!QPixmapCache::find(key, &pm))
        {
            pm = QPixmap::fromImage(renderThemedIcon(m_path, std::min(px.width(), px.height()), dark, mode));
            QPixmapCache::insert(key, pm);
        }
        pm.setDevicePixelRatio(scale);
        return pm;
    }

    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine* clone() const override { return new ThemedSvgIconEngine(m_path); }
    QString key() const override { return QStringLiteral("TSAThemedSvg"); }
    bool isNull() override { return rawSvg(m_path).isEmpty(); }

private:
    QString m_path;
};
} // namespace

const QHash<QString, QString>& iconPalette(bool dark)
{
    static const QHash<QString, QString> darkTable = loadPalette("dark");
    static const QHash<QString, QString> lightTable = loadPalette("light");
    return dark ? darkTable : lightTable;
}

QByteArray themedSvg(const QString& resourcePath, bool dark)
{
    QString svg = QString::fromUtf8(rawSvg(resourcePath));
    if (svg.isEmpty()) return QByteArray();
    const auto& table = iconPalette(dark);
    if (!table.isEmpty())
    {
        static const QRegularExpression color(QStringLiteral("#[0-9A-Fa-f]{6}\\b"));
        QString out;
        out.reserve(svg.size());
        qsizetype last = 0;
        for (auto it = color.globalMatch(svg); it.hasNext();)
        {
            const auto m = it.next();
            out += QStringView(svg).mid(last, m.capturedStart() - last);
            const QString c = m.captured(0).toUpper();
            out += table.value(c, m.captured(0));
            last = m.capturedEnd();
        }
        out += QStringView(svg).mid(last);
        svg = out;
    }
    return svg.toUtf8();
}

QImage renderThemedIcon(const QString& resourcePath, int size, bool dark, QIcon::Mode mode)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer renderer(themedSvg(resourcePath, dark));
    if (renderer.isValid())
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
    }
    if (mode == QIcon::Disabled) toDisabled(img, dark);
    return img;
}

QIcon themedIcon(const QString& resourcePath)
{
    if (!resourcePath.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) return QIcon(resourcePath);
    return QIcon(new ThemedSvgIconEngine(resourcePath));
}

} // namespace TSA::UI
