// Copyright 2020 Burylov DEnis <burylov01@mail.ru>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_
#include <gumbo.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <utility>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

std::optional<std::pair<std::string, std::string>> get_host_and_target(
    const std::string& url);

http::response<http::string_body> download_http(const std::string& host,
                                                const std::string& port,
                                                const std::string& target);

std::set<::std::string> find_objects_(const GumboNode* node, GumboTag tag,
                                      const char* name);

std::ostream& operator<<(std::ostream& out,
                         const std::set<::std::string>& image_urls);

std::set<::std::string> parse_image(
    const http::response<http::string_body>& response);
std::set<::std::string> parse_children(
    const http::response<http::string_body>& response);

class Crawler {
 public:
  Crawler(const std::size_t& depth, const std::string& url,
          const std::size_t& network_workers, const std::size_t& parser_workers,
          const std::string& output);

  void join();

 private:
  std::size_t depth_;
  std::ofstream output_;
  asio::thread_pool network_workers_;
  asio::thread_pool parser_workers_;
  std::thread writer_worker_;
  std::atomic_size_t works_ = 0;
  std::mutex works_mut_{};
  std::condition_variable works_cv_{};
  std::queue<std::string> writer_queue_{};
  std::mutex writer_mut_{};
  std::condition_variable writer_cv_{};

  void start_of_work_();

  void end_of_work_();

  void downloader(const std::string& url, const size_t& depth);

  void parser(const http::response<http::string_body>& response,
              const size_t& depth);
};

#endif  // INCLUDE_HEADER_HPP_
