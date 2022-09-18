#include "test_example_functions.h"

// -------- Начало модульных тестов поисковой системы ----------

//Переопределяем стандартный вывод для массивов

//Вывод DocumentStatus
std::ostream& operator<<(std::ostream& out, const DocumentStatus status) {
    using namespace std::literals;
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

//Функция теста для булева значения
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    using namespace std::literals;
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

/*Функции тестирования*/
//Тест проверяет добавление документов в сервер
void TestAddDocument() {
    using namespace std::literals;
    const int doc_id = 1;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    //Проверяем что до добавления документов нет и что после добавления их количество увеличилось.
    {
        SearchServer server(""s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0, "Server must have no documents after initialization"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1u, "AddDocument() works incorrect"s);
        server.AddDocument(doc_id +1, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2u, "AddDocument() works incorrect"s);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    using namespace std::literals;
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

//Тест проверяет работу поисковой системы с минус-словами
void TestMinusWords() {
    using namespace std::literals;
    const int document_id = 1;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};

    //Проверяем, ищется ли документ по плюс-словам
    {
        SearchServer server(""s);
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, document_id);
    }

    //Теперь проверяем что поиск с минус-словом возвращает пустой результат
    {
        SearchServer server(""s);
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Documents with minus words must be excluded from result"s);
    }

    //Проверяем что по запросам с минус-словами находятся документы без минус-слов
    {
        SearchServer server(""s);
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 1, "cat food is delicious"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Search with minus words is incorrect"s);
        ASSERT_EQUAL_HINT(found_docs[0].id, 2, "Search with minus words is incorrect"s);
    }
}

//Тест проверяет работу матчинга
void TestMatching() {
    using namespace std::literals;
    const std::string content = "cat in the city eats cat food and does other stuff cat do"s;
    const std::string stop_words = "in the and"s;

    //Проверяем наличие всех слов из запроса, входящих в документ
    {
        SearchServer server(""s);
        server.AddDocument(1, content, DocumentStatus::BANNED, {1, 2, 3});
        const std::string query = "cat food"s;
        const std::vector<std::string> expected_words = {"cat"s, "food"s};
        const auto [matched_words, status] = server.MatchDocument(query, 1);
        ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching words from document is incorrect"s);
        ASSERT_EQUAL_HINT(status, DocumentStatus::BANNED, "Status from matching is incorrect"s);
    }

    //Проверяем матчинг при нахождении в документе минус-слова из запроса.
    {
        SearchServer server(""s);
        server.AddDocument(1, content, DocumentStatus::ACTUAL, {1, 2, 3});
        const std::string query = "cat food -city";
        const auto [matched_words, status] = server.MatchDocument(query, 1);
        std::vector<std::string> expected_words;
        expected_words.clear();
        ASSERT_EQUAL_HINT(matched_words, expected_words, "Minus words not processed correctly in MatchDocuments()"s);
    }

    //Проверяем матчинг со стоп-словами
    {
        SearchServer server(stop_words);
        server.AddDocument(1, content, DocumentStatus::ACTUAL, {1, 2, 3});
        //Проверяем запрос со стоп-словами и не стоп-словами
        {
            const std::string query = "cat in the food"s;
            const auto [matched_words, status] = server.MatchDocument(query, 1);
            const std::vector<std::string> expected_words = {"cat"s, "food"s};
            ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching with stop words is incorrect"s);
        }
        //Проверяем запрос только из стоп слов
        {
            const std::string query = "and in"s;
            const auto [matched_words, status] = server.MatchDocument(query, 1);
            std::vector<std::string> expected_words;
            expected_words.clear();
            ASSERT_EQUAL_HINT(matched_words, expected_words, "Matching document with stop words only is incorrect"s);
        }
    }
}

//Тест проверяет калькуляцию рейтинга
void TestRating() {
    using namespace std::literals;
    const int doc_id = 1;
    const std::string content = "cat in the city"s;
    //Проверяем для чётной суммы рейтингов
    {
        const std::vector<int> ratings = {1, 2, 3};
        const int expected_rating = (std::accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect"s);
    }

    //Проверяем для нечётной суммы рейтингов
    {
        const std::vector<int> ratings = {1, 3, 3};
        const int expected_rating = (std::accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect"s);
    }

    //Проверяем для отрицательных значений
    {
        const std::vector<int> ratings = {-1, -3, -3};
        const int expected_rating = (std::accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect (negative rating)"s);
    }

    //Проверяем для рейтинга с суммой меньше количества оценок
    {
        const std::vector<int> ratings = {1, 3, -3};
        const int expected_rating = (std::accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size()));
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].rating, expected_rating, "Rating calculation is incorrect (around 0)"s);
    }
}

//Тест проверяет вычисление релевантности документов
void TestRelevanceCalculation() {
    using namespace std::literals;
    const std::vector<std::string> content = {"белый кот модный ошейник"s, "пушистый кот пушистый хвост"s, "ухоженный пёс выразительные глаза"s};
    const std::string query = "ухоженный кот"s;
    const std::vector<std::vector<int>> ratings = {{8, -3}, {7, 2, 7}, {5, -12, 2, 1}};

    //Вычисляем TF-IDF для использования как эталонного значения
    std::map<int, std::vector<std::string>> ids_to_words;  //<ID документа, вектор из слов документа>
    std::map<int, std::map<std::string, double>> ids_to_tf; //<ID документа, <слово из запроса, TF слова для документа>>
    std::map<std::string, double> word_idf; //<слово из запроса, IDF для слова>
    std::map<int, double> expected_relevance; //ID документа, релевантность
    //Заполняем ids_to_words словами
    for(int i = 0; i < 3; ++i) {
        ids_to_words[i] = SplitIntoWords(content[i]);
    }
    //высчитываем и записываем в ids_to_tf TF для каждого слова
    for(int i = 0; i < 3; ++i) {
        for(const std::string& word : SplitIntoWords(query)) {
            int occurences = count_if(ids_to_words.at(i).begin(), ids_to_words.at(i).end(), [word](const std::string& str){
                return str == word;
            });
            ids_to_tf[i][word] = (occurences * 1.0)/ids_to_words.at(i).size();
        }
    }
    //Вычисляем IDF для каждого слова из запроса
    for(const std::string& word : SplitIntoWords(query)) {
        int documents_with_word = 0;
        for(const auto& [id, words] : ids_to_words) {
            int counter = count_if(words.begin(), words.end(), [word](const std::string& str) {
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
        for(const std::string& word : SplitIntoWords(query)) {
            relevance += word_idf[word] * ids_to_tf[i][word];
        }
        expected_relevance[i] = relevance;
    }

    //Проверяем вычисление релевантности
    {
        SearchServer server("и в на"s);
        for(int i = 0; i < 3; ++i) {
            server.AddDocument(i, content[i], DocumentStatus::ACTUAL, ratings[i]);
        }
        const std::vector<Document> result = server.FindTopDocuments(query);
        ASSERT_EQUAL(result.size(), 3u);
        //проверяем вычисление релевантности
        std::map<int, double> result_id_to_relevance;
        for(const auto& [id, relevance] : result_id_to_relevance) {
            ASSERT_HINT(abs(relevance - expected_relevance.at(id)) < COMPARISON_PRECISION, "Relevance calculation is incorrect"s);
        }
    }
}

//Тест проверяет сортировку выдачи
void TestSorting() {
    using namespace std::literals;
    const std::vector<std::string> content = {"cat in the city"s,
                                    "cat in the city eats cat food and does other stuff cat do"s,
                                    "cat cat cat food food food"s};
    SearchServer server(""s);
    for(int i = 0; i < 3; ++i) {
        server.AddDocument(i, content[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    std::vector<Document> result = server.FindTopDocuments("cat food"s);
    ASSERT_EQUAL(result.size(), 3u);
    ASSERT_HINT(result[0].relevance >= result[1].relevance && result[1].relevance >= result[2].relevance, "Sorting is incorrect"s);
}

//Тест проверяет работу предиката-фильтра
void TestPredicate() {
    using namespace std::literals;
    const std::vector<std::string> content = {"cat in the city"s,
                                    "cat in the city eats cat food and does other stuff cat do"s,
                                    "cat cat cat food food food"s};
    const std::vector<DocumentStatus> statuses = {DocumentStatus::ACTUAL, DocumentStatus::BANNED, DocumentStatus::IRRELEVANT};
    const std::vector<std::vector<int>> ratings = {{3, 4, 5}, {2, 3, 4}, {1, 2, 3}};
    //Создаём общий SearchServer для всех тестов этой функции
    SearchServer server(""s);
    for(int i = 0; i < 3; ++i) {
        server.AddDocument(i, content[i], statuses[i], ratings[i]);
    }
    //Тестируем предикаты
    //Фильтр по ID
    {
        const std::vector<Document> result = server.FindTopDocuments("cat food"s, [](int id, DocumentStatus status, int rating) {
            return id < 2;
        });
        //We expect to get only doc ids 0 and 1
        ASSERT_EQUAL(result.size(), 2u);
        std::vector<int> result_ids;
        for(const Document& doc : result) {
            result_ids.push_back(doc.id);
        }
        std::vector<int> expected_ids = {1, 0};
        ASSERT_EQUAL_HINT(result_ids, expected_ids, "Predicate filtering is incorrect"s);
    }
    //Фильтр по рейтингу
    {
        const std::vector<Document> result = server.FindTopDocuments("cat"s, [](int id, DocumentStatus status, int rating) {
            return rating == 2;
        });
        //Ожидаем только последний документ (ID = 2, ratings = {1, 2, 3})
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_HINT(result[0].id == 2, "Predicate filtering is incorrect"s);
    }
}

//Тест проверяет поведение перегруженных функций FindTopDocuments
void TestFindByStatus() {
    using namespace std::literals;
    const std::vector<std::string> content = {"cat in the city"s,
                                    "cat in the city eats cat food and does other stuff cat do"s,
                                    "cat cat cat food food food"s};
    const std::vector<DocumentStatus> statuses = {DocumentStatus::ACTUAL, DocumentStatus::BANNED, DocumentStatus::IRRELEVANT};
    const std::vector<std::vector<int>> ratings = {{3, 4, 5}, {2, 3, 4}, {1, 2, 3}};
    SearchServer server(""s);
    for(int i = 0; i < 3; ++i) {
        server.AddDocument(i, content[i], statuses[i], ratings[i]);
    }
    //Проверяем фильтр по статусу
    {
        const std::vector<Document> result = server.FindTopDocuments("cat food"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL_HINT(result[0].id, 1, "Status filtering is incorrect"s);
        const std::vector<Document> no_existing_status_result = server.FindTopDocuments("cat", DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(no_existing_status_result.size(), 0, "Status filtering is incorrect");
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestRating);
    RUN_TEST(TestRelevanceCalculation);
    RUN_TEST(TestSorting);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestFindByStatus);
}
