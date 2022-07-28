//#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <numeric>

using namespace std;

//Класс SearchServer и необходимые для него функции, структуры и константы

    const int MAX_RESULT_DOCUMENT_COUNT = 5;
    const double COMPARISON_PRECISION = 1e-6; //Постарался дать "говорящее" имя

    string ReadLine() {
        string s;
        getline(cin, s);
        return s;
    }

    int ReadLineWithNumber() {
        int result;
        cin >> result;
        ReadLine();
        return result;
    }

    vector<string> SplitIntoWords(const string& text) {
        vector<string> words;
        string word;
        for (const char c : text) {
            if (c == ' ') {
                if (!word.empty()) {
                    words.push_back(word);
                    word.clear();
                }
            } else {
                word += c;
            }
        }
        if (!word.empty()) {
            words.push_back(word);
        }

        return words;
    }
        
    struct Document {
        int id;
        double relevance;
        int rating;
    };

    enum class DocumentStatus {
        ACTUAL,
        IRRELEVANT,
        BANNED,
        REMOVED,
    };

    class SearchServer {
    public:
        void SetStopWords(const string& text) {
            for (const string& word : SplitIntoWords(text)) {
                stop_words_.insert(word);
            }
        }    
        
        void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
            const vector<string> words = SplitIntoWordsNoStop(document);
            const double inv_word_count = 1.0 / words.size();
            for (const string& word : words) {
                word_to_document_freqs_[word][document_id] += inv_word_count;
            }
            documents_.emplace(document_id, 
                DocumentData{
                    ComputeAverageRating(ratings), 
                    status
                });
        }

        template <typename FilterPredicate> //Конкретизировал в имени шаблона и далее в имени аргумента для чего служит этот предикат
        vector<Document> FindTopDocuments(const string& raw_query, FilterPredicate filter_predicate) const {            
            const Query query = ParseQuery(raw_query);
            vector<Document> matched_documents = FindAllDocuments(query, filter_predicate);
            sort(matched_documents.begin(), matched_documents.end(),
                [](const Document& lhs, const Document& rhs) {
                        if (abs(lhs.relevance - rhs.relevance) < COMPARISON_PRECISION) {
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

        //FindTopDocuments with args: query, status
        vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {            
            return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus stat, int rating) {
                document_id += 1;
                rating += 1;
                return stat == status;
            });
        }

        //FindTopDocuments with args: query (status = ACTUAL by default)
        vector<Document> FindTopDocuments(const string& raw_query) const {  
            return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) {
                document_id += 1;
                rating += 1;
                return status == DocumentStatus::ACTUAL;
            });
        }

        int GetDocumentCount() const {
            return documents_.size();
        }
        
        tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
            const Query query = ParseQuery(raw_query);
            vector<string> matched_words;
            for (const string& word : query.plus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.push_back(word);
                }
            }
            for (const string& word : query.minus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                    break;
                }
            }
            return {matched_words, documents_.at(document_id).status};
        }
        
    private:
        struct DocumentData {
            int rating;
            DocumentStatus status;
        };

        set<string> stop_words_;
        map<string, map<int, double>> word_to_document_freqs_;
        map<int, DocumentData> documents_;

        bool IsStopWord(const string& word) const {
            return stop_words_.count(word) > 0;
        }
        
        vector<string> SplitIntoWordsNoStop(const string& text) const {
            vector<string> words;
            for (const string& word : SplitIntoWords(text)) {
                if (!IsStopWord(word)) {
                    words.push_back(word);
                }
            }
            return words;
        }
        
        static int ComputeAverageRating(const vector<int>& ratings) {
            if (ratings.empty()) {
                return 0;
            }
            int rating_sum = accumulate(ratings.begin(), ratings.end(), 0); //Вместо перебора в for
            return rating_sum / static_cast<int>(ratings.size());
        }
        
        struct QueryWord {
            string data;
            bool is_minus;
            bool is_stop;
        };
        
        QueryWord ParseQueryWord(string text) const {
            bool is_minus = false;
            // Word shouldn't be empty
            if (text[0] == '-') {
                is_minus = true;
                text = text.substr(1);
            }
            return {
                text,
                is_minus,
                IsStopWord(text)
            };
        }
        
        struct Query {
            set<string> plus_words;
            set<string> minus_words;
        };
        
        Query ParseQuery(const string& text) const {
            Query query;
            for (const string& word : SplitIntoWords(text)) {
                const QueryWord query_word = ParseQueryWord(word);
                if (!query_word.is_stop) {
                    if (query_word.is_minus) {
                        query.minus_words.insert(query_word.data);
                    } else {
                        query.plus_words.insert(query_word.data);
                    }
                }
            }
            return query;
        }
        
        // Existence required
        double ComputeWordInverseDocumentFreq(const string& word) const {
            return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
        }

        template <typename FilterPredicate> //Конкретизировал в имени шаблона и далее в имени аргумента для чего служит этот предикат (аналогично методу FindTopDocuments)
        vector<Document> FindAllDocuments(const Query& query, FilterPredicate filter_predicate) const {
            map<int, double> document_to_relevance;
            for (const string& word : query.plus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& current_document_data = documents_.at(document_id); //DocumentData заменен на const auto&
                    if(filter_predicate(document_id, current_document_data.status, current_document_data.rating)) {
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
            }
            
            for (const string& word : query.minus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(document_id);
                }
            }

            vector<Document> matched_documents;
            for (const auto [document_id, relevance] : document_to_relevance) {
                   
                    matched_documents.push_back({
                        document_id,
                        relevance,
                        documents_.at(document_id).rating
                });
            }
            return matched_documents;
        }

    };

