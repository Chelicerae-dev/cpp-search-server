#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
        return RequestQueue::AddFindRequest(raw_query, [status](int id, DocumentStatus status_filter, int rating) {
            return status_filter == status;
        });
    }

    std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
        return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    }

    int RequestQueue::GetNoResultRequests() const {
        return std::count_if(requests_.begin(), requests_.end(), [](const QueryResult& res) {
            return res.size == 0;
        });
    }

    