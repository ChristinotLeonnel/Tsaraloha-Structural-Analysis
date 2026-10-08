#pragma once

// Identifiants de l'extension Explorateur (miniatures) du produit TSA, partagés par la DLL
// (src/ShellExtension) et ses tests. Chaque produit a les siens : TSA et TSALab coexistent sur le
// poste, chacun propriétaire de son extension de fichier.

#include <guiddef.h>

namespace TSA::Product::Shell
{

// {5BA6698A-ED79-442D-92EE-31DA2704C07D}
inline constexpr CLSID kThumbnailProviderClsid = { 0x5ba6698a, 0xed79, 0x442d, { 0x92, 0xee, 0x31, 0xda, 0x27, 0x04, 0xc0, 0x7d } };
inline constexpr wchar_t kThumbnailProviderClsidString[] = L"{5BA6698A-ED79-442D-92EE-31DA2704C07D}";
inline constexpr wchar_t kThumbnailProviderName[] = L"TSA Thumbnail Provider";
inline constexpr wchar_t kExtension[] = L".tsa";
inline constexpr wchar_t kProgId[] = L"TSA.Project";

} // namespace TSA::Product::Shell
