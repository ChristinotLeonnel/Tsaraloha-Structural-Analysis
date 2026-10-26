#include "DefinitionReference.h"

#include <QRegularExpression>
#include <QString>

#include <cmath>

namespace TSA::Model
{

std::optional<SemanticVersion> SemanticVersion::fromString(const std::string& str)
{
    if (str.empty()) return std::nullopt;

    // Expression régulière SemVer 2.0 simplifiée : Major.Minor[.Patch][-Prerelease]
    static const QRegularExpression regex(R"(^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:-(.+))?$)");
    QRegularExpressionMatch match = regex.match(QString::fromStdString(str));
    if (!match.hasMatch())
    {
        return std::nullopt;
    }

    SemanticVersion v;
    v.major = match.captured(1).toInt();
    v.minor = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    v.patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    v.prerelease = match.captured(4).toStdString();
    return v;
}

std::string SemanticVersion::toString() const
{
    std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty())
    {
        s += "-" + prerelease;
    }
    return s;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const
{
    return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const
{
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;

    // Prerelease vide = version finale > version prerelease (ex: 1.0.0 > 1.0.0-rc1)
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    return prerelease < other.prerelease;
}

bool MechanicalSnapshot::operator==(const MechanicalSnapshot& other) const
{
    auto approxEq = [](double a, double b) {
        return std::abs(a - b) <= 1e-6 * (std::abs(a) + std::abs(b) + 1.0);
    };
    return approxEq(youngModulus, other.youngModulus) &&
           approxEq(poissonRatio, other.poissonRatio) &&
           approxEq(density, other.density) &&
           approxEq(characteristicStrength, other.characteristicStrength) &&
           approxEq(yieldStrength, other.yieldStrength) &&
           approxEq(thermalCoeff, other.thermalCoeff);
}

} // namespace TSA::Model
