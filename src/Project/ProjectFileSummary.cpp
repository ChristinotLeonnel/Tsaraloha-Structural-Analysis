#include "ProjectFileSummary.h"

#include "../IO/TSAFile.h"

namespace TSA::Project
{

ProjectFileSummary readProjectFileSummary(const QString& path)
{
    ProjectFileSummary summary;
    TSA::IO::TSAFileHeader header;
    if (!TSA::IO::TSAFileReader::readHeader(path.toStdString(), header, nullptr)) return summary;
    summary.valid = true;
    summary.projectName = QString::fromUtf8(header.projectName, static_cast<qsizetype>(qstrnlen(header.projectName, sizeof(header.projectName))));
    summary.formatMajor = header.versionMajor;
    summary.formatMinor = header.versionMinor;
    summary.passwordProtected = (header.flags & TSA::IO::FLAG_ENCRYPTED) != 0;
    return summary;
}

} // namespace TSA::Project