// -------- Начало модульных тестов поисковой системы ----------

//Переопределяем стандартный вывод для массивов

//Переопределяем вывод пар
template <typename Key, typename Value>
ostream& operator<<(ostream& out, const pair<Key, Value>& element) {
    out <<  element.first << ": "s << element.second;
    return out;
}

//Вывод Document
ostream& operator<<(ostream& out, const Document& document) {
    out << '|' << document.id << ", "s << document.relevance << ", "s << document.rating << '|';
    return out; 
}
//Шаблонная функция для вывода шаблонного контейнера в стандартный выход
template <typename Container>
ostream& Print(ostream& out, const Container& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if(!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
    return out;
}

//Вывод вектора
template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << '[';
    Print(out, container);
    out << ']';
    return out;
}
 
//Вывод сета
template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

//Вывод мапа
template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

//Вывод DocumentStatus
ostream& operator<<(ostream& out, const DocumentStatus status) {
    switch(status) {
        case DocumentStatus::ACTUAL:
            out << "ACTUAL"s;
            break;
        case DocumentStatus::BANNED:
            out << "BANNED"s;
            break;
        case DocumentStatus::IRRELEVANT:
            out << "IRRELEVANT"s;
            break;
        case DocumentStatus::REMOVED:
            out << "REMOVED"s;
            break;
    }
    return out;
}
//Функция теста для сравнения 2 аргументов
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

//Макросы для теста-сравнения 2 аргументов
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

//Функция теста для булева значения
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

//Макросы для теста булева значения
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

//Функция для вызова функций тестирования и вывода ответа после успешного завершения теста
template <typename function>
void RunTestImpl(function f, const string& function_name) {
    f();
    cerr << function_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func) 



/*Функции тестирования*/
//Тест проверяет добавление документов в сервер
void TestAddDocument() {
    const int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    //Проверяем что до добавления документов нет и что после добавления их количество увеличилось.
    {
        SearchServer server;
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0, "Server must have no documents after initialization"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1u, "AddDocument() works incorrect"s);
        server.AddDocument(doc_id +1, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2u, "AddDocument() works incorrect"s);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}


//Тест проверяет работу поисковой системы с минус-словами
void TestMinusWords() {
    const int document_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    //Проверяем, ищется ли документ по плюс-словам
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, document_id);
    }

    //Теперь проверяем что поиск с минус-словом возвращает пустой результат
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);      
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Documents with minus words must be excluded from result"s);
    }
    
    //Проверяем что по запросам с минус-словами находятся документы без минус-слов
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 1, "cat food is delicious"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Search with minus words is incorrect"s);
        ASSERT_EQUAL_HINT(found_docs[0].id, 2, "Search with minus words is incorrect"s);
    }
}

