#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include <utility>

namespace TSA::NDC
{

struct NDCTable
{
    QString caption;
    QStringList headers;
    std::vector<QStringList> rows;
};

struct NDCSection
{
    QString title;
    QStringList paragraphs;
    std::vector<std::pair<QString, QString>> keyValues;
    std::vector<NDCTable> tables;
};

struct NDCChapter
{
    int number = 1;
    QString title;
    std::vector<NDCSection> sections;
};

class NDCDocument
{
public:
    NDCDocument();

    QString projectTitle = "Projet TSA";
    QString author = "Ingénieur Structure";
    QString organization = "Tsaraloha Structural Analysis";
    QString date;
    QString softwareVersion = "TSA v1.0.0 (Moteur : OpenSees v3.8.0)";
    QString standardReference = "Eurocodes (EN 1990, EN 1991, EN 1992, EN 1993)";

    std::vector<NDCChapter> chapters;

    void addChapter(const NDCChapter& chapter) { chapters.push_back(chapter); }
    void clear() { chapters.clear(); }

    [[nodiscard]] QString toHtml() const;
    [[nodiscard]] QString toPlainText() const;
};

} // namespace TSA::NDC
