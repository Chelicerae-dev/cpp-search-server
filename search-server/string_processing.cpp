#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

std::vector<std::string_view> SplitIntoWords(const std::string_view& str) {
        auto text = str;
        std::vector<std::string_view> result;
        text.remove_prefix(std::min(text.size(), text.find_first_not_of(" ")));
        //int64_t pos = str.find_first_not_of(" ");
        const int64_t pos_end = text.npos;
        int64_t space = text.find(' ');
        while (space != pos_end) {
            result.push_back(space == pos_end ? text : text.substr( 0, space));
            text.remove_prefix(space);
            text.remove_prefix(std::min(text.size(), text.find_first_not_of(" ")));
            space = text.find(' ');
        }
        if(space == pos_end && text.size() != 0) {
            result.push_back(text);
        }

        return result;
}
