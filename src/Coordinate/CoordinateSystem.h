#pragma once

#include <QObject>
#include "Point3D.h"
#include "LevelManager.h"
#include <vector>
#include <string>
#include <memory>

namespace TSA::Model
{
class Model;
}

namespace TSA::Coordinate
{

enum class WorkPlaneAxis
{
    Z = 0, // Horizontal XY (planchers / niveaux)
    X = 1, // Vertical YZ (coupes transversales)
    Y = 2  // Vertical XZ (coupes longitudinales)
};

struct DetectedPlaneInfo
{
    WorkPlaneAxis axis = WorkPlaneAxis::Z;
    double offset = 0.0;
    std::string name; // ex: "R+1 (Z = 3.00 m)", "Axe 2 (X = 5.00 m)", etc.
    std::string id;   // identifiant unique
};

class CoordinateSystem : public QObject
{
    Q_OBJECT

public:
    explicit CoordinateSystem(QObject* parent = nullptr);
    ~CoordinateSystem() override = default;

    // Détection automatique des plans structurels selon l'axe X, Y ou Z
    std::vector<DetectedPlaneInfo> detectStructuralPlanes(WorkPlaneAxis axis, const TSA::Model::Model* model = nullptr) const;

    // --- Coordonnées X ---
    const std::vector<double>& xPositions() const { return m_xPositions; }
    size_t xCount() const { return m_xPositions.size(); }
    void setXPositions(const std::vector<double>& positions);
    void setXSpacings(double startX, const std::vector<double>& spacings);
    std::vector<double> getXSpacings() const;

    // --- Coordonnées Y ---
    const std::vector<double>& yPositions() const { return m_yPositions; }
    size_t yCount() const { return m_yPositions.size(); }
    void setYPositions(const std::vector<double>& positions);
    void setYSpacings(double startY, const std::vector<double>& spacings);
    std::vector<double> getYSpacings() const;

    // --- Coordonnées Z (Gérées de manière centrale via LevelManager) ---
    std::vector<double> zLevels() const;
    size_t zCount() const;
    void setZLevels(const std::vector<double>& elevations);
    void setZSpacings(double startZ, const std::vector<double>& spacings);
    std::vector<double> getZSpacings() const;

    LevelManager* levelManager() { return m_levelManager.get(); }
    const LevelManager* levelManager() const { return m_levelManager.get(); }

    // --- Point exact d'intersection 3D (X[ix], Y[iy], Z[iz]) ---
    Point3D gridPoint(int ix, int iy, int iz) const;

    // --- Libellés d'axes ---
    const std::vector<std::string>& xLabels() const { return m_xLabels; }
    const std::vector<std::string>& yLabels() const { return m_yLabels; }
    void setXLabels(const std::vector<std::string>& labels);
    void setYLabels(const std::vector<std::string>& labels);

    std::string getXLabel(size_t index) const;
    std::string getYLabel(size_t index) const;
    std::string getZLabel(size_t index) const;

    // --- Recherche et Accrochage Géométrique (Snapping) ---
    bool findClosestX(double x, double tol, double& outX, int& outIdx) const;
    bool findClosestY(double y, double tol, double& outY, int& outIdx) const;
    bool findClosestZ(double z, double tol, double& outZ, int& outIdx) const;
    bool findClosestIntersection(const Point3D& rawPnt, double tol, Point3D& outPnt, int& outIx, int& outIy, int& outIz) const;

    // Réinitialisation aux valeurs par défaut
    void setDefaultBuildingCoordinates();

    // Sérialisation
    std::string serializeToJson() const;
    void deserializeFromJson(const std::string& json);

signals:
    void coordinatesChanged();

private:
    void synchronizeLabels();
    static std::string indexToLetter(size_t index);

private:
    std::vector<double> m_xPositions;
    std::vector<double> m_yPositions;
    std::shared_ptr<LevelManager> m_levelManager;

    std::vector<std::string> m_xLabels;
    std::vector<std::string> m_yLabels;
};

} // namespace TSA::Coordinate
