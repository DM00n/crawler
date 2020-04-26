// Copyright 2018 Your Name <your_email>

#include "header.hpp"

Crawler::Crawler(const std::string &url, unsigned depth,
        unsigned network_threads,
        unsigned parser_threads, std::string output) :
        _url(url), _depth(depth), _network_threads(network_threads),
        _parser_threads(parser_threads), _output(std::move(output)) {
    _unique_links.push_back(url);
    _links_queue.push({url, 0});
    _net_counter = 1;
    _net_work = false;
    _pars_counter = 1;
    _pars_work = false;
}

void Crawler::make_link_vector(const std::string &url, unsigned depth) {
    try {
        if (depth > _depth) return;
        std::string port = get_port_from_link(url);
        std::string body;
        if (port == HTTP_PORT) body = http_downloader(port, url);
        if (port == HTTPS_PORT) body = https_downloader(port, url);
        GumboOutput *output = gumbo_parse(body.c_str());
        std::vector<std::string> tmp;
        search_for_links(output->root, &tmp);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        depth++;
        std::vector<std::string>::iterator it1, it2;
        cs.lock();
        for (it1 = tmp.begin(); it1 != tmp.end(); ++it1) {
            for (it2 = _unique_links.begin();
            it2 != _unique_links.end(); ++it2){
                if (*it1 == *it2) break;
            }
            if (it2 == _unique_links.end()) {
                _cond.notify_one();
                _unique_links.push_back(*it1);
                _pars_queue.push({url, body});
                _links_queue.push({*it1, depth});
            }
        }
        body.clear();
        tmp.clear();
        cs.unlock();
    }
    catch (std::exception const &e) {
        //std::cerr << "Error: " << e.what() << std::endl;
        return;
    }
}

std::string Crawler::get_host_from_link(const std::string &str) {
    auto start = str.rfind(DSLASH);
    auto end = str.find('/', start + 2);
    return str.substr(start + 2, end - start - 2);
}

std::string Crawler::get_port_from_link(const std::string &str) {
    if (str.substr(0, 4) == HTTP) {
        if (str.substr(0, 5) == HTTPS) return HTTPS_PORT;
        if (str.substr(0, 5) == HTTP_COLON) return HTTP_PORT;
        return ERROR;
    }
    return ERROR;
}

std::string Crawler::get_target_from_link(const std::string &str) {
    auto start = str.rfind(DSLASH);
    auto end = str.find(SLASH, start + 2);
    if (end == std::string::npos) return std::string{SLASH};
    else
    {
        std::string s(str, end, std::string::npos);
        return s;
    }
}

void Crawler::search_for_links(GumboNode *node, std::vector<std::string> *v) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }
    GumboAttribute *href;
    if (node->v.element.tag == GUMBO_TAG_A &&
        (href = gumbo_get_attribute(&node->v.element.attributes, HREF))) {
        std::string a = href->value;
        if (a.find(HTTP) == 0) {
            std::cout << href->value << std::endl;
            v->push_back(href->value);
        }
    }
    GumboVector *children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_links(static_cast<GumboNode *>(children->data[i]), v);
    }
}

void Crawler::printer() {
    std::ofstream fout;
    fout.open(_output);
    if (!fout) {
        std::cout << ERR_OPEN_FILE << std::endl;
        return;
    }
    if (_images.empty()) fout << NO_IMAGES << std::endl;
    else
    {
        unsigned count = 1;
        for (auto const &i : _images) {
            fout << count << SPACE_DASH_SPACE << i << std::endl;
            ++count;
        }
    }

    fout.close();
}

void Crawler::search_for_img(GumboNode *node, const std::string &url) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }
    if (node->v.element.tag == GUMBO_TAG_IMG) {
        GumboAttribute *img;
        img = gumbo_get_attribute(&node->v.element.attributes, SRC);
        if (!img) return;
        std::string tmp = img->value;
        if (tmp.find(DSLASH) == 0) {
            tmp = HTTP_COLON + tmp;
        }
        if (tmp.find(SLASH) != 0) {
            if ((tmp.find(HTTP_COLON) != 0) && (tmp.find(HTTPS_COLON) != 0)) {
                std::string host = get_host_from_link(url);
                tmp = HTTP_COLON_DSLASH + host + DSLASH + tmp;
            }
            std::vector<std::string>::iterator it1;
            cs.lock();
            for (it1 = _images.begin(); it1 != _images.end(); ++it1) {
                if (*it1 == tmp) break;
            }
            if (it1 == _images.end()) {
                //std::cout<<tmp<<std::endl;
                _images.emplace_back(tmp);
            }
            cs.unlock();
        }
    }
    GumboVector *children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_img(static_cast<GumboNode *>(children->data[i]), url);
    }
}

