#pragma once
// Frontières d'exception (BUG-155) : texte de l'exception en cours, à appeler dans un bloc catch (...). Une exception qui
// sortait d'un corps de QThread::create appelait std::terminate (fermeture brutale), et celle d'un calcul traversait l'interface.
// OCCT 8 : Standard_Failure dérive de std::exception (what() renseigné) ; std::bad_alloc : mémoire insuffisante.
#include <exception>
#include <new>
#include <string>
#include <typeinfo>

namespace TSA::Core
{

inline std::string currentExceptionText()
{
    try
    {
        throw;
    }
    catch (const std::bad_alloc&)
    {
        return "mémoire insuffisante (std::bad_alloc)";
    }
    catch (const std::exception& e)
    {
        const std::string what = e.what() ? e.what() : "";
        return what.empty() ? std::string(typeid(e).name()) : what;
    }
    catch (...)
    {
        return "exception d'un type inconnu";
    }
}

} // namespace TSA::Core
