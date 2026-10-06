#pragma once

// GlobalId IFC (IfcGloballyUniqueId) : GUID de 128 bits encodé en 22 caractères selon
// l'alphabet base64 propre à IFC (« 0-9A-Za-z_$ »). Distinct de l'identifiant interne TSA.

#include <array>
#include <cstdint>
#include <string>

namespace TSA::BIM::IfcGuid
{

/// Nouveau GlobalId aléatoire (UUID v4).
std::string create();
/// Encodage IFC de 16 octets (ordre RFC 4122).
std::string compress(const std::array<std::uint8_t, 16>& bytes);
/// Décodage ; false si la chaîne n'est pas un GlobalId IFC valide.
bool expand(const std::string& globalId, std::array<std::uint8_t, 16>& bytes);
/// 22 caractères de l'alphabet IFC et premier caractère ∈ [0-3].
bool isValid(const std::string& globalId);

} // namespace TSA::BIM::IfcGuid
