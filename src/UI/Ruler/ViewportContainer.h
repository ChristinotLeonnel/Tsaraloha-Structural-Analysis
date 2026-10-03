#pragma once

#include <QWidget>
#include <memory>

class OccView;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;

namespace TSA::UI {

class HorizontalRulerWidget;
class VerticalRulerWidget;
class CornerWidget;

class ViewportContainer : public QWidget {
  Q_OBJECT

public:
  explicit ViewportContainer(OccView *occView, QWidget *parent = nullptr);
  ~ViewportContainer() override = default;

  OccView *occView() const { return m_occView; }

  void setRulersVisible(bool visible);
  bool areRulersVisible() const { return m_rulersVisible; }

  void setDarkMode(bool dark);
  bool isDarkMode() const { return m_isDarkMode; }

  // Gestion du niveau actif (Dessin en hauteur style Robot SA)
  void updateLevelsList(const std::vector<double> &elevations,
                        const std::vector<std::string> &names = {});
  double activeLevelElevation() const;
  void setActiveLevelIndex(int index);
  void setActiveLevelElevation(double elevation);

  void setMode2D(bool enabled);
  bool isMode2D() const;

signals:
  void activeLevelChanged(double elevation, const QString &name);
  void mode2DChanged(bool active);

public slots:
  void updateRulers();
  void updateTheme(bool isDark);
  void updateMode2DBadge(bool active);

private slots:
  void onMouseMovedInViewport(int px, int py);
  void onCameraChanged();
  void onLevelComboChanged(int index);
  void onLevelUp();
  void onLevelDown();

private:
  void setupUi();

private:
  OccView *m_occView = nullptr;
  CornerWidget *m_corner = nullptr;
  HorizontalRulerWidget *m_topRuler = nullptr;
  VerticalRulerWidget *m_leftRuler = nullptr;
  VerticalRulerWidget *m_rightRuler = nullptr;
  QWidget *m_topCornerRight = nullptr;

  QWidget *m_topBar = nullptr;
  QCheckBox *m_chkMode2D = nullptr;
  QLabel *m_lblMode2DBadge = nullptr;
  QComboBox *m_levelCombo = nullptr;
  QPushButton *m_btnLevelUp = nullptr;
  QPushButton *m_btnLevelDown = nullptr;
  QCheckBox *m_chkSyncWorkPlane = nullptr;

  bool m_rulersVisible = true;
  bool m_isDarkMode = false;
  QLabel *m_lblHint = nullptr;
};

} // namespace TSA::UI
