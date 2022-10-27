#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <stdexcept>
#include <set>
#include <cmath>
#include <algorithm>
#include <utility>
#include "document.h"
#include "string_processing.h"
#include <numeric>
#include <execution>
#include <typeinfo>
#include <list>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view& stop_words_text);


//    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;
//    template <typename DocumentPredicate>
//    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
//    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
//    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    int GetDocumentCount() const;

    //Deprecated
    //int GetDocumentId(int index) const;

    std::set<int>::iterator begin() const;
    std::set<int>::iterator end() const;

    //const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template<class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

    template<class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::map<int, std::map<std::string_view, double>> documents_by_id_; //{ Ид документа, {слово, TF}}
    std::set<int> document_ids_;

    bool IsStopWord(const std::string& word) const;

    static bool IsValidWord(const std::string& word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string& text) const;
    QueryWord ParseQueryWord(const std::string_view& text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;
    Query ParseQuery(const std::string_view& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
};

void AddDocument(SearchServer& search_server, int document_id, const std::string_view& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view& query);

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    using std::literals::string_literals::operator""s; //не подумал что можно использовать внутри метода
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}
//template <typename DocumentPredicate>
//std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
//    return SearchServer::FindTopDocuments(std::string_view(raw_query), document_predicate);
//}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if(SearchServer::GetDocumentCount() == 0 || document_ids_.count(document_id) == 0) {
        return;
    }
    //В задании указано сохранить поведение, поэтому разделяем в зависимости от полиси
    if(typeid(policy).name() == typeid(std::execution::par).name()) {
        documents_.erase(document_id);
        document_ids_.erase(document_id);
        std::vector<std::pair<std::string_view, double>> words(documents_by_id_.at(document_id).size());
        std::transform(policy, documents_by_id_.at(document_id).begin(), documents_by_id_.at(document_id).end(),
                       words.begin(), [](const auto& value) {
                    return value;
                });
        std::unique(policy, words.begin(), words.end());
        std::list<std::string_view> temp_words;
        std::for_each(policy, words.begin(), words.end(), [this, document_id, &temp_words](const auto &value) {
            this->word_to_document_freqs_.at(std::string(value.first)).erase(document_id);
            if (this->word_to_document_freqs_.at(std::string(value.first)).size() == 0) {
                temp_words.push_back(value.first);
            }
        });
        documents_by_id_.erase(document_id);

        if (!temp_words.empty()) {
            std::for_each(policy, temp_words.begin(), temp_words.end(), [this](const std::string_view& word) {
                this->word_to_document_freqs_.erase(std::string(word));
            });
        }
    } else {
        //В задании указано сохранить поведение, поэтому старая реализация
        document_ids_.erase(document_id);
        std::set<std::string_view> words;
        for(const auto& [word, stats] : documents_by_id_.at(document_id)) {
            words.insert(word);
        }
        documents_by_id_.erase(document_id);
        documents_.erase(document_id);
        std::vector<std::string> temp;
        for(auto& [word, stats] : word_to_document_freqs_) {
            auto id_iter = stats.find(document_id);
            if(id_iter != stats.end()) {
                stats.erase(id_iter);
                if(stats.empty()) {
                    temp.push_back(word);
                }
            }
        }
        if(!temp.empty()) {
            for(const std::string& word : temp) {
                word_to_document_freqs_.erase(word);
            }
        }
    }
}

template<class ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, const std::string_view& raw_query, int document_id) const {
    if(document_ids_.count(document_id) == 0) {
        using namespace std::literals;
        throw std::out_of_range("No such id"s);
    }
    //В задании указано сохранить поведение, поэтому разделяем в зависимости от полиси
    if(typeid(policy).name() == typeid(std::execution::par).name()) {
        std::vector<std::string> plus_words, minus_words;
        std::vector<std::string_view> words = SplitIntoWords(raw_query);
        for(const std::string_view word : words) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    minus_words.push_back(query_word.data);
                } else {
                    plus_words.push_back(query_word.data);
                }
            }
        }
        if(std::any_of(policy, minus_words.begin(), minus_words.end(), [this, document_id](const auto& word){
            auto pos = word_to_document_freqs_.find(word);
            return pos != word_to_document_freqs_.end() && pos->second.count(document_id) != 0;
        })) {
            return {std::vector<std::string_view>{}, documents_.at(document_id).status};
        }
        std::sort(policy, plus_words.begin(), plus_words.end());
        auto plus_last = std::unique(plus_words.begin(), plus_words.end());
        std::vector<std::string_view> matched_words(plus_words.size());
        auto last = std::copy_if(policy, plus_words.begin(), plus_last, matched_words.begin(), [document_id, this](const auto& word){
            auto pos = word_to_document_freqs_.find(word);
            return pos != word_to_document_freqs_.end() && pos->second.count(document_id) != 0;
        });
        matched_words.erase(last, matched_words.end());
        return {matched_words, documents_.at(document_id).status};
    } else {
        const auto query = ParseQuery(raw_query);

        std::vector<std::string_view> matched_words;
        for (const std::string& word: query.minus_words) {
            if (word_to_document_freqs_.at(word).count(document_id) != 0) {
                return {std::vector<std::string_view>{}, documents_.at(document_id).status};
            }
        }
        for (const std::string& word: query.plus_words) {
            auto pos = word_to_document_freqs_.find(word);
            if (pos->second.count(document_id) != 0) {
                matched_words.push_back(pos->first);
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
}