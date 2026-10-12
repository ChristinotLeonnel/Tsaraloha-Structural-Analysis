#pragma once

// Profils d'affichage enregistrables (docs/DISPLAY_SCALES.md § 7) : un profil est un ensemble nommé de réglages
// GRAPHIQUES de la vue 3D — représentation, couleurs, couches visibles, annotations, échelle des charges, déformée,
// diagrammes, étiquettes, réactions, légende. Liste blanche de clés : aucune valeur du modèle (géométrie, sections,
// matériaux, charges, appuis) ni des résultats ne peut y figurer ; appliquer un profil ne modifie aucune donnée physique.
//
// Stockage : réglages de l'application (QSettings « Display/Profiles »), communs à tous les projets et conservés après
// redémarrage. Profil propre à un document : association chemin du fichier → nom du profil, dans les réglages (le
// format .tsa n'est pas modifié ; anciens fichiers compatibles). Clés inconnues ignorées et signalées, clés absentes
// sans effet (le réglage courant est conservé) : un profil d'une version plus récente ou plus ancienne se charge.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

class OccView;

namespace TSA::Viewer
{

struct DisplayProfile
{
    QString name;
    QJsonObject settings;   ///< clés de displayProfileKeys() seulement
};

/// Clés reconnues (toutes graphiques).
QStringList displayProfileKeys();

/// État d'affichage courant de la vue (toutes les clés reconnues).
QJsonObject captureDisplaySettings(const OccView& view);

/// Applique les clés présentes ; renvoie les clés inconnues (ignorées). Aucune donnée du modèle n'est modifiée.
/// Les clés « annotation* » agissent sur le style d'annotation de l'application (commun à tous les documents).
QStringList applyDisplaySettings(OccView& view, const QJsonObject& settings);

/// Profils fournis : Modélisation, Charges, Maillage, Résultats, Déformées, Analyse modale, Présentation.
QList<DisplayProfile> defaultDisplayProfiles();

/// Profils de l'utilisateur, enregistrés dans les réglages de l'application.
class DisplayProfileStore
{
public:
    /// Groupe QSettings (les tests utilisent un groupe à part pour ne pas toucher aux profils de l'utilisateur).
    explicit DisplayProfileStore(const QString& group = QStringLiteral("Display/Profiles"));

    /// Profils enregistrés ; au premier usage (aucun profil), les profils fournis sont créés.
    QList<DisplayProfile> profiles() const;
    bool contains(const QString& name) const;
    DisplayProfile profile(const QString& name) const;

    /// Crée ou remplace. Nom vide ou trop long : refusé (*error).
    bool save(const DisplayProfile& profile, QString* error = nullptr);
    /// Refusé si l'ancien n'existe pas ou si le nouveau nom est déjà pris.
    bool rename(const QString& oldName, const QString& newName, QString* error = nullptr);
    bool remove(const QString& name);
    /// Remet les profils fournis (les profils créés par l'utilisateur sont conservés).
    void restoreDefaults();

    /// Profil associé à un document (chemin du fichier), vide si aucun.
    QString documentProfile(const QString& filePath) const;
    void setDocumentProfile(const QString& filePath, const QString& name);

private:
    QString m_group;
    void seedIfEmpty() const;
};

} // namespace TSA::Viewer
