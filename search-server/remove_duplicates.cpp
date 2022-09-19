#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> duplicates;
    std::set<std::set<std::string>> words_to_documents;
    for(int document_id : search_server) {
        std::set<std::string> words;
        for(const auto& [word, stats] : search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }
        if(words_to_documents.count(words) == 0) {
            words_to_documents.insert(words);
        } else {
            duplicates.insert(document_id);
        }
    }
    for(int id : duplicates) {
        using namespace std::literals;
        std::cout << "Found duplicate document id "s << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
