#pragma once

#include <QWidget>
#include <memory>
#include <vector>
#include <string>
#include "../../Coordinate/CoordinateSystem.h"

class OccView;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;

namespace TSA::Model {
class Model;
}

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

  void setModel(TSA::Model::Model *model);
  TSA::Model::Model *model() const { return m_model; }

  // Gestion du niveau actif (Dessin en hauteur style Robot SA) - Compatibilité Z
  void updateLevelsList(const std::vector<double> &elevations,
                        const std::vector<std::string> &names = {});
  double activeLevelElevation() const;
  void setActiveLevelIndex(int index);
  void setActiveLevelElevation(double elevation);

  // Direction / Axe et Plans X, Y, Z
  TSA::Coordinate::WorkPlaneAxis currentAxis() const { return m_currentAxis; }
  void setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis axis);
  void setActivePlane(TSA::Coordinate::WorkPlaneAxis axis, double offset);
  void refreshAvailablePlanes();

  void setMode2D(bool enabled);
  bool isMode2D() const;

signals:
  void activeLevelChanged(double elevation, const QString &name);
  void activeWorkPlaneChanged(TSA::Coordinate::WorkPlaneAxis axis, double offset, const QString &name);
  void mode2DChanged(bool active);

public slots:
  void updateRulers();
  void updateTheme(bool isDark);
  void updateMode2DBadge(bool active);

private slots:
  void onMouseMovedInViewport(int px, int py);
  void onCameraChanged();
  void onAxisComboChanged(int index);
  void onPlanComboChanged(int index);
  void onPlanUp();
  void onPlanDown();

private:
  void setupUi();
  void populatePlansForCurrentAxis();
  void updateQuickButtonsState();

private:
  OccView *m_occView = nullptr;
  TSA::Model::Model *m_model = nullptr;
  CornerWidget *m_corner = nullptr;
  HorizontalRulerWidget *m_topRuler = nullptr;
  VerticalRulerWidget *m_leftRuler = nullptr;
  VerticalRulerWidget *m_rightRuler = nullptr;
  QWidget *m_topCornerRight = nullptr;

  QWidget *m_topBar = nullptr;
  QLabel *m_lblAxis = nullptr;
  QComboBox *m_axisCombo = nullptr;
  QPushButton *m_btnQuickZ = nullptr;
  QPushButton *m_btnQuickX = nullptr;
  QPushButton *m_btnQuickY = nullptr;
  QLabel *m_lblPlan = nullptr;
  QComboBox *m_planCombo = nullptr;
  QPushButton *m_btnPlanUp = nullptr;
  QPushButton *m_btnPlanDown = nullptr;
  QCheckBox *m_chkSyncWorkPlane = nullptr;
  QCheckBox *m_chkMode2D = nullptr;
  QLabel *m_lblMode2DBadge = nullptr;
  QLabel *m_lblHint = nullptr;

  TSA::Coordinate::WorkPlaneAxis m_currentAxis = TSA::Coordinate::WorkPlaneAxis::Z;
  /// Garde de réentrance : vrai pendant que onPlanComboChanged() pousse le plan vers OccView.
  /// OccView réémet alors workPlaneChanged -> MainWindow::onWorkPlaneChanged -> setActiveLevelElevation(),
  /// qui rappelait onPlanComboChanged() : boucle infinie et débordement de pile au démarrage.
  bool m_isApplyingPlane = false;
  std::vector<TSA::Coordinate::DetectedPlaneInfo> m_currentPlanes;

  // Données héritées pour compatibilité updateLevelsList
  std::vector<double> m_legacyElevations;
  std::vector<std::string> m_legacyNames;

  bool m_rulersVisible = true;
  bool m_isDarkMode = false;
};

} // namespace TSA::UI
