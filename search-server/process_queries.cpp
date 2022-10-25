#include "process_queries.h"


std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    //RequestQueue queue(search_server);
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const std::string& str) {
        return search_server.FindTopDocuments(str);
    });
    return result;
}

std::list<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::list<Document> result;
    auto temp = ProcessQueries(search_server, queries);
    for(const auto& vect : temp) {
        for(const Document& doc : vect) {
            result.push_back(doc);
        }
    }
    return result;
}