#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> duplicates;
    std::map<std::set<std::string>, std::set<int>> words_to_documents;
    for(int document_id : search_server) {
        std::set<std::string> words;
        for(const auto& [word, stats] : search_server.documents_by_id_.at(document_id)) {
            words.insert(word);
        }
        words_to_documents[words].insert(document_id);
    }
    for(const auto& [words, ids] : words_to_documents) {
        if(ids.size() > 1) {
            std::for_each(next(ids.begin()), ids.end(), [&duplicates](int id) {
                duplicates.insert(id);
            });
        }
    }
    for(int id : duplicates) {
        using namespace std::literals;
        std::cout << "Found duplicate document id "s << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
