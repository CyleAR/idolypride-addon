#include "Misc.hpp"

#include <codecvt>
#include <locale>
#include <jni.h>


extern JavaVM* g_javaVM;


namespace HoshimiLocal::Misc {
    std::u16string ToUTF16(const std::string_view& str) {
        std::u16string result;
        result.reserve(str.size());
        for (size_t i = 0; i < str.size(); ) {
            uint32_t cp = 0;
            unsigned char c = (unsigned char)str[i++];
            if (c < 0x80) cp = c;
            else if (c < 0xE0) {
                if (i < str.size()) cp = ((c & 0x1F) << 6) | ((unsigned char)str[i++] & 0x3F);
            }
            else if (c < 0xF0) {
                if (i + 1 < str.size()) {
                    cp = ((c & 0x0F) << 12) | (((unsigned char)str[i++] & 0x3F) << 6);
                    cp |= ((unsigned char)str[i++] & 0x3F);
                }
            }
            else {
                if (i + 2 < str.size()) {
                    cp = ((c & 0x07) << 18) | (((unsigned char)str[i++] & 0x3F) << 12);
                    cp |= (((unsigned char)str[i++] & 0x3F) << 6);
                    cp |= ((unsigned char)str[i++] & 0x3F);
                }
            }

            if (cp < 0x10000) {
                result.push_back((char16_t)cp);
            }
            else {
                cp -= 0x10000;
                result.push_back((char16_t)((cp >> 10) | 0xD800));
                result.push_back((char16_t)((cp & 0x3FF) | 0xDC00));
            }
        }
        return result;
    }

    std::string ToUTF8(const std::u16string_view& str) {
        std::string result;
        result.reserve(str.size() * 3);
        for (size_t i = 0; i < str.size(); ++i) {
            uint32_t cp = str[i];
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < str.size()) {
                uint32_t low = str[++i];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    cp = ((cp - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
                }
            }

            if (cp < 0x80) result.push_back((char)cp);
            else if (cp < 0x800) {
                result.push_back((char)((cp >> 6) | 0xC0));
                result.push_back((char)((cp & 0x3F) | 0x80));
            }
            else if (cp < 0x10000) {
                result.push_back((char)((cp >> 12) | 0xE0));
                result.push_back((char)(((cp >> 6) & 0x3F) | 0x80));
                result.push_back((char)((cp & 0x3F) | 0x80));
            }
            else {
                result.push_back((char)((cp >> 18) | 0xF0));
                result.push_back((char)(((cp >> 12) & 0x3F) | 0x80));
                result.push_back((char)(((cp >> 6) & 0x3F) | 0x80));
                result.push_back((char)((cp & 0x3F) | 0x80));
            }
        }
        return result;
    }

    JNIEnv* GetJNIEnv() {
        if (!g_javaVM) return nullptr;
        JNIEnv* env = nullptr;
        if (g_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            int status = g_javaVM->AttachCurrentThread(&env, nullptr);
            if (status < 0) {
                return nullptr;
            }
        }
        return env;
    }

    CSEnum::CSEnum(const std::string& name, const int value) {
        this->Add(name, value);
    }

    CSEnum::CSEnum(const std::vector<std::string>& names, const std::vector<int>& values) {
        if (names.size() != values.size()) return;
        this->names = names;
        this->values = values;
    }

    int CSEnum::GetIndex() {
        return currIndex;
    }

    void CSEnum::SetIndex(int index) {
        if (index < 0) return;
        if (index + 1 >= values.size()) return;
        currIndex = index;
    }

    int CSEnum::GetTotalLength() {
        return values.size();
    }

    void CSEnum::Add(const std::string &name, const int value) {
        this->names.push_back(name);
        this->values.push_back(value);
    }

    std::pair<std::string, int> CSEnum::GetCurrent() {
        return std::make_pair(names[currIndex], values[currIndex]);
    }

    std::pair<std::string, int> CSEnum::Last() {
        const auto maxIndex = this->GetTotalLength() - 1;
        if (currIndex <= 0) {
            currIndex = maxIndex;
        }
        else {
            currIndex--;
        }
        return this->GetCurrent();
    }

    std::pair<std::string, int> CSEnum::Next() {
        const auto maxIndex = this->GetTotalLength() - 1;
        if (currIndex >= maxIndex) {
            currIndex = 0;
        }
        else {
            currIndex++;
        }
        return this->GetCurrent();
    }

    int CSEnum::GetValueByName(const std::string &name) {
        for (int i = 0; i < names.size(); i++) {
            if (names[i].compare(name) == 0) {
                return values[i];
            }
        }
        return values[0];
    }

    namespace StringFormat {
        std::pair<std::string, std::string> split_once(const std::string& str, const std::string& delimiter) {
            size_t pos = str.find(delimiter);
            if (pos == std::string::npos) {
                return {str, ""};
            }
            return {str.substr(0, pos), str.substr(pos + delimiter.length())};
        }
    }

}
