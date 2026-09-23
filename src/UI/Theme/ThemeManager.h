#pragma once

#include <QString>

namespace TSA::UI
{

class ThemeManager
{
public:
    static QString darkStyleSheet();
    static QString lightStyleSheet();

    static QString topBarDarkStyle();
    static QString topBarLightStyle();

    static QString bottomBarDarkStyle();
    static QString bottomBarLightStyle();

    static bool isDarkMode();
    static void setDarkMode(bool dark);

private:
    static bool s_darkMode;
};

} // namespace TSA::UI
