#pragma once
#include <iostream>
#include <vector>
#include <string>

using std::literals::string_literals::operator""s;

struct Document {
    Document();
    Document(int id, double relevance, int rating);

    int id = 0;
    double relevance = 0;
    int rating = 0;
};

std::ostream& operator<<(std::ostream& out, const Document& document);

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);
//