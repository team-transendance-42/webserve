A web crawler (also called a spider or bot) is an automated program that systematically browses the internet by following links from page to page, downloading and indexing content along the way.
Think of it like this: the web is a giant graph where pages are nodes and hyperlinks are edges. A crawler does a graph traversal (usually BFS) across this graph.

SEED URLs
    │
    ▼
┌─────────────┐
│  URL Queue  │  ◄──────────────────────┐
└─────────────┘                         │
    │                                   │
    ▼                                   │
┌─────────────┐     ┌──────────────┐   │
│  Fetch Page │────►│  Parse HTML  │   │
│  (HTTP GET) │     │  Extract     │───┘
└─────────────┘     │  new URLs    │
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
                    │   Store /    │
                    │   Index      │
                    └──────────────┘

Pop a URL from the queue
Fetch the page (HTTP GET)
Parse HTML, extract all <a href=""> links
Filter & normalize new URLs
Push unseen URLs into the queue
Store/index the page content
Repeat
--------------------------------------------------

1. URL Frontier (the queue)
The data structure holding URLs to visit. In production crawlers this is a priority queue — not plain BFS — so you can prioritize important pages.
Priority factors:
  - PageRank estimate
  - Domain freshness (last crawl time)
  - URL depth (how many hops from seed)
  - Domain politeness (don't hammer one server)
--------------------------------------------------

2. Fetcher
Makes the actual HTTP request. Needs to handle:

Redirects (301, 302)
Timeouts
Gzip/deflate encoding
User-Agent header (identifies your bot)
Robots.txt compliance
-------------------------------------------------

3. HTML Parser
Extracts links and content. Must handle malformed HTML (most real HTML is broken).
4. URL Normalizer
Canonicalizes URLs so you don't visit the same page twice:
http://Example.COM/Page  →  http://example.com/page
http://site.com/a/../b   →  http://site.com/b
http://site.com/?b=2&a=1 →  http://site.com/?a=1&b=2
--------------------------------------------------

5. Seen/Visited Store
A set of already-crawled URLs. At web scale this uses a Bloom filter (probabilistic, memory-efficient) instead of a hash set.
6. Robots.txt Parser
Every polite crawler must respect robots.txt:
# robots.txt example
User-agent: *
Disallow: /admin/
Disallow: /private/

User-agent: Googlebot
Allow: /

Crawl-delay: 10        ← wait 10s between requests
---------------------------------------------------

Politeness & Rate Limiting
Crawlers must not overwhelm servers. Standard rules:
Per-domain delay:     ≥ 1 request / second (minimum)
Crawl-delay header:   always respect it
Concurrent requests:  limit per domain (e.g. max 2)
User-Agent:           always identify yourself honestly
---------------------------------------------------

Small Working Example in Python
pythonimport requests
from bs4 import BeautifulSoup
from collections import deque
from urllib.parse import urljoin, urlparse

def crawl(seed_url, max_pages=20):
    visited = set()
    queue   = deque([seed_url])
    base    = urlparse(seed_url).netloc  # stay on same domain

    while queue and len(visited) < max_pages:
        url = queue.popleft()

        if url in visited:
            continue

        try:
            res = requests.get(url, timeout=5, headers={
                "User-Agent": "MyBot/1.0"
            })
            if "text/html" not in res.headers.get("Content-Type", ""):
                continue
        except Exception:
            continue

        visited.add(url)
        print(f"[{len(visited)}] {url}")

        soup = BeautifulSoup(res.text, "html.parser")

        for tag in soup.find_all("a", href=True):
            absolute = urljoin(url, tag["href"])   # handle relative URLs
            parsed   = urlparse(absolute)

            # stay on same domain, only HTTP(S), not yet visited
            if (parsed.netloc == base
                    and parsed.scheme in ("http", "https")
                    and absolute not in visited):
                queue.append(absolute)

    return visited

crawl("https://example.com")
-------------------------------------------------------

Same thing in C (closer to what you know)
c/*
  Minimal crawler skeleton in C — shows the architecture.
  Uses libcurl for HTTP, no HTML parser (you'd add libxml2).
*/

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_URLS  1000
#define MAX_LEN   2048

typedef struct {
    char  urls[MAX_URLS][MAX_LEN];
    int   head, tail, count;
} Queue;

char visited[MAX_URLS][MAX_LEN];
int  visited_count = 0;

int already_seen(const char *url) {
    for (int i = 0; i < visited_count; i++)
        if (strcmp(visited[i], url) == 0) return 1;
    return 0;
}

void enqueue(Queue *q, const char *url) {
    if (q->count >= MAX_URLS) return;
    strncpy(q->urls[q->tail], url, MAX_LEN - 1);
    q->tail  = (q->tail + 1) % MAX_URLS;
    q->count++;
}

// libcurl write callback — stores response body
size_t write_cb(void *ptr, size_t size, size_t nmemb, char *buf) {
    size_t total = size * nmemb;
    strncat(buf, ptr, total);   // simplified; use dynamic buffer in real code
    return total;
}

void fetch_and_crawl(Queue *q, const char *url) {
    CURL   *curl = curl_easy_init();
    char    body[1024 * 512] = {0};

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "MyBot/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        printf("Fetched: %s (%zu bytes)\n", url, strlen(body));
        // TODO: parse body for <a href=""> links, enqueue new URLs

    curl_easy_cleanup(curl);
}

int main(void) {
    Queue q = {0};
    curl_global_init(CURL_GLOBAL_ALL);

    enqueue(&q, "http://example.com");

    while (q.count > 0 && visited_count < 50) {
        char url[MAX_LEN];
        strncpy(url, q.urls[q.head], MAX_LEN - 1);
        q.head  = (q.head + 1) % MAX_URLS;
        q.count--;

        if (already_seen(url)) continue;
        strncpy(visited[visited_count++], url, MAX_LEN - 1);

        fetch_and_crawl(&q, url);
    }

    curl_global_cleanup();
    return 0;
}
------------------------------------

Real-World Crawlers
CrawlerUsed ByNotesGooglebotGoogle SearchCrawls billions of pages/dayBingbotBingRespects crawl-delay strictlyScrapyPython devsPopular open-source frameworkHeritrixInternet ArchiveArchives the entire webNutchApacheDistributed, Hadoop-based

Where Crawlers Are Used

Search engines — index the web (Google, Bing)
Price comparison — scrape product prices
SEO tools — audit your own site (Screaming Frog)
Security — find exposed endpoints, leaked data
Research — collect datasets (Common Crawl — 3 billion pages)
Your webserv project — you could write a test crawler to stress-test your server