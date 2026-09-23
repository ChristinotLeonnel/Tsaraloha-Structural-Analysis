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

    void setDarkMode(bool dark);
    bool isDarkMode() const { return m_isDarkMode; }

    // Gestion du niveau actif (Dessin en hauteur style Robot SA)
    void updateLevelsList(const std::vector<double>& elevations, const std::vector<std::string>& names = {});
    double activeLevelElevation() const;
    void setActiveLevelIndex(int index);

signals:
    void activeLevelChanged(double elevation, const QString& name);

public slots:
    void updateRulers();

private slots:
    void onMouseMovedInViewport(int px, int py);
    void onCameraChanged();
    void onMouseCoordsChanged(double x, double y, double z);
    void onViewFront();
    void onViewTop();
    void onViewIso();
    void onWorkplaneClicked();
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

    QWidget* m_topBar = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QPushButton* m_btnLevelUp = nullptr;
    QPushButton* m_btnLevelDown = nullptr;

    QWidget* m_bottomBar = nullptr;
    QPushButton* m_btnPlane = nullptr;
    QLabel* m_lblElevation = nullptr;
    QPushButton* m_btnViewFront = nullptr;
    QPushButton* m_btnViewTop = nullptr;
    QPushButton* m_btnViewIso = nullptr;
    QLabel* m_lblCoords = nullptr;

    bool m_rulersVisible = true;
    bool m_isDarkMode = false;
    QWidget* m_topCornerRight = nullptr;
    QLabel* m_lblHint = nullptr;
};

} // namespace TSA::UI
