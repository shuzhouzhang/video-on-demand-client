// util.h 放一些轻量级工具函数和日志宏。
// 目前主要提供路径取文件名、以及带文件名/行号/函数名的 LOG() 输出。
#ifndef UTIL_H
#define UTIL_H

#include <QDebug>
#include <QFileInfo>
#include <QString>

namespace Util {

// 从文件路径中提取文件名。
// 例如：D:/video/test.mp4 -> test.mp4
inline QString fileNameFromPath(const QString &filePath)
{
    return QFileInfo(filePath).fileName();
}

// 封装日志输出。
// file/function/line 由 LOG() 自动传进来，平时不需要手动填写。
inline QDebug log(const char *file, int line, const char *function)
{
    const QString fileName = QFileInfo(QString::fromLocal8Bit(file)).fileName();
    return qDebug().noquote()
           << QString("[文件: %1 | 行号: %2 | 函数: %3]").arg(fileName).arg(line).arg(function);
}

} // namespace Util

// LOG() 是一个小宏，用来把提示输出到 Qt Creator 底部的“应用程序输出”。
// 用法：LOG() << "输出信息";
#define LOG() Util::log(__FILE__, __LINE__, Q_FUNC_INFO)

#endif // UTIL_H
