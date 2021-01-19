// Copyright 2020 Burylov DEnis <burylov01@mail.ru>

#include <header.hpp>

std::optional<std::pair<std::string, std::string>> get_host_and_target(
    const std::string& url) {
  size_t offset;
  if (url.find("://") != std::string::npos) {
    if (url.substr(0, 4) != "http") return std::nullopt;
    offset = 7;
  } else {
    offset = 0;
  }
  auto slash_index = url.find('/', offset);
  if (slash_index == std::string::npos) {
    return std::make_pair(url.substr(offset), "/");
  } else {
    return std::make_pair(url.substr(offset, slash_index - offset),
                          url.substr(slash_index));
  }
}

http::response<http::string_body> download_http(std::string const& host,
                                                std::string const& port,
                                                std::string const& target) {
  asio::io_context context;
  beast::tcp_stream stream{context};
  stream.connect(tcp::resolver{context}.resolve(host, port));
  http::request<http::string_body> request{http::verb::get, target, 11};
  request.set(http::field::host, host);
  request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  http::write(stream, request);
  beast::flat_buffer buffer;
  http::response<http::string_body> response;
  http::read(stream, buffer, response);
  return response;
}

struct WrapperGumboOutput {
  GumboOutput* gumbo_output;

  ~WrapperGumboOutput() {
    if (gumbo_output) gumbo_destroy_output(&kGumboDefaultOptions, gumbo_output);
  }

  typename std::add_lvalue_reference<GumboOutput>::type operator*() const {
    return *gumbo_output;
  }

  GumboOutput* operator->() const noexcept { return gumbo_output; }
};

std::set<::std::string> parse_image(
    const http::response<http::string_body>& response) {
  WrapperGumboOutput gumbo{gumbo_parse(response.body().c_str())};
  return find_objects_(gumbo->root, GUMBO_TAG_IMG, "src");
}
std::set<::std::string> parse_children(
    const http::response<http::string_body>& response) {
  WrapperGumboOutput gumbo{gumbo_parse(response.body().c_str())};
  return find_objects_(gumbo->root, GUMBO_TAG_A, "href");
}

std::set<std::string> find_objects_(const GumboNode* node, const GumboTag tag,
                                    const char* name) {
  if (node->type != GUMBO_NODE_ELEMENT) return {};

  std::set<std::string> objects;
  if (node->v.element.tag == tag) {
    auto href_tag = gumbo_get_attribute(&node->v.element.attributes, name);
    if (href_tag) objects.emplace(href_tag->value);
  }

  auto children = &node->v.element.children;
  auto length = children->length;
  for (std::size_t i = 0; i < length; i++) {
    auto child_objects = find_objects_(
        static_cast<const GumboNode*>(children->data[i]), tag, name);
    objects.insert(child_objects.begin(), child_objects.end());
  }
  return objects;
}

std::ostream& operator<<(std::ostream& out,
                         const std::set<std::string>& image_urls) {
  for (const auto& image_url : image_urls) {
    out << image_url << std::endl;
  }
  return out;
}

Crawler::Crawler(const std::size_t& depth, const std::string& url,
                 const std::size_t& network_workers,
                 const std::size_t& parser_workers, const std::string& output)
    : depth_{depth},
      output_{output},
      network_workers_{network_workers},
      parser_workers_{parser_workers},
      writer_worker_{} {
  downloader(url, 0);
  writer_worker_ = std::thread{[this] {
    while (true) {
      std::unique_lock lock{writer_mut_};
      while (writer_queue_.empty() && (works_ != 0)) {
        writer_cv_.wait(lock);
      }
      if (works_ == 0) break;
      auto result = writer_queue_.front();
      writer_queue_.pop();
      lock.unlock();
      output_ << result << ::std::endl;

      end_of_work_();
    }
  }};
}

void Crawler::join() {
  std::unique_lock lock{works_mut_};
  works_cv_.wait(lock, [this] { return works_ == 0; });
  writer_worker_.join();
  network_workers_.join();
  parser_workers_.join();
}

void Crawler::start_of_work_() {
  std::lock_guard lock{works_mut_};
  works_++;
  works_cv_.notify_all();
}

void Crawler::end_of_work_() {
  std::lock_guard lock{works_mut_};
  if (--works_ == 0) writer_cv_.notify_one();
  works_cv_.notify_all();
}

void Crawler::downloader(const std::string& url, const size_t& depth) {
  start_of_work_();

  asio::post(network_workers_, [this, url, depth] {
    auto host_and_target = get_host_and_target(url);
    if (host_and_target) {
      auto response =
          download_http(host_and_target->first, "80", host_and_target->second);
      parser(response, depth);
    }

    end_of_work_();
  });
}

void Crawler::parser(const http::response<http::string_body>& response,
                     const size_t& depth) {
  start_of_work_();

  asio::post(parser_workers_, [this, response, depth] {
    auto parse_child = depth < depth_;
    auto image_urls = parse_image(response);
    for (const auto& image_url : image_urls) {
      start_of_work_();
      std::lock_guard lock{writer_mut_};
      writer_queue_.push(image_url);
      writer_cv_.notify_one();
    }
    if (parse_child) {
      auto const next_depth = depth + 1;
      auto child_urls = parse_children(response);
      for (const auto& child_url : child_urls)
        downloader(child_url, next_depth);
    }
    end_of_work_();
  });
}
