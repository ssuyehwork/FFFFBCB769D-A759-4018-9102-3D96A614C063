#ifndef SVGICON_H
#define SVGICON_H

#include <QPixmap>
#include <QSize>
#include <QColor>
#include <QMap>
#include <QString>

class SvgIcon {
public:
    enum IconType {
        Lock,           // 锁
        Unlock,         // 解锁
        Eye,            // 显示密码
        EyeOff,         // 隐藏密码
        Warning,        // 警告（请勿操作）
        Settings,       // 设置齿轮
        Clock,          // 时钟
        Shield,         // 应用主图标
        Tray,           // 托盘图标
        Log,            // 日志
        Close,          // 关闭
        Play,           // 开始/继续
        Pause,          // 暂停
    };

    // 返回指定颜色和尺寸的 QPixmap
    static QPixmap get(IconType type, QSize size, QColor color = Qt::white);

private:
    static const QMap<IconType, QString> svgTemplates;
};

#endif // SVGICON_H