//Тест проверяет работу матчинга
void TestMatching() {
    const string content = "cat in the city eats cat food and does other stuff cat do"s;
    const string stop_words = "in the and"s;
    
    //Проверяем наличие всех слов из запроса, входящих в документ
    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::BANNED, {1, 2, 3});
        const string query = "cat food"s;
        const vector<string> expected_words = {"cat"s, "food"s};
        const auto [matched_words, status] = server.MatchDocument(query, 1);
        ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching words from document is incorrect"s);
        ASSERT_EQUAL_HINT(status, DocumentStatus::BANNED, "Status from matching is incorrect"s);
    }
    
    //Проверяем матчинг при нахождении в документе минус-слова из запроса.
    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, {1, 2, 3});
        const string query = "cat food -city";
        const auto [matched_words, status] = server.MatchDocument(query, 1);
        vector<string> expected_words;
        expected_words.clear();
        ASSERT_EQUAL_HINT(matched_words, expected_words, "Minus words not processed correctly in MatchDocuments()"s);
    }

    //Проверяем матчинг со стоп-словами
    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, {1, 2, 3});
        server.SetStopWords(stop_words);
        //Проверяем запрос со стоп-словами и не стоп-словами
        {
            const string query = "cat in the food"s;
            const auto [matched_words, status] = server.MatchDocument(query, 1);
            const vector<string> expected_words = {"cat"s, "food"s};
            ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching with stop words is incorrect"s);
        }
        //Проверяем запрос только из стоп слов
        {
            const string query = "and in"s;
            const auto [matched_words, status] = server.MatchDocument(query, 1);
            vector<string> expected_words;
            expected_words.clear();
            ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching document with stop words only is incorrect"s);
        }
    }
}   

