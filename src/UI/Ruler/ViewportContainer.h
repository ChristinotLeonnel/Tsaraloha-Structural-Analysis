#pragma once

#include <QWidget>
#include <memory>

class OccView;
class QLabel;
class QPushButton;
class QComboBox;

namespace TSA::UI
{

class HorizontalRulerWidget;
class VerticalRulerWidget;
class CornerWidget;

class ViewportContainer : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportContainer(OccView* occView, QWidget* parent = nullptr);
    ~ViewportContainer() override = default;

    OccView* occView() const { return m_occView; }

    void setRulersVisible(bool visible);
    bool areRulersVisible() const { return m_rulersVisible; }

    // Gestion du niveau actif (Dessin en hauteur style Robot SA)
    void updateLevelsList(const std::vector<double>& elevations, const std::vector<std::string>& names = {});
    double activeLevelElevation() const;
    void setActiveLevelIndex(int index);

signals:
    void activeLevelChanged(double elevation, const QString& name);

public slots:
    void updateRulers();
    void updateTheme(bool isDark);

private slots:
    void onMouseMovedInViewport(int px, int py);
    void onCameraChanged();
    void onLevelComboChanged(int index);
    void onLevelUp();
    void onLevelDown();

private:
    void setupUi();

private:
    OccView* m_occView = nullptr;
    CornerWidget* m_corner = nullptr;
    HorizontalRulerWidget* m_topRuler = nullptr;
    VerticalRulerWidget* m_leftRuler = nullptr;
    VerticalRulerWidget* m_rightRuler = nullptr;
    QWidget* m_topCornerRight = nullptr;

    QWidget* m_topBar = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QPushButton* m_btnLevelUp = nullptr;
    QPushButton* m_btnLevelDown = nullptr;

    bool m_rulersVisible = true;
};

} // namespace TSA::UI
