#include "format.hpp"
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <iostream>
#include <optional>
#include <string>

std::optional<std::string> formatNix(std::string_view source) {
    namespace bp = boost::process;

    std::string output;
    try {
        boost::asio::io_service ios;
        std::future<std::string> data;
        bp::child c(
            // todo: make configurable
            bp::search_path("alejandra"),
            "--quiet",
            (bp::std_in < bp::buffer(source)),
            (bp::std_out > data),
            ios
        );
        ios.run();
        c.wait();
        if (c.exit_code() != 0) {
            std::cerr << "formatter failed\n";
            return {};
        }
        output = data.get();
    } catch (bp::process_error& e) {
        std::cerr << "error running formatter: " << e.what() << "\n";
        return {};
    }

    return output;
}