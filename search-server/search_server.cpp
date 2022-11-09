#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
{
}
SearchServer::SearchServer(const std::string_view& stop_words_text)
        : SearchServer(SplitIntoWords(std::string(stop_words_text)))
{
}

void SearchServer::AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        using std::literals::string_literals::operator""s;
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(std::string(document));

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        std::string_view word_sv = std::string_view (word_to_document_freqs_.find(word)->first);
        documents_by_id_[document_id][word_sv] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});

    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> empty_map = {}; //Заглушка для возвращения ссылки на пустой мап
    if(documents_by_id_.count(document_id) != 0) {
        return documents_by_id_.at(document_id);
    }
    return empty_map;
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    if(SearchServer::GetDocumentCount() == 0 || document_ids_.count(document_id) == 0) {
        return;
    }
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    std::vector<std::pair<std::string_view, double>> words(documents_by_id_.at(document_id).size());
    std::transform(std::execution::par, documents_by_id_.at(document_id).begin(), documents_by_id_.at(document_id).end(),
                   words.begin(), [](const auto& value) {
                return value;
            });
    std::unique(std::execution::par, words.begin(), words.end());
    std::list<std::string_view> temp_words;
    std::for_each(std::execution::par, words.begin(), words.end(), [this, document_id, &temp_words](const auto &value) {
        this->word_to_document_freqs_.at(std::string(value.first)).erase(document_id);
        if (this->word_to_document_freqs_.at(std::string(value.first)).size() == 0) {
            temp_words.push_back(value.first);
        }
    });
    documents_by_id_.erase(document_id);

    if (!temp_words.empty()) {
        std::for_each(std::execution::par, temp_words.begin(), temp_words.end(), [this](const std::string_view& word) {
            this->word_to_document_freqs_.erase(std::string(word));
        });
    }
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    if(SearchServer::GetDocumentCount() == 0 || document_ids_.count(document_id) == 0) {
        return;
    }
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

void SearchServer::RemoveDocument(int document_id) {
    SearchServer::RemoveDocument(std::execution::seq, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const {
    if(document_ids_.count(document_id) == 0) {
        using namespace std::literals;
        throw std::out_of_range("No such id"s);
    }
    std::vector<std::string_view> words = SplitIntoWords(raw_query);
    std::vector<std::string_view> plus_words, minus_words;
    plus_words.reserve(words.size());
    minus_words.reserve(words.size());
    std::for_each(words.begin(), words.end(), [&plus_words, &minus_words](auto& word){
        if (word.empty()) {
            throw std::invalid_argument("Query word is empty");
        }
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            using namespace std::literals;
            throw std::invalid_argument("Query word "s + std::string(word) + " is invalid"s);
        }
        if(is_minus) {
            minus_words.push_back(word);
        } else {
            plus_words.push_back(word);
        }
    });

    if(std::any_of(std::execution::par, minus_words.begin(), minus_words.end(), [this, document_id](const auto& word){
        auto pos = word_to_document_freqs_.find(word);
        return pos != word_to_document_freqs_.end() && pos->second.count(document_id) != 0;
    })) {
        return {std::vector<std::string_view>{}, documents_.at(document_id).status};
    }
    std::sort(std::execution::par, plus_words.begin(), plus_words.end());
    auto plus_last = std::unique(std::execution::par, plus_words.begin(), plus_words.end());
    plus_words.erase(plus_last, plus_words.end());
    std::vector<std::string_view> matched_words(plus_words.size());


    auto last = std::copy_if(std::execution::par, plus_words.begin(), plus_last, matched_words.begin(), [document_id, this](const auto& word){
        auto pos = word_to_document_freqs_.find(word);
        return pos != word_to_document_freqs_.end() && pos->second.count(document_id) != 0;
    });

    matched_words.erase(last, matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const {
    if(document_ids_.count(document_id) == 0) {
        using namespace std::literals;
        throw std::out_of_range("No such id"s);
    }
    const auto query = ParseQuery(raw_query, true);

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

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view& raw_query, int document_id) const {
    return SearchServer::MatchDocument(std::execution::seq, raw_query, document_id);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            using std::literals::string_literals::operator""s;
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
    using std::literals::string_literals::operator""s;
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view & text) const {
    return SearchServer::ParseQueryWord(std::string(text));
}
SearchServer::Query SearchServer::ParseQuery(const std::string& text, bool sort_required) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
//                result.minus_words.insert(query_word.data);
                result.minus_words.push_back(query_word.data);
            } else {
//                result.plus_words.insert(query_word.data);
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if(sort_required) {
        std::sort(std::execution::par, result.plus_words.begin(), result.plus_words.end());
        std::sort(std::execution::par, result.minus_words.begin(), result.minus_words.end());
    }
    auto plus_last = std::unique(std::execution::par, result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(plus_last, result.plus_words.end());
    auto minus_last = std::unique(std::execution::par, result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(minus_last, result.minus_words.end());
    return result;
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool sort_required) const {
    return SearchServer::ParseQuery(std::string(text), sort_required);
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const std::string_view& document, DocumentStatus status,
                 const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const std::invalid_argument& e) {
        using std::literals::string_literals::operator""s;
        std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string_view& raw_query) {
    using std::literals::string_literals::operator""s;
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка поиска: "s << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string_view& query) {
    using std::literals::string_literals::operator""s;
    try {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = *next(search_server.begin(), index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}