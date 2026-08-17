// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增12行/修改0行/删除0行; 总行数12行
// @AI-LastModified: 2026-04-15 19:39:07

#pragma once

#include <QString>

namespace PinyinHelper {

/// 将中文字符串转为拼音全拼（小写字母，声母+韵母）。
/// 非中文字符保持原样。多音字取最常用读音。
QString toPinyinFull(const QString& text);

/// 将中文字符串转为拼音首字母（每个汉字取声母首字母）。
/// 非中文字符保持原样。
QString toPinyinInitials(const QString& text);

/// 检查 needle 是否匹配 text 的拼音（首字母或全拼子串匹配）。
bool matchesPinyin(const QString& text, const QString& needle);

} // namespace PinyinHelper
