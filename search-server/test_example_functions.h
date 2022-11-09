#pragma once

#include <iostream>
#include "document.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cassert>
#include "search_server.h"
#include <algorithm>
#include <numeric>
#include "process_queries.h"

const double COMPARISON_PRECISION = 1e-6;
//Переопределяем стандартный вывод для массивов
//Переопределяем вывод пар
template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::pair<Key, Value>& element) {
    using namespace std::literals;
    out <<  element.first << ": "s << element.second;
    return out;
}

//Шаблонная функция для вывода шаблонного контейнера в стандартный выход
template <typename Container>
std::ostream& Print(std::ostream& out, const Container& container) {
    using namespace std::literals;
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
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out << '[';
    Print(out, container);
    out << ']';
    return out;
}

//Вывод сета
template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::set<Element>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

//Вывод мапа
template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

//Вывод DocumentStatus
std::ostream& operator<<(std::ostream& out, const DocumentStatus status);

//Макросы для теста-сравнения 2 аргументов
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

//Макросы для теста булева значения
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

//Функция теста для сравнения 2 аргументов
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    using namespace std::literals;
    if (t != static_cast<T>(u)) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

//Функция теста для булева значения
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

//Функция для вызова функций тестирования и вывода ответа после успешного завершения теста
template <typename function>
void RunTestImpl(function f, const std::string& function_name) {
    using namespace std::literals;
    f();
    std::cerr << function_name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

/*Функции тестирования*/

//Тест проверяет добавление документов в сервер
void TestAddDocument();
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();
//Тест проверяет работу поисковой системы с минус-словами
void TestMinusWords();
//Тест проверяет работу матчинга
void TestMatching();
//Тест проверяет калькуляцию рейтинга
void TestRating();
//Тест проверяет вычисление релевантности документов
void TestRelevanceCalculation();
//Тест проверяет сортировку выдачи
void TestSorting();
//Тест проверяет работу предиката-фильтра
void TestPredicate();
//Тест проверяет поведение перегруженных функций FindTopDocuments
void TestFindByStatus();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();