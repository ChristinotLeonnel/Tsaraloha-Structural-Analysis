#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QFrame;

namespace TSA::UI
{

/// En-tête standard pour les boîtes de dialogue de TSA (style Revit / CAO BIM pro).
/// Affiche une icône vectorielle 32x32 thémée, un titre en gras (H1), un sous-titre explicatif
/// et une ligne de séparation horizontale fine et discrète.
class StandardDialogHeader : public QWidget
{
    Q_OBJECT

public:
    explicit StandardDialogHeader(const QString& title,
                                 const QString& subtitle = QString(),
                                 const QString& iconPath = QString(),
                                 QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);
    void setIcon(const QString& iconPath);

    QString title() const;
    QString subtitle() const;

private:
    void setupUi(const QString& title, const QString& subtitle, const QString& iconPath);

private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QFrame* m_divider = nullptr;
};

} // namespace TSA::UI
