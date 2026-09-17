#include "tool.h"

QString Tool::typeName() const
{
    switch (type) {
    case ToolType::Service:     return QStringLiteral("service");
    case ToolType::WebApp:      return QStringLiteral("webapp");
    case ToolType::DesktopApp:  return QStringLiteral("desktop");
    case ToolType::AndroidApp:  return QStringLiteral("android");
    case ToolType::Document:    return QStringLiteral("document");
    }
    return QStringLiteral("service");
}

QString Tool::typeLabel() const
{
    switch (type) {
    case ToolType::Service:     return QStringLiteral("服务");
    case ToolType::WebApp:      return QStringLiteral("网页应用");
    case ToolType::DesktopApp:  return QStringLiteral("桌面应用");
    case ToolType::AndroidApp:  return QStringLiteral("Android 应用");
    case ToolType::Document:    return QStringLiteral("文档");
    }
    return QStringLiteral("服务");
}