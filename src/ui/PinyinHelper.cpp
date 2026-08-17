#include "ui/PinyinHelper.h"
#include <QChar>
#include <cstdint>

namespace {

#include "PinyinData.inc"

// 获取单个汉字的完整拼音，非汉字返回空指针
const char* pinyinSyllable(const QChar& ch) {
    const uint16_t code = ch.unicode();
    if (code < 0x4E00 || code > 0x9FFF) {
        return nullptr;
    }
    const uint16_t idx = kPinyinIndex[code - 0x4E00];
    if (idx == 0xFFFF || idx >= kPinyinSyllableCount) {
        return nullptr;
    }
    return kPinyinSyllables[idx];
}

// 将中文字符串转为拼音全拼（逐字拼接）
QString buildFullPinyin(const QString& text) {
    if (text.isEmpty()) return {};
    QString result;
    result.reserve(text.size() * 3);
    for (const QChar& ch : text) {
        const char* syl = pinyinSyllable(ch);
        if (syl) {
            result.append(QLatin1String(syl));
        } else {
            result.append(ch.toLower());
        }
    }
    return result;
}

// 递归前缀匹配：needle 是否能匹配 chars 序列的拼音（从任意字开始）
// chars: 待匹配的汉字序列, needle: 用户输入的小写拼音
// 支持跨字匹配，如 "qiaozhi" 匹配 "乔志"（qiao+zhi）
bool prefixMatchPinyin(const QString& text, int textPos, const QString& needle, int needlePos) {
    if (needlePos >= needle.size()) {
        return true;  // needle 已全部匹配完
    }
    if (textPos >= text.size()) {
        return false;  // text 用完但 needle 还没匹配完
    }

    const QChar ch = text.at(textPos);
    const char* syl = pinyinSyllable(ch);

    if (syl) {
        // 汉字：尝试用 needle 剩余部分匹配该字的拼音前缀
        const int sylLen = static_cast<int>(strlen(syl));
        const int remaining = needle.size() - needlePos;
        const int matchLen = qMin(sylLen, remaining);

        // 检查 needle[needlePos .. needlePos+matchLen) 是否等于 syl[0..matchLen)
        bool prefixOk = true;
        for (int i = 0; i < matchLen; ++i) {
            if (needle.at(needlePos + i).toLatin1() != syl[i]) {
                prefixOk = false;
                break;
            }
        }
        if (!prefixOk) {
            return false;
        }

        if (matchLen < sylLen) {
            // needle 在该字拼音中间就用完了，算匹配成功
            return true;
        }
        // 该字拼音完全被消耗，继续匹配下一个字
        return prefixMatchPinyin(text, textPos + 1, needle, needlePos + sylLen);
    } else {
        // 非汉字：逐字符匹配
        if (needle.at(needlePos).toLower() == ch.toLower()) {
            return prefixMatchPinyin(text, textPos + 1, needle, needlePos + 1);
        }
        return false;
    }
}

} // namespace

namespace PinyinHelper {

QString toPinyinInitials(const QString& text) {
    if (text.isEmpty()) return {};
    QString result;
    result.reserve(text.size());
    for (const QChar& ch : text) {
        const char* syl = pinyinSyllable(ch);
        if (syl) {
            result.append(QLatin1Char(syl[0]));
        } else {
            result.append(ch.toLower());
        }
    }
    return result;
}

QString toPinyinFull(const QString& text) {
    return buildFullPinyin(text);
}

bool matchesPinyin(const QString& text, const QString& needle) {
    if (needle.isEmpty()) return true;
    if (text.contains(needle, Qt::CaseInsensitive)) return true;

    const QString lowerNeedle = needle.toLower();

    // 首字母匹配
    const QString initials = toPinyinInitials(text);
    if (initials.contains(lowerNeedle)) return true;

    // 全拼连续匹配（如 "qiaozhitao" 匹配 "乔志桃" 的全拼）
    const QString fullPinyin = toPinyinFull(text);
    if (fullPinyin.contains(lowerNeedle)) return true;

    // 前缀逐字匹配（从任意字开始，支持部分拼音输入如 "qiaozhi"）
    for (int i = 0; i < text.size(); ++i) {
        if (prefixMatchPinyin(text, i, lowerNeedle, 0)) {
            return true;
        }
    }

    return false;
}

} // namespace PinyinHelper