void Crawler::networking() {
    cs.lock();
    if (_net_counter < _network_threads) {
        ++_net_counter;
        cs.unlock();
        std::unique_lock<std::mutex> lock_1(as);
        _cond.wait(lock_1);
        cs.lock();
    }
    if ((_net_counter == _network_threads) && (!_net_work)) {
        _cond.notify_all();
        _net_work = true;
    }
    cs.unlock();
    while (!_links_queue.empty()) {
        cs.lock();
        if (_links_queue.empty()) {
            cs.unlock();
            std::unique_lock<std::mutex> lock(as);
            _cond.wait(lock);
            cs.lock();
        }
        auto a = _links_queue.front().url;
        auto b = _links_queue.front().depth;
        _links_queue.pop();
        //std::cout << _links_queue.size() <<std::endl;
        cs.unlock();
        make_link_vector(a, b);
    }
}

void Crawler::create_net_threaders() {
    for (unsigned i = 1; i <= _network_threads; ++i) {
        _net_threads.emplace_back(std::thread(&Crawler::networking, this));
    }
    for (auto &thr : _net_threads) {
        thr.join();
    }
}

void Crawler::create_pars_threaders() {
    for (unsigned i = 1; i <= _parser_threads; ++i) {
        _pars_threads.emplace_back(std::thread(&Crawler::parsing, this));
    }
    for (auto &i : _pars_threads) {
        i.join();
    }
}

void Crawler::parsing() {
    cs.lock();
    if (_pars_counter < _parser_threads) {
        ++_pars_counter;
        cs.unlock();
        std::unique_lock<std::mutex> lock_2(as);
        _cond.wait(lock_2);
        cs.lock();
    }
    if ((_pars_counter == _parser_threads) && (!_pars_work)) {
        _cond.notify_all();
        _pars_work = true;
    }
    cs.unlock();
    while (!_pars_queue.empty()) {
        cs.lock();
        auto a = _pars_queue.front();
        _pars_queue.pop();
        cs.unlock();
        GumboOutput *output = gumbo_parse(a.body.c_str());
        search_for_img(output->root, a.url);
    }
}

std::string Crawler::http_downloader(std::string port, std::string url) {
    std::string host = get_host_from_link(url);
    std::string target = get_target_from_link(url);
    int version = 11;
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);
    boost::beast::tcp_stream stream(ioc);
    auto const results = resolver.resolve(host, port);
    stream.connect(results);
    boost::beast::http::request<boost::beast::http::string_body>
            req{boost::beast::http::verb::get, target, version};
    req.set(boost::beast::http::field::host, host);
    req.set(boost::beast::http::field::user_agent,
            BOOST_BEAST_VERSION_STRING);
    boost::beast::http::write(stream, req);
    boost::beast::flat_buffer buffer;
    boost::beast::http::response<boost::beast::http::dynamic_body> res;
    boost::beast::http::read(stream, buffer, res);
    std::string body{boost::asio::buffers_begin(res.body().data()),
                     boost::asio::buffers_end(res.body().data())};
    boost::beast::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                             ec);
    if (ec && ec != boost::beast::errc::not_connected)
        throw boost::beast::system_error{ec};
    return body;
}

std::string Crawler::https_downloader(std::string port, std::string url) {
    boost::asio::ssl::context ct{boost::asio::ssl::context::sslv23_client};
    std::string host = get_host_from_link(url);
    std::string target = get_target_from_link(url);
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream{ioc, ct};
    auto const results = resolver.resolve(host, port);

    boost::asio::connect(stream.next_layer(),
                         results.begin(), results.end());
    stream.handshake(ssl::stream_base::client);
    boost::beast::http::request<boost::beast::http::string_body>
            request{boost::beast::http::verb::get, target, 11};
    request.set(boost::beast::http::field::host, host);
    request.set(boost::beast::http::field::user_agent,
            BOOST_BEAST_VERSION_STRING);
    boost::beast::http::write(stream, request);
    boost::beast::flat_buffer buff;
    boost::beast::http::response<boost::beast::http::dynamic_body> res;
    boost::beast::http::read(stream, buff, res);
    std::string body{boost::asio::buffers_begin(res.body().data()),
                     boost::asio::buffers_end(res.body().data())};
    return body;
}

int main(int argc, char **argv) {
    boost::program_options::options_description desc("asd");
    desc.add_options()
            ("url", boost::program_options::value<std::string>())
            ("depth", boost::program_options::value<unsigned>())
            ("network_threads", boost::program_options::value<unsigned>())
            ("parser_threads", boost::program_options::value<unsigned>())
            ("output", boost::program_options::value<std::string>());
    boost::program_options::variables_map vm;
    try {
        boost::program_options::store(boost::program_options::
            parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);
    }
    catch (boost::program_options::error &e) {
        std::cout << e.what() << std::endl;
    }
    Crawler a(vm["url"].as<std::string>(),
              vm["depth"].as<unsigned>(),
              vm["network_threads"].as<unsigned>(),
              vm["parser_threads"].as<unsigned>(),
              vm["output"].as<std::string>());
    a.create_net_threaders();
    std::cout << "okey" << std::endl;
    a.create_pars_threaders();
    a.printer();
    return 0;
}
