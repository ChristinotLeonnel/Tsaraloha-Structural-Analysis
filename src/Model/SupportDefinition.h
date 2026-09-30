#pragma once

#include <string>

namespace TSA::Model
{

// État d'un Degré de Liberté (DDL)
enum class DOFState
{
    Free,       // Libre de se déplacer / tourner
    Fixed,      // Bloqué / Encastré rigide
    Spring      // Élastique (Ressort avec raideur K)
};

// Type d'orientation de l'appui
enum class SupportOrientationType
{
    Global,         // Repère cartésien global X, Y, Z
    LocalBar,       // Repère local de la barre connectée
    CustomVector    // Direction ou normale personnalisée
};

// Types d'appuis prédéfinis standards
enum class SupportType
{
    Free,       // Libre (aucun DDL bloqué)
    Fixed,      // Encastrement (Tx=Ty=Tz=Rx=Ry=Rz bloqués)
    Pinned,     // Articulation / Rotule (Tx=Ty=Tz bloqués, rotations libres)
    Roller,     // Appui simple / Rouleau (Tz bloqué ou direction personnalisée)
    Sliding,    // Appui glissant (translation axiale libre, autres bloqués)
    Linear,     // Appui guidé linéaire
    Planar,     // Appui plan (glissement dans le plan)
    Elastic,    // Appui élastique (ressorts)
    Custom      // Configuration personnalisée par l'utilisateur
};

/**
 * @brief Définition complète d'un appui structurel 3D à 6 degrés de liberté.
 * Gère les états de translation (UX, UY, UZ), de rotation (RX, RY, RZ),
 * les constantes de raideur élastique et l'orientation géométrique.
 */
class SupportDefinition
{
public:
    SupportDefinition() = default;

    SupportDefinition(
        DOFState tx, DOFState ty, DOFState tz,
        DOFState rx, DOFState ry, DOFState rz,
        double kx = 0.0, double ky = 0.0, double kz = 0.0,
        double krx = 0.0, double kry = 0.0, double krz = 0.0,
        SupportOrientationType orient = SupportOrientationType::Global,
        double dirX = 0.0, double dirY = 0.0, double dirZ = 1.0,
        int refElemId = 0);

    // --- Accesseurs DDLs ---
    DOFState tx() const { return m_tx; }
    DOFState ty() const { return m_ty; }
    DOFState tz() const { return m_tz; }
    DOFState rx() const { return m_rx; }
    DOFState ry() const { return m_ry; }
    DOFState rz() const { return m_rz; }

    void setTx(DOFState s) { m_tx = s; }
    void setTy(DOFState s) { m_ty = s; }
    void setTz(DOFState s) { m_tz = s; }
    void setRx(DOFState s) { m_rx = s; }
    void setRy(DOFState s) { m_ry = s; }
    void setRz(DOFState s) { m_rz = s; }

    void setDOFs(DOFState tx, DOFState ty, DOFState tz, DOFState rx, DOFState ry, DOFState rz)
    {
        m_tx = tx; m_ty = ty; m_tz = tz;
        m_rx = rx; m_ry = ry; m_rz = rz;
    }

    // --- Raideurs élastiques (kN/m en translation, kNm/rad en rotation) ---
    double kx() const { return m_kx; }
    double ky() const { return m_ky; }
    double kz() const { return m_kz; }
    double krx() const { return m_krx; }
    double kry() const { return m_kry; }
    double krz() const { return m_krz; }

    void setKx(double v) { m_kx = v; }
    void setKy(double v) { m_ky = v; }
    void setKz(double v) { m_kz = v; }
    void setKrx(double v) { m_krx = v; }
    void setKry(double v) { m_kry = v; }
    void setKrz(double v) { m_krz = v; }

    void setStiffnesses(double kx, double ky, double kz, double krx = 0.0, double kry = 0.0, double krz = 0.0)
    {
        m_kx = kx; m_ky = ky; m_kz = kz;
        m_krx = krx; m_kry = kry; m_krz = krz;
    }

    // --- Orientation ---
    SupportOrientationType orientationType() const { return m_orientation; }
    void setOrientationType(SupportOrientationType t) { m_orientation = t; }

    double customDirX() const { return m_customDirX; }
    double customDirY() const { return m_customDirY; }
    double customDirZ() const { return m_customDirZ; }
    void setCustomDirection(double x, double y, double z)
    {
        m_customDirX = x; m_customDirY = y; m_customDirZ = z;
    }

    int referenceElementId() const { return m_refElemId; }
    void setReferenceElementId(int id) { m_refElemId = id; }

    // --- Usines statiques (Presets) ---
    static SupportDefinition free();
    static SupportDefinition fixed();
    static SupportDefinition pinned();
    static SupportDefinition roller(double dirX = 0.0, double dirY = 0.0, double dirZ = 1.0);
    static SupportDefinition sliding(double dirX = 1.0, double dirY = 0.0, double dirZ = 0.0);
    static SupportDefinition linear(double axisX = 1.0, double axisY = 0.0, double axisZ = 0.0);
    static SupportDefinition planar(double normX = 0.0, double normY = 0.0, double normZ = 1.0);
    static SupportDefinition elastic(double kx, double ky, double kz, double krx = 0.0, double kry = 0.0, double krz = 0.0);
    static SupportDefinition custom(DOFState tx, DOFState ty, DOFState tz, DOFState rx, DOFState ry, DOFState rz);

    // --- Prédicats & Helpers ---
    bool isFree() const;
    bool isFixed() const;
    bool isPinned() const;
    bool isRoller() const;
    bool hasSprings() const;

    SupportType supportType() const;
    std::string typeName() const;
    std::string dofSummary() const;

    // Compatibilité descendante
    SupportType toLegacySupportType() const;
    static SupportDefinition fromLegacySupportType(SupportType type);

    // Opérateurs
    bool operator==(const SupportDefinition& o) const;
    bool operator!=(const SupportDefinition& o) const { return !(*this == o); }

private:
    DOFState m_tx = DOFState::Free;
    DOFState m_ty = DOFState::Free;
    DOFState m_tz = DOFState::Free;
    DOFState m_rx = DOFState::Free;
    DOFState m_ry = DOFState::Free;
    DOFState m_rz = DOFState::Free;

    double m_kx = 0.0;
    double m_ky = 0.0;
    double m_kz = 0.0;
    double m_krx = 0.0;
    double m_kry = 0.0;
    double m_krz = 0.0;

    SupportOrientationType m_orientation = SupportOrientationType::Global;
    double m_customDirX = 0.0;
    double m_customDirY = 0.0;
    double m_customDirZ = 1.0;
    int m_refElemId = 0;
};

} // namespace TSA::Model
