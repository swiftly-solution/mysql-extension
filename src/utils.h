#ifndef _utils_h
#define _utils_h

#include <string>
#include <vector>
#include <sstream>

std::vector<std::string> explode(std::string str, std::string delimiter);

template<typename T>
std::string implode(std::vector<T> elements, std::string delimiter)
{
    std::ostringstream oss;
    for (size_t i = 0; i < elements.size(); ++i) {
        oss << elements[i];
        if (i != elements.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

template <typename... Args>
std::string string_format(const std::string &format, Args... args)
{
    int size_s = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0)
        return "";

    size_t size = static_cast<size_t>(size_s);
    char* buf = new char[size];
    snprintf(buf, size, format.c_str(), args...);
    std::string out = std::string(buf, buf + size - 1); // We don't want the '\0' inside
    delete buf;
    return out;
}

#endif