#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
        std::vector<Document> result = server_.FindTopDocuments(raw_query, status);
        if(requests_.size() >= min_in_day_) {
            requests_.pop_front();
        }
        requests_.push_back({result.size()});
        return result;
    }

    std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
        std::vector<Document> result = server_.FindTopDocuments(raw_query);
        if(requests_.size() >= min_in_day_) {
            requests_.pop_front();
        }
        requests_.push_back({result.size()});
        return result;
    }

    int RequestQueue::GetNoResultRequests() const {
        return std::count_if(requests_.begin(), requests_.end(), [](const QueryResult& res) {
            return res.size == 0;
        });
    }