#pragma once

#include <map>
#include <vector>
#include <mutex>


using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : count_(bucket_count) {
        m_ = std::vector<std::mutex>(count_);
        content_.resize(count_);
    }

    Access operator[](const Key& key) {
        auto target = static_cast<uint64_t>(key) % count_;
        return { std::lock_guard(m_[target]), content_[target][key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for(size_t i = 0; i < count_; ++i) {
            m_[i].lock();
            result.merge(content_[i]);
            m_[i].unlock();
        }
        return result;
    }

    void erase(const Key& key){
        auto target = static_cast<uint64_t>(key) % count_;
        m_[target].lock();
        content_[target].erase(key);
        m_[target].unlock();
    }

private:
    size_t count_;
    std::vector<std::mutex> m_;
    std::vector<std::map<Key, Value>> content_;
};

//template <typename Value>
class ConcurrentStringSet {
public:
    struct Access {
        std::lock_guard<std::mutex> guard;
        std::string& ref_to_value;
    };

    explicit ConcurrentStringSet(size_t bucket_count) : count_(bucket_count) {
        m_ = std::vector<std::mutex>(count_);
        content_.resize(count_);
    }

//    Access operator[](const std::string& value) {
//        auto target = static_cast<uint64_t>(value[0]) % count_;
//        return { std::lock_guard(m_[target]), content_[target][value]};
//    }

    void insert(const std::string& value) {
        auto target = static_cast<uint64_t>(value[0]) % count_;
        m_[target].lock();
        content_[target].insert(value);
        m_[target].unlock();

    }

    std::set<std::string, std::less<>> BuildOrdinarySet() {
        std::set<std::string, std::less<>> result;
        for(size_t i = 0; i < count_; ++i) {
            m_[i].lock();
            result.merge(content_[i]);
            m_[i].unlock();
        }
        return result;
    }

private:
    size_t count_;
    std::vector<std::mutex> m_;
    std::vector<std::set<std::string, std::less<>>> content_;
};