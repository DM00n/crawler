// Copyright 2018 Your Name <your_email>

#include <header.hpp>


Crawler::Crawler(const std::string &url, unsigned depth, unsigned network_threads,
                 unsigned parser_threads, std::string output) :
        _url(url), _depth(depth), _network_threads(network_threads),
        _parser_threads(parser_threads), _output(std::move(output)) {
    _unique_links.push_back(url);
    _links_queue.push({url, 0, false});
    _net_counter = 1;
    _net_work = false;
    _pars_counter = 1;
    _pars_work = false;
}

void Crawler::make_link_vector(const std::string& url, unsigned depth){
    try{
    if (depth > _depth) return;
    std::string host = get_host_from_link(url);
    std::string port = get_port_from_link(url);
    std::string target = get_target_from_link(url);
    int version = 11;
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::resolver resolver(ioc);//?
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
    std::string body { boost::asio::buffers_begin(res.body().data()),
                       boost::asio::buffers_end(res.body().data()) };
    boost::beast::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                             ec);
    if (ec && ec != boost::beast::errc::not_connected)
        throw boost::beast::system_error{ec};
    GumboOutput* output = gumbo_parse(body.c_str());
    //search_for_img(output->root);
    std::vector<std::string> tmp;
    search_for_links(output->root, &tmp);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    depth++;
    std::vector<std::string>::iterator it1, it2;
    cs.lock();
    for (it1 = tmp.begin(); it1 != tmp.end(); ++it1) {
        for (it2 = _unique_links.begin(); it2 != _unique_links.end(); ++it2) {
            if (*it1 == *it2) break;
        }
        if (it2 == _unique_links.end()){
            _cond.notify_one();
            _unique_links.push_back(*it1);
            _pars_queue.push(body);
            _links_queue.push({*it1, depth, false});
        }
    }
    body.clear();
    tmp.clear();
    cs.unlock();
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }
}

std::string Crawler::get_host_from_link(const std::string &str) {
    auto start = str.rfind("//");
    auto end = str.find('/', start + 2);
    return str.substr(start + 2, end - start - 2);
}

std::string Crawler::get_port_from_link(const std::string &str) {
    if (str.substr(0, 4) == "http") {
        if (str.substr(0, 5) == "https") return "443";
        if (str.substr(0, 5) == "http:") return "80";
        return "ERROR";
    }
    return "ERROR";
}

std::string Crawler::get_target_from_link(const std::string &str) {
    auto start = str.rfind("//");
    auto end = str.find('/', start + 2);
    std::string s(str, end, std::string::npos);
    return s;
}

void Crawler::search_for_links(GumboNode* node,std::vector<std::string> *v) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }
    GumboAttribute* href;
    if (node->v.element.tag == GUMBO_TAG_A &&
        (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
        std::string a = href->value;
        if (a.find("http") == 0) {
            //std::cout << href->value << std::endl;
            v->push_back(href->value);
        }
    }
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_links(static_cast<GumboNode*>(children->data[i]), v);
    }
}

void Crawler::printer() {
//    if (_unique_links.empty()) std::cout<<"no links"<<std::endl;
//    else
//    for (auto const& i : _unique_links) std::cout << i << std::endl;
//    std::cout<<"--------------------------------------------"<<std::endl;
    if (_images.empty()) std::cout<<"no images"<<std::endl;
    else
        for (auto const& i : _images) std::cout<<i<<std::endl;
}

void Crawler::search_for_img(GumboNode *node) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }
    if (node->v.element.tag == GUMBO_TAG_IMG) {
        GumboAttribute* img;
        img = gumbo_get_attribute(&node->v.element.attributes, "src");
        std::string tmp = img->value;
        if (tmp.find("//") == 0 ) {
            tmp = "http:" + tmp;
        }
        if (tmp.find('/') != 0) {
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
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_img(static_cast<GumboNode*>(children->data[i]));
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
        cs.unlock();
        make_link_vector(a, b);
    }
}

void Crawler::create_net_threaders() {
    for (unsigned i = 1; i <= _network_threads; ++i){
        _net_threads.emplace_back(std::thread(&Crawler::networking, this));
    }
    for (auto& i : _net_threads){
        i.join();
    }
}

void Crawler::create_pars_threaders() {
    for (unsigned i = 1; i <= _parser_threads; ++i){
        _pars_threads.emplace_back(std::thread(&Crawler::parsing, this));
    }
    for (auto& i : _pars_threads){
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
//        if (_pars_queue.empty()) {
//            cs.unlock();
//            std::unique_lock<std::mutex> lock(as);
//            _cond.wait(lock);
//            cs.lock();
//        }
        auto a = _pars_queue.front();
        _pars_queue.pop();
        cs.unlock();
        GumboOutput* output = gumbo_parse(a.c_str());
        search_for_img(output->root);
    }
}