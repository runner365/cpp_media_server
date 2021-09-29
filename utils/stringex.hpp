#ifndef STRING_EXTEN_HPP
#define STRING_EXTEN_HPP

#include <random>
#include <string>
#include <stdint.h>
#include <stddef.h>

inline int string_split(const std::string& input_str, const std::string& split_str, std::vector<std::string>& output_vec) {
    if (input_str.length() == 0) {
        return 0;
    }
    
    std::string tempString(input_str);
    do {

        size_t pos = tempString.find(split_str);
        if (pos == tempString.npos) {
            output_vec.push_back(tempString);
            break;
        }
        std::string seg_str = tempString.substr(0, pos);
        tempString = tempString.substr(pos+split_str.size());
        output_vec.push_back(seg_str);
    } while(tempString.size() > 0);

    return output_vec.size();
}

inline std::string get_random_string(size_t len)
{
    const size_t MAX_LEN = 256;
    char buffer[MAX_LEN];
    static const char chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };
    std::default_random_engine random;

    if (len > MAX_LEN) {
        len = MAX_LEN;
    }
    for (size_t i{ 0 }; i < len; ++i)
    {
        size_t rand_number = random()%sizeof(chars);
        buffer[i] = chars[rand_number];
    }
    return std::string(buffer, len);
}

#endif //STRING_EXTEN_HPP