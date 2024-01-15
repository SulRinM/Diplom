#include "indexer.h"

void Indexer::idx_words(const string &reg, const string &sentence)
{
    regex word_regex(reg);

    for (auto itr = sregex_iterator(sentence.begin(), sentence.end(), word_regex);
         itr != sregex_iterator(); ++itr)
    {
        string word = (*itr).str();
        auto word_value_itr = words.find(word);
        if (word_value_itr == words.end())
            words.emplace(word, 1);
        else
            ++word_value_itr->second;
    }
}

bool Indexer::find_protocol(const string &ref, const Link &link, ProtocolType &protocol)
{
    regex http_regex("http://");
    regex https_regex("https://");
    regex href_regex("href\\s*=\\s*['\"]?/");
    smatch match;

    if (regex_search(ref, match, http_regex))
    {
        protocol = ProtocolType::HTTP;
        return true;
    }
    else if (regex_search(ref, match, https_regex))
    {
        protocol = ProtocolType::HTTPS;
        return true;
    }
    else if (regex_search(ref, match, href_regex))
    {
        protocol = link.protocol;
        return true;
    }
    else
    {
        return false;
    }
}

bool Indexer::find_domain(const string &ref, const Link &link, string &domain)
{
    regex domain_regex("//([^/]+)/");
    regex href_regex("href\\s*=\\s*['\"]?/");
    smatch match;

    if (regex_search(ref, match, domain_regex))
    {
        domain = match[1].str();
        return true;
    }
    else if (regex_search(ref, match, href_regex))
    {
        domain = link.hostName;
        return true;
    }
    else
    {
        return false;
    }
}

bool Indexer::find_query(const string &ref, string &query)
{
    regex query_regex("\\?([^#]*)");
    smatch match;

    if (regex_search(ref, match, query_regex))
    {
        query = match[1].str();
        return true;
    }
    else
    {
        query = "/";
        return false;
    }
}

void Indexer::parse_words(const string &raw_data)
{
    string sub_html = raw_data;

    Indexer::clr_data(
        {"<(script|style|noscript|!--)([\\w\\W]+?)</(script|style|noscript|--)>",
         "<[^<>]+>",
         "[^A-Za-z]"},
        sub_html);

    transform(sub_html.begin(), sub_html.end(), sub_html.begin(),
              [](unsigned char ch)
              { return tolower(ch); });

    Indexer::idx_words("[a-z]{3,32}", sub_html);
}

void Indexer::parse_links(const string &raw_data, const Link &parent_link)
{
    string sub_html = raw_data;

    regex a_tag_regex("<a\\b([^>]+)>(.*?)</a>");
    unordered_set<string> stored_links;

    for (auto itr = sregex_iterator(sub_html.begin(), sub_html.end(), a_tag_regex);
         itr != sregex_iterator(); ++itr)
    {
        string a_tag = (*itr).str();

        regex href_regex("href\\s*=\\s*\"([^\"]*)\"");
        smatch match;
        if (regex_search(a_tag, match, href_regex))
        {
            string ref = match[1];

            ProtocolType protocol;
            string domain;
            string query;

            if (Indexer::find_protocol(ref, parent_link, protocol) &&
                Indexer::find_domain(ref, parent_link, domain) &&
                Indexer::find_query(ref, query))
            {
                string link_key = domain + query;

                if (stored_links.insert(link_key).second)
                {
                    links.push_back({protocol, domain, query});
                }
            }
        }
    }
}

void Indexer::push_data_db(const string &host, const string &port,
                           const string &dbname, const string &user, const string &pass,
                           const Link &link)
{
    // ... Код функции push_data_db остается без изменений ...
}

void Indexer::clr_data(const vector<string> &regular_expressions, string &data)
{
    for (const auto &regex_pattern : regular_expressions)
    {
        regex regex(regex_pattern);
        data = regex_replace(data, regex, " ");
    }
}

unordered_map<string, unsigned long> Indexer::get_words()
{
    return words;
}

vector<Link> Indexer::get_links()
{
    return links;
}
