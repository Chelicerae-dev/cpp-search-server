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

#include "concurrent_map.h"

    const int MAX_RESULT_DOCUMENT_COUNT = 5;
    //Вводил её в ревью к спринту, в котором она появилась. Толи заготовка из тренажёра в этот раз без неё, толи я что-то
    //не то выгрузил с Github - весь спринт работал в дополнительной ветке репозитория, может где-то что-то не так пошло.
    const double COMPARISSON_PRECISION = 1e-6;

    class SearchServer {

    public:
        template <typename StringContainer>
        explicit SearchServer(const StringContainer& stop_words);
        explicit SearchServer(const std::string& stop_words_text);
        explicit SearchServer(const std::string_view& stop_words_text);

        void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

        //Не знаю как ещё визуально их сгруппировать, когда несколько тимплейтов подряд - читать неудобно всё равно
        template <class ExecutionPolicy, typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

        template <class ExecutionPolicy>
        std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const;
        std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

        template <class ExecutionPolicy>
        std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const;
        std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

        int GetDocumentCount() const;

        std::set<int>::iterator begin() const;
        std::set<int>::iterator end() const;

        const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

        //См. комментарии к объявлению FindAllDocuments, пожалуйста.
        void RemoveDocument(std::execution::parallel_policy, int document_id);
        void RemoveDocument(std::execution::sequenced_policy, int document_id);
        void RemoveDocument(int document_id);

        //См. комментарии к объявлению FindAllDocuments, пожалуйста.
        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const;
        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const;
        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

    private:
        struct DocumentData {
            int rating;
            DocumentStatus status;
        };
        const std::set<std::string> stop_words_;
        std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
        std::map<int, DocumentData> documents_;
        std::map<int, std::map<std::string_view, double>> documents_by_id_; //{ Ид документа, {слово, TF}}
        std::set<int> document_ids_;

        bool IsStopWord(const std::string& word) const;

        static bool IsValidWord(const std::string_view& word);

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
            std::vector<std::string> plus_words;
            std::vector<std::string> minus_words;
        };

        Query ParseQuery(const std::string& text, bool sort_required = false) const;
        Query ParseQuery(std::string_view text, bool sort_required = false) const;

        // Existence required
        double ComputeWordInverseDocumentFreq(const std::string& word) const;

        //Проверка через type_id, очевидно, медленнее, чем поиск нужной перегрузки, но при этом помогает избежать копирования
        //либо я что-то не так понял, ведь два этих метода практически одинаковые в реализации.
        //А значение по умолчанию нельзя поставить из-за заданного формата (очерёдности) принимаемых аргументов
        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const;
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

    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(policy, query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < COMPARISSON_PRECISION) {
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

    template <class ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const {
        return SearchServer::FindTopDocuments(policy, raw_query, [status](int, DocumentStatus document_status, int) {
            return document_status == status;
        });
    }

    template <class ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const {
        return SearchServer::FindTopDocuments(policy, raw_query, [](int, DocumentStatus status, int) {
           return status == DocumentStatus::ACTUAL;
        });
    }

    template<typename DocumentPredicate>
    std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
        return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_to_relevance(50);
        std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),[this, &document_predicate, &document_to_relevance](const std::string& word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return ;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if(document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        });
        std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](const std::string& word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        });

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_to_relevance(50);
        std::for_each(std::execution::seq, query.plus_words.begin(), query.plus_words.end(),[this, &document_predicate, &document_to_relevance](const std::string& word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return ;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if(document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        });
        std::for_each(std::execution::seq, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](const std::string& word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        });

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
    }


