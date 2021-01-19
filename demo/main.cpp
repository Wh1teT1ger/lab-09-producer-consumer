// Copyright 2020 Burylov DEnis <burylov01@mail.ru>

#include <boost/program_options.hpp>
#include <header.hpp>

namespace po = boost::program_options;
namespace asio = ::boost::asio;
using tcp = asio::ip::tcp;
namespace beast = ::boost::beast;
namespace http = beast::http;

int main(int argc, char** argv) {
  po::options_description desc{"Options"};
  po::variables_map vm;
  std::string url, output;
  size_t depth, network_threads, parser_threads;
  desc.add_options()("help", "Shows this help message")(
      "url", po::value<::std::string>(&url), "HTML page address")(
      "depth", po::value<::std::size_t>(&depth)->default_value(1),
      "Page search depth")(
      "network_threads",
      po::value<::std::size_t>(&network_threads)
          ->default_value(std::thread::hardware_concurrency()),
      "Number of downloader-threads")(
      "parser_threads",
      po::value<::std::size_t>(&parser_threads)
          ->default_value(std::thread::hardware_concurrency()),
      "Number of parser-threads")("output", po::value<::std::string>(&output),
                                  "Path to output file");
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }
  if (!vm.count("url")) {
    std::cerr << "Missing option url" << ::std::endl;
    return 1;
  }
  if (!vm.count("output")) {
    std::cerr << "Missing option output" << ::std::endl;
    return 1;
  }

  Crawler crawler{depth, url, network_threads, parser_threads, output};
  crawler.join();
}
