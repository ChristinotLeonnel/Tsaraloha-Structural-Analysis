#include "ReportConfiguration.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate>

namespace TSA::NDC
{

QJsonObject ReportConfiguration::toJson() const
{
    QJsonObject root;

    // 1. Informations projet
    QJsonObject infoObj;
    infoObj["projectTitle"] = projectTitle;
    infoObj["projectDescription"] = projectDescription;
    infoObj["projectNumber"] = projectNumber;
    infoObj["documentNumber"] = documentNumber;
    infoObj["revision"] = revision;
    infoObj["documentStatus"] = documentStatus;
    infoObj["engineerName"] = engineerName;
    infoObj["organization"] = organization;
    infoObj["clientName"] = clientName;
    infoObj["organizationAddress"] = organizationAddress;
    infoObj["contactEmail"] = contactEmail;
    infoObj["contactPhone"] = contactPhone;
    infoObj["emissionDate"] = emissionDate;
    root["projectInfo"] = infoObj;

    // 2. Style & Apparence
    QJsonObject styleObj;
    styleObj["showTsaLogo"] = showTsaLogo;
    styleObj["customLogoPath"] = customLogoPath;
    styleObj["secondaryLogoPath"] = secondaryLogoPath;
    styleObj["primaryColor"] = primaryColor;
    styleObj["fontFamily"] = fontFamily;
    styleObj["baseFontSizePt"] = baseFontSizePt;
    styleObj["pageFormat"] = (pageFormat == PageFormat::A3) ? "A3" : "A4";
    styleObj["pageOrientation"] = (pageOrientation == PageOrientation::Landscape) ? "Landscape" : "Portrait";
    styleObj["marginMmLeft"] = marginMmLeft;
    styleObj["marginMmRight"] = marginMmRight;
    styleObj["marginMmTop"] = marginMmTop;
    styleObj["marginMmBottom"] = marginMmBottom;
    styleObj["enableHeader"] = enableHeader;
    styleObj["customHeaderText"] = customHeaderText;
    styleObj["enableFooter"] = enableFooter;
    styleObj["enablePagination"] = enablePagination;
    styleObj["templateId"] = templateId;
    root["style"] = styleObj;

    // 3. Sélection des chapitres
    QJsonObject chObj;
    chObj["includeCoverPage"] = includeCoverPage;
    chObj["includeToc"] = includeToc;
    chObj["includeLof"] = includeLof;
    chObj["includeLot"] = includeLot;
    chObj["includeIntroduction"] = includeIntroduction;
    chObj["includeStandards"] = includeStandards;
    chObj["includeModelGeometry"] = includeModelGeometry;
    chObj["includeMaterials"] = includeMaterials;
    chObj["includeSections"] = includeSections;
    chObj["includeBoundaryConditions"] = includeBoundaryConditions;
    chObj["includeLoadsAndCombinations"] = includeLoadsAndCombinations;
    chObj["includeCalculationMethod"] = includeCalculationMethod;
    chObj["includeModelVerification"] = includeModelVerification;
    chObj["include3DModelSnapshots"] = include3DModelSnapshots;
    chObj["includeBendingMoment"] = includeBendingMoment;
    chObj["includeShearForce"] = includeShearForce;
    chObj["includeAxialForce"] = includeAxialForce;
    chObj["includeTorsion"] = includeTorsion;
    chObj["includeDisplacements"] = includeDisplacements;
    chObj["includeDeflections"] = includeDeflections;
    chObj["includeReactions"] = includeReactions;
    chObj["includeMostStressedSummary"] = includeMostStressedSummary;
    chObj["includeExtremaSpatialTable"] = includeExtremaSpatialTable;
    chObj["includeEnvelopes"] = includeEnvelopes;
    chObj["includeDetailedElementTables"] = includeDetailedElementTables;
    chObj["includeEurocodeDesignChecks"] = includeEurocodeDesignChecks;
    chObj["includeWarningsAndLimitations"] = includeWarningsAndLimitations;
    chObj["includeConclusion"] = includeConclusion;
    chObj["includeBibliography"] = includeBibliography;
    root["chapters"] = chObj;

    // 4. Captures 3D
    QJsonObject snapObj;
    snapObj["snapshotWidth"] = snapshotWidth;
    snapObj["snapshotHeight"] = snapshotHeight;
    snapObj["deformationScaleFactor"] = deformationScaleFactor;
    snapObj["show3DNodes"] = show3DNodes;
    snapObj["show3DLoads"] = show3DLoads;
    snapObj["show3DAxes"] = show3DAxes;
    root["snapshots"] = snapObj;

    return root;
}

void ReportConfiguration::fromJson(const QJsonObject& json)
{
    if (json.contains("projectInfo") && json["projectInfo"].isObject())
    {
        QJsonObject info = json["projectInfo"].toObject();
        if (info.contains("projectTitle")) projectTitle = info["projectTitle"].toString();
        if (info.contains("projectDescription")) projectDescription = info["projectDescription"].toString();
        if (info.contains("projectNumber")) projectNumber = info["projectNumber"].toString();
        if (info.contains("documentNumber")) documentNumber = info["documentNumber"].toString();
        if (info.contains("revision")) revision = info["revision"].toString();
        if (info.contains("documentStatus")) documentStatus = info["documentStatus"].toString();
        if (info.contains("engineerName")) engineerName = info["engineerName"].toString();
        if (info.contains("organization")) organization = info["organization"].toString();
        if (info.contains("clientName")) clientName = info["clientName"].toString();
        if (info.contains("organizationAddress")) organizationAddress = info["organizationAddress"].toString();
        if (info.contains("contactEmail")) contactEmail = info["contactEmail"].toString();
        if (info.contains("contactPhone")) contactPhone = info["contactPhone"].toString();
        if (info.contains("emissionDate")) emissionDate = info["emissionDate"].toString();
    }

    if (json.contains("style") && json["style"].isObject())
    {
        QJsonObject st = json["style"].toObject();
        if (st.contains("showTsaLogo")) showTsaLogo = st["showTsaLogo"].toBool();
        if (st.contains("customLogoPath")) customLogoPath = st["customLogoPath"].toString();
        if (st.contains("secondaryLogoPath")) secondaryLogoPath = st["secondaryLogoPath"].toString();
        if (st.contains("primaryColor")) primaryColor = st["primaryColor"].toString();
        if (st.contains("fontFamily")) fontFamily = st["fontFamily"].toString();
        if (st.contains("baseFontSizePt")) baseFontSizePt = st["baseFontSizePt"].toInt();
        if (st.contains("pageFormat"))
        {
            pageFormat = (st["pageFormat"].toString() == "A3") ? PageFormat::A3 : PageFormat::A4;
        }
        if (st.contains("pageOrientation"))
        {
            pageOrientation = (st["pageOrientation"].toString() == "Landscape") ? PageOrientation::Landscape : PageOrientation::Portrait;
        }
        if (st.contains("marginMmLeft")) marginMmLeft = st["marginMmLeft"].toDouble();
        if (st.contains("marginMmRight")) marginMmRight = st["marginMmRight"].toDouble();
        if (st.contains("marginMmTop")) marginMmTop = st["marginMmTop"].toDouble();
        if (st.contains("marginMmBottom")) marginMmBottom = st["marginMmBottom"].toDouble();
        if (st.contains("enableHeader")) enableHeader = st["enableHeader"].toBool();
        if (st.contains("customHeaderText")) customHeaderText = st["customHeaderText"].toString();
        if (st.contains("enableFooter")) enableFooter = st["enableFooter"].toBool();
        if (st.contains("enablePagination")) enablePagination = st["enablePagination"].toBool();
        if (st.contains("templateId")) templateId = st["templateId"].toString();
    }

    if (json.contains("chapters") && json["chapters"].isObject())
    {
        QJsonObject ch = json["chapters"].toObject();
        if (ch.contains("includeCoverPage")) includeCoverPage = ch["includeCoverPage"].toBool();
        if (ch.contains("includeToc")) includeToc = ch["includeToc"].toBool();
        if (ch.contains("includeLof")) includeLof = ch["includeLof"].toBool();
        if (ch.contains("includeLot")) includeLot = ch["includeLot"].toBool();
        if (ch.contains("includeIntroduction")) includeIntroduction = ch["includeIntroduction"].toBool();
        if (ch.contains("includeStandards")) includeStandards = ch["includeStandards"].toBool();
        if (ch.contains("includeModelGeometry")) includeModelGeometry = ch["includeModelGeometry"].toBool();
        if (ch.contains("includeMaterials")) includeMaterials = ch["includeMaterials"].toBool();
        if (ch.contains("includeSections")) includeSections = ch["includeSections"].toBool();
        if (ch.contains("includeBoundaryConditions")) includeBoundaryConditions = ch["includeBoundaryConditions"].toBool();
        if (ch.contains("includeLoadsAndCombinations")) includeLoadsAndCombinations = ch["includeLoadsAndCombinations"].toBool();
        if (ch.contains("includeCalculationMethod")) includeCalculationMethod = ch["includeCalculationMethod"].toBool();
        if (ch.contains("includeModelVerification")) includeModelVerification = ch["includeModelVerification"].toBool();
        if (ch.contains("include3DModelSnapshots")) include3DModelSnapshots = ch["include3DModelSnapshots"].toBool();
        if (ch.contains("includeBendingMoment")) includeBendingMoment = ch["includeBendingMoment"].toBool();
        if (ch.contains("includeShearForce")) includeShearForce = ch["includeShearForce"].toBool();
        if (ch.contains("includeAxialForce")) includeAxialForce = ch["includeAxialForce"].toBool();
        if (ch.contains("includeTorsion")) includeTorsion = ch["includeTorsion"].toBool();
        if (ch.contains("includeDisplacements")) includeDisplacements = ch["includeDisplacements"].toBool();
        if (ch.contains("includeDeflections")) includeDeflections = ch["includeDeflections"].toBool();
        if (ch.contains("includeReactions")) includeReactions = ch["includeReactions"].toBool();
        if (ch.contains("includeMostStressedSummary")) includeMostStressedSummary = ch["includeMostStressedSummary"].toBool();
        if (ch.contains("includeExtremaSpatialTable")) includeExtremaSpatialTable = ch["includeExtremaSpatialTable"].toBool();
        if (ch.contains("includeEnvelopes")) includeEnvelopes = ch["includeEnvelopes"].toBool();
        if (ch.contains("includeDetailedElementTables")) includeDetailedElementTables = ch["includeDetailedElementTables"].toBool();
        if (ch.contains("includeEurocodeDesignChecks")) includeEurocodeDesignChecks = ch["includeEurocodeDesignChecks"].toBool();
        if (ch.contains("includeWarningsAndLimitations")) includeWarningsAndLimitations = ch["includeWarningsAndLimitations"].toBool();
        if (ch.contains("includeConclusion")) includeConclusion = ch["includeConclusion"].toBool();
        if (ch.contains("includeBibliography")) includeBibliography = ch["includeBibliography"].toBool();
    }

    if (json.contains("snapshots") && json["snapshots"].isObject())
    {
        QJsonObject snap = json["snapshots"].toObject();
        if (snap.contains("snapshotWidth")) snapshotWidth = snap["snapshotWidth"].toInt();
        if (snap.contains("snapshotHeight")) snapshotHeight = snap["snapshotHeight"].toInt();
        if (snap.contains("deformationScaleFactor")) deformationScaleFactor = snap["deformationScaleFactor"].toDouble();
        if (snap.contains("show3DNodes")) show3DNodes = snap["show3DNodes"].toBool();
        if (snap.contains("show3DLoads")) show3DLoads = snap["show3DLoads"].toBool();
        if (snap.contains("show3DAxes")) show3DAxes = snap["show3DAxes"].toBool();
    }
}

bool ReportConfiguration::saveToFile(const QString& filePath, QString* error) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (error) *error = QString("Impossible d'écrire dans le fichier : %1").arg(file.errorString());
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ReportConfiguration::loadFromFile(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error) *error = QString("Impossible de lire le fichier : %1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();

    if (parseErr.error != QJsonParseError::NoError)
    {
        if (error) *error = QString("Erreur de syntaxe JSON : %1").arg(parseErr.errorString());
        return false;
    }

    if (!doc.isObject())
    {
        if (error) *error = "Format de configuration invalide (objet racine requis).";
        return false;
    }

    fromJson(doc.object());
    return true;
}

} // namespace TSA::NDC
