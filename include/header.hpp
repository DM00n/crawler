// Copyright 2018 Your Name <your_email>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_

#include <string>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/program_options.hpp>
#include <boost/beast.hpp>
#include <gumbo.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <mutex>
#include <boost/asio/io_service.hpp>
#include <queue>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <root_certificate.h>

const char HTTP_PORT[] = "80";
const char HTTPS_PORT[] = "443";
const char HTTP[] = "http";
const char ERROR[] = "ERROR";
const char HTTP_COLON[] = "http:";
const char HTTP_COLON_DSLASH[] = "http://";
const char HTTPS_COLON[] = "https:";
const char HTTPS[] = "https";
const char DSLASH[] = "//";
const char SLASH = '/';
const char SPACE_DASH_SPACE[] = " - ";
const char HREF[] = "href";
const char ERR_OPEN_FILE[] = "error opening output file!";
const char NO_IMAGES[] = "no images found";
const char SRC[] = "src";

struct URL_with_depth {
    std::string url;
    unsigned depth;
};

struct URL_with_body {
    std::string url;
    std::string body;
};

class Crawler {
public:
    Crawler(const std::string &url, unsigned depth, unsigned network_threads,
            unsigned parser_threads, std::string output);

    void make_link_vector(const std::string &url, unsigned depth);

    void printer();

    void search_for_links(GumboNode *node, std::vector<std::string> *v);

    std::string get_host_from_link(const std::string &str);

    std::string get_port_from_link(const std::string &str);

    std::string get_target_from_link(const std::string &str);

    void search_for_img(GumboNode *node, const std::string &url);

    void networking();

    void create_net_threaders();

    void create_pars_threaders();

    void parsing();

    std::string http_downloader(std::string port, std::string url);

    std::string https_downloader(std::string port, std::string url);

private:
    std::vector<std::string> _images;
    std::vector<std::string> _unique_links;
    std::vector<std::thread> _net_threads;
    std::vector<std::thread> _pars_threads;
    std::queue<URL_with_depth> _links_queue;
    std::queue<URL_with_body> _pars_queue;
    std::string _url;
    unsigned _depth;
    unsigned _network_threads;
    unsigned _parser_threads;
    std::string _output;
    std::mutex cs;
    std::mutex as;
    std::condition_variable _cond;
    unsigned _net_counter;
    bool _net_work;
    unsigned _pars_counter;
    bool _pars_work;
};

#endif  // INCLUDE_HEADER_HPP_