//Тест проверяет калькуляцию рейтинга
void TestRating() {
    const int doc_id = 1;
    const string content = "cat in the city"s;
    //Проверяем для чётной суммы рейтингов
    {
        const vector<int> ratings = {1, 2, 3};
        const int expected_rating = (accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect"s);
    }

    //Проверяем для нечётной суммы рейтингов
    {
        const vector<int> ratings = {1, 3, 3};
        const int expected_rating = (accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect"s);
    }

    //Проверяем для отрицательных значений 
    {
        const vector<int> ratings = {-1, -3, -3};
        const int expected_rating = (accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect (negative rating)"s);
    }

    //Проверяем для рейтинга с суммой меньше количества оценок
    {
        const vector<int> ratings = {1, 3, -3};
        const int expected_rating = (accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect (around 0)"s);
    }
}

//Тест проверяет работу с релевантностью документов
 //Объединены в один во избежание дублирования кода для вычисления релевантности
void TestRelevanceCalculationAndSort() {
    const vector<string> content = {"белый кот модный ошейник"s, "пушистый кот пушистый хвост"s, "ухоженный пёс выразительные глаза"s};
    const string query = "ухоженный кот"s;
    const vector<vector<int>> ratings = {{8, -3}, {7, 2, 7}, {5, -12, 2, 1}};
    
    //Вычисляем TF-IDF для использования как эталонного значения
    map<int, vector<string>> ids_to_words;  //<ID документа, вектор из слов документа>
    map<int, map<string, double>> ids_to_tf; //<ID документа, <слово из запроса, TF слова для документа>>
    map<string, double> word_idf; //<слово из запроса, IDF для слова>
    map<int, double> expected_relevance; //ID документа, релевантность
    //Заполняем ids_to_words словами
    for(int i = 0; i < 3; ++i) {
        ids_to_words[i] = SplitIntoWords(content[i]);
    }
    //высчитываем и записываем в ids_to_tf TF для каждого слова
    for(int i = 0; i < 3; ++i) {
        for(const string& word : SplitIntoWords(query)) {
            int occurences = count_if(ids_to_words.at(i).begin(), ids_to_words.at(i).end(), [word](const string& str){
                return str == word;
            });
            ids_to_tf[i][word] = (occurences * 1.0)/ids_to_words.at(i).size();
        }
    }
    //Вычисляем IDF для каждого слова из запроса
    for(const string& word : SplitIntoWords(query)) {
        int documents_with_word = 0;
        for(const auto& [id, words] : ids_to_words) {
            int counter = count_if(words.begin(), words.end(), [word](const string& str) {
                return str == word;
            });
            if(counter > 0) {
                ++documents_with_word;
            }
        }
        word_idf[word] = log((content.size()*1.0)/documents_with_word);
    }
    //Вычисляем релевантность каждого документа
    for(int i = 0; i < 3; ++i) {
        double relevance = 0;
        for(const string& word : SplitIntoWords(query)) {
            relevance += word_idf[word] * ids_to_tf[i][word];
        }
        expected_relevance[i] = relevance;
    }


    //Проверяем вычисление релевантности и сортировку по ней
   
    {
        SearchServer server;
        server.SetStopWords("и в на"s);
        for(int i = 0; i < 3; ++i) {
            server.AddDocument(i, content[i], DocumentStatus::ACTUAL, ratings[i]);
        }
        const vector<Document> result = server.FindTopDocuments(query);
        ASSERT_EQUAL(result.size(), 3u);
        //проверяем вычисление релевантности
        map<int, double> result_id_to_relevance;
        for(const auto& [id, relevance] : result_id_to_relevance) {
        ASSERT_HINT(abs(relevance - expected_relevance.at(id)) < COMPARISON_PRECISION, "Relevance calculation is incorrect"s);
        }

        //проверяем сортировку
        vector<Document> expected_vector;
        for(const auto& [id, relevance] : expected_relevance) {
            expected_vector.push_back({id, relevance, accumulate(ratings[id].begin(), ratings[id].end(), 0)/static_cast<int>(ratings[id].size())});
        }
        sort(expected_vector.begin(), expected_vector.end(),
            [](const Document& lhs, const Document& rhs) {
                    if (abs(lhs.relevance - rhs.relevance) < COMPARISON_PRECISION) {
                         return lhs.rating > rhs.rating;
                    } else {
                        return lhs.relevance > rhs.relevance;
                    }
            });
        for(int i = 0; i < 3; ++i) {
            ASSERT_EQUAL_HINT(result[i].id, expected_vector[i].id, "Relevance sorting is incorrect"s);
        }
    }

}

//Тест проверяет работу предиката-фильтра
void TestPredicate() {
    const vector<string> content = {"cat in the city"s,
        "cat in the city eats cat food and does other stuff cat do"s,
        "cat cat cat food food food"s};
    const vector<DocumentStatus> statuses = {DocumentStatus::ACTUAL, DocumentStatus::BANNED, DocumentStatus::IRRELEVANT};
    const vector<vector<int>> ratings = {{3, 4, 5}, {2, 3, 4}, {1, 2, 3}};
    //Создаём общий SearchServer для всех тестов этой функции
    SearchServer server;
    for(int i = 0; i < 3; ++i) {
        server.AddDocument(i, content[i], statuses[i], ratings[i]);
    }
    //Тестируем предикаты
    //Сортировка по ID
    {
        const vector<Document> result = server.FindTopDocuments("cat food"s, [](int id, DocumentStatus status, int rating) {
            return id < 2;
        });
        //We expect to get only doc ids 0 and 1
        ASSERT_EQUAL(result.size(), 2u);
        vector<int> result_ids;
        for(const Document& doc : result) {
            result_ids.push_back(doc.id);
        }
        vector<int> expected_ids = {1, 0};      
        ASSERT_EQUAL_HINT(result_ids, expected_ids, "Predicate filtering is incorrect"s);
    }
    //Сортировка по рейтингу
    {
        const vector<Document> result = server.FindTopDocuments("cat"s, [](int id, DocumentStatus status, int rating) {
            return rating == 2;
        });
        //We expect to get only last document (ID = 2, ratings = {1, 2, 3})
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_HINT(result[0].id == 2, "Predicate filtering is incorrect"s);
    }
}

//Тест проверяет поведение перегруженных функций FindTopDocuments
void TestFindTopDocumentsOverloads() {
    const vector<string> content = {"cat in the city"s,
        "cat in the city eats cat food and does other stuff cat do"s,
        "cat cat cat food food food"s};
    const vector<DocumentStatus> statuses = {DocumentStatus::ACTUAL, DocumentStatus::BANNED, DocumentStatus::IRRELEVANT};
    const vector<vector<int>> ratings = {{3, 4, 5}, {2, 3, 4}, {1, 2, 3}};
    //Создаём общий SearchServer для всех тестов этой функции
    SearchServer server;
    for(int i = 0; i < 3; ++i) {
        server.AddDocument(i, content[i], statuses[i], ratings[i]);
    }
    //Проверяем фильтр по статусу
    {
        const vector<Document> result = server.FindTopDocuments("cat food"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].id, 1, "Status filtering is incorrect"s);
        const vector<Document> no_existing_status_result = server.FindTopDocuments("cat", DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(no_existing_status_result.size(), 0, "Status filtering is incorrect");
    }
    //Проверяем стандартную FindTopDocuments(const string& query)
    {
        const vector<Document> result = server.FindTopDocuments("-city food"s);
        ASSERT_EQUAL_HINT(result.size(), 0, "Default FindTopDocuments implementation with 1 argument is incorrect"s);
        const vector<Document> result2 = server.FindTopDocuments("stuff");
        ASSERT_EQUAL_HINT(result2.size(), 0, "Default FindTopDocuments implementation with 1 argument is incorrect (not only DocumentStatus::ACTUAL included)"s);
    }    
}
/*
Разместите код остальных тестов здесь
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestRating);
    RUN_TEST(TestRelevanceCalculationAndSort);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestFindTopDocumentsOverloads);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    RUN_TEST(TestSearchServer);
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}