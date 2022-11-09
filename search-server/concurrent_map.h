#pragma once

#include <map>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
    struct Content {
        std::mutex mutex;
        std::map<Key, Value> data;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : content_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto target = static_cast<uint64_t>(key) % content_.size();
        return { std::lock_guard(content_[target].mutex), content_[target].data[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for(size_t i = 0; i < content_.size(); ++i) {
            content_[i].mutex.lock();
            result.insert(content_[i].data.begin(), content_[i].data.end());
            content_[i].mutex.unlock();
        }
        return result;
    }

    void erase(const Key& key){
        auto target = static_cast<uint64_t>(key) % content_.size();
        content_[target].mutex.lock();
        content_[target].data.erase(key);
        content_[target].mutex.unlock();
    }

private:
    std::vector<Content> content_;
};

