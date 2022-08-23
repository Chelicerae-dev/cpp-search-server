#pragma once
#include "document.h"
#include "search_server.h"
#include <vector>
#include <deque>
#include <algorithm>
#include <utility>

using std::literals::string_literals::operator""s;

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) 
    : server_(search_server) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result = server_.FindTopDocuments(raw_query, document_predicate);
        if(requests_.size() >= min_in_day_) {
            requests_.pop_front();
        }
        requests_.push_back({result.size()});
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    const SearchServer& server_;
    struct QueryResult {
        size_t size;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
}; 
