#include "InstalledCatalogs.h"

#include <utility>

namespace TSA::Model
{

namespace
{
InstalledCatalogs& storage()
{
    static InstalledCatalogs catalogs;
    return catalogs;
}
} // namespace

void setInstalledCatalogs(InstalledCatalogs catalogs)
{
    storage() = std::move(catalogs);
}

const InstalledCatalogs& installedCatalogs()
{
    return storage();
}

} // namespace TSA::Model
