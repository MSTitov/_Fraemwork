﻿#include "stdafx2.h"

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() 
{
    string s;
    getline(cin, s);

    return s;
}

int ReadLineWithNumber() 
{
    int result;
    cin >> result;
    ReadLine();

    return result;
}

vector<string> SplitIntoWords(const string& text) 
{
    vector<string> words;
    string word;
    for (const char c : text) 
    {
        if (c == ' ') 
        {
            words.push_back(word);
            word = "";
        }
        else 
        {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document
{
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus 
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer 
{
public:
    void SetStopWords(const string& text) 
    {
        for (const string& word : SplitIntoWords(text)) 
        {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id,
        const string& document,
        DocumentStatus status,
        const vector<int>& ratings)
    {
        ++_document_count;
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words)
        {
            if (document_id >= 0)
            {
                word_to_document_freqs_[word][document_id] += inv_word_count;
            }            
        }
        documents_.emplace(document_id,
            DocumentData
            {
                ComputeAverageRating(ratings),
                status
            });
    }

    template<typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query,
        const KeyMapper& key_mapper) const 
    {
        if constexpr (is_same_v<KeyMapper, DocumentStatus>)
        {
            return FindTopDocuments(raw_query, [=](int document_id, DocumentStatus status, int rating)
                {
                    return status == key_mapper;
                });
        }

        else
        {
            const Query query = ParseQuery(raw_query);
            auto matched_documents = FindAllDocuments(query, key_mapper);

            sort(matched_documents.begin(), matched_documents.end(),
                [=](const Document& lhs, const Document& rhs)
                {
                    if (abs(lhs.relevance - rhs.relevance) < EPSILON) return lhs.rating > rhs.rating;
                    else return lhs.relevance > rhs.relevance;
                });
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
            {
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            }

            return matched_documents;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const
    {
        return  FindTopDocuments(raw_query, [=](int document_id, DocumentStatus status, int rating) 
            { 
                return status == DocumentStatus::ACTUAL; 
            });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(
        const string& raw_query, 
        int document_id) const
    {
        vector<string> query_word;
        const Query query = ParseQuery(raw_query);
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id) != 0)
            {
                query_word.push_back(word);
            }
            else if (word_to_document_freqs_.count(word) == 0 && word_to_document_freqs_.at(word).count(document_id) == 0)
            {
                query_word.clear();
            }
        }

        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id) != 0)
            {
                query_word.clear();
            }
        }

        return { query_word, documents_.at(document_id).status };
    }

private:
    struct DocumentData 
    {
        int rating;
        DocumentStatus status;
    };

    int _document_count = 0;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const 
    {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const 
    {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) 
        {
            if (!IsStopWord(word)) 
            {
                words.push_back(word);
            }
        }

        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) 
    {
        if (ratings.empty()) 
        {
            return 0;
        }
        
        int rating_sum = accumulate(begin(ratings), end(ratings), 0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord 
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const 
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }

        return 
        {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query 
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const
    {
        Query query;
        for (const string& word : SplitIntoWords(text)) 
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) 
            {
                if (query_word.is_minus) 
                {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }

        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const 
    {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    template<typename KeyMapper>
    vector<Document> FindAllDocuments(const Query& query, 
        const KeyMapper& key_mapper) const 
    {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0) 
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto it : word_to_document_freqs_.at(word)) 
            {
                document_to_relevance[it.first] += it.second * inverse_document_freq;
            }
        }

        for (const string& word : query.minus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0) 
            {
                continue;
            }
            for (const auto it : word_to_document_freqs_.at(word)) 
            {
                document_to_relevance.erase(it.first);
            }
        }

        vector<Document> matched_documents;
        for (const auto it : document_to_relevance) 
        {
            if (key_mapper(it.first, documents_.at(it.first).status, documents_.at(it.first).rating))
                matched_documents.push_back(
                {
                    it.first,
                    it.second,
                    documents_.at(it.first).rating
                });

        }

        return matched_documents;
    }
};

void PrintDocument(const Document& document) 
{
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() 
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) 
    {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) 
    {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) 
    {
        PrintDocument(document);
    }

    return 0;
}