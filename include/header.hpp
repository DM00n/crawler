// Copyright 2018 Your Name <your_email>


#ifndef CRAWLER_HEADER_HPP
#define CRAWLER_HEADER_HPP
#include <string>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/beast.hpp>
#include "gumbo.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <mutex>
#include <boost/asio/io_service.hpp>
#include <queue>
#include <thread>
#include <condition_variable>

struct URL_info{
    std::string url;
    unsigned depth;
    bool taken;
};

class Crawler{
public:
    Crawler(const std::string& url, unsigned  depth, unsigned  network_threads,
            unsigned parser_threads, std::string output);
    void make_link_vector(const std::string& url, unsigned depth);
    void printer();
    void search_for_links(GumboNode* node, std::vector<std::string> *v);
    std::string get_host_from_link(const std::string& str);
    std::string get_port_from_link(const std::string& str);
    std::string get_target_from_link(const std::string& str);
    void search_for_img(GumboNode* node);
    void networking();
    void create_net_threaders();
    void create_pars_threaders();
    void parsing();
private:
    std::vector<std::string> _images;
    std::vector<std::string> _unique_links;
    std::vector<std::thread> _net_threads;
    std::vector<std::thread> _pars_threads;
    std::queue<URL_info> _links_queue;
    std::queue<std::string> _pars_queue;
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
#endif //CRAWLER_HEADER_HPP
