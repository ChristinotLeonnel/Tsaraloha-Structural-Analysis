#pragma once

#include <QString>
#include <QColor>
#include <vector>
#include <map>
#include <memory>
#include "../Model/Section.h"
#include "../Model/Material.h"
#include "../Model/Model.h"

namespace TSA::Library
{

struct CustomColor
{
    QString name;
    QString hexCode;
    QString category; // "Général", "Béton", "Acier", "Mes Couleurs"
};

struct CustomTexture
{
    QString id;
    QString name;
    QString category; // "Béton", "Acier", "Bois", "Sol", "Mur", "Mes Textures"
    QString filePath; // Chemin vers image de texture (png/jpg)
    double repeatU = 1.0;
    double repeatV = 1.0;
};

struct StructureTemplate
{
    QString id;
    QString name;
    QString category; // "Portiques", "Fermes", "Escaliers", "Mes Structures"
    QString description;
    TSA::Model::Model::ModelStateSnapshot snapshot;
};

class LibraryManager
{
public:
    static LibraryManager& instance();

    // Initialisation et chargement / sauvegarde
    void initialize();
    void saveAll();

    QString libraryDirectory() const { return m_libraryDir; }

    // --- Sections ---
    const std::vector<TSA::Model::Section>& standardSections() const { return m_standardSections; }
    const std::vector<TSA::Model::Section>& customSections() const { return m_customSections; }
    std::vector<TSA::Model::Section> allSections() const;

    bool addCustomSection(const TSA::Model::Section& section);
    bool removeCustomSection(const std::string& name);
    const TSA::Model::Section* findSectionByName(const std::string& name) const;

    // --- Matériaux ---
    const std::vector<TSA::Model::Material>& standardMaterials() const { return m_standardMaterials; }
    const std::vector<TSA::Model::Material>& customMaterials() const { return m_customMaterials; }
    std::vector<TSA::Model::Material> allMaterials() const;

    bool addCustomMaterial(const TSA::Model::Material& material);
    bool removeCustomMaterial(const std::string& name);
    const TSA::Model::Material* findMaterialByName(const std::string& name) const;

    // --- Couleurs ---
    const std::vector<CustomColor>& colors() const { return m_colors; }
    bool addCustomColor(const QString& name, const QString& hexCode, const QString& category = "Mes Couleurs");
    bool removeCustomColor(const QString& name);
    QColor getColor(const QString& name, const QColor& defaultColor = QColor("#007ACC")) const;

    // --- Textures ---
    const std::vector<CustomTexture>& textures() const { return m_textures; }
    bool addCustomTexture(const QString& name, const QString& filePath, const QString& category = "Mes Textures", double repU = 1.0, double repV = 1.0);
    bool removeCustomTexture(const QString& id);

    // --- Structures Modèles (Templates) ---
    const std::vector<StructureTemplate>& templates() const { return m_templates; }
    bool addStructureTemplate(const QString& name, const QString& category, const QString& description, const TSA::Model::Model::ModelStateSnapshot& snapshot);
    bool removeStructureTemplate(const QString& id);
    bool instantiateTemplateInModel(const QString& templateId, TSA::Model::Model* targetModel, double offsetX = 0.0, double offsetY = 0.0, double offsetZ = 0.0);

private:
    LibraryManager();
    ~LibraryManager() = default;
    LibraryManager(const LibraryManager&) = delete;
    LibraryManager& operator=(const LibraryManager&) = delete;

    void loadSections();
    void saveSections();

    void loadMaterials();
    void saveMaterials();

    void loadColors();
    void saveColors();

    void loadTextures();
    void saveTextures();

    void loadTemplates();
    void saveTemplates();

    void populateDefaultStandardData();

private:
    QString m_libraryDir;

    std::vector<TSA::Model::Section> m_standardSections;
    std::vector<TSA::Model::Section> m_customSections;

    std::vector<TSA::Model::Material> m_standardMaterials;
    std::vector<TSA::Model::Material> m_customMaterials;

    std::vector<CustomColor> m_colors;
    std::vector<CustomTexture> m_textures;
    std::vector<StructureTemplate> m_templates;
};

} // namespace TSA::Library
