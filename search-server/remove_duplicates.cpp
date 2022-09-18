#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> duplicates;
    for(auto iter = search_server.words_by_id_.begin(); iter != search_server.words_by_id_.end(); ++iter) {
        auto pos = std::find_if(next(iter), search_server.words_by_id_.end(), [iter](const std::pair<int, std::set<std::string>>& value) {
            return iter->second == value.second;
        });
        if(pos != search_server.words_by_id_.end()) {
            duplicates.insert(pos->first);
        }
    }
    for(int id : duplicates) {
        using namespace std::literals;
        std::cout << "Found duplicate document id "s << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
