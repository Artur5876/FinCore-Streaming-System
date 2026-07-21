#include "cli/fincore_cli.hpp"

#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fincore
{
    namespace
    {
        Quote make_quote(const std::string& symbol,
                         double price = 100.0,
                         Volume volume = 1'000)
        {
            Quote quote{};
            quote.symbol = symbol;
            quote.price = price;
            quote.open = price - 1.0;
            quote.high = price + 2.0;
            quote.low = price - 2.0;
            quote.volume = volume;
            quote.change_pct = 1.25;
            quote.source = "TEST";
            return quote;
        }

        struct FakeBackend {
            bool connected{true};
            bool cached{false};
            bool quote_write_result{true};
            bool book_write_result{true};

            //history check lists that will carry data in order to
            //produce statistics(mean, median, deviation etc)
            int quote_calls{};
            int quote_write_calls{};
            int book_write_calls{};
            int connection_checks{};

            std::vector<std::string> requested_symbols;
            std::vector<std::string> quote_write_symbols;
            std::vector<std::string> book_write_symbols;

            std::map<Price, Volume> last_bids;
            std::map<Price, Volume> last_asks;

            std::function<std::optional<Quote>(const Symbol&)> quote_handler;
        };

        cli::CliServices make_services(FakeBackend& backend)
        {
            cli::CliServices services;

            services.get_quote =
                [&backend](const Symbol& symbol) {
                    ++backend.quote_calls;
                    backend.requested_symbols.push_back(symbol);

                    //if nothing is foudn than return empty Quote{}
                    if (!backend.quote_handler) {
                        return std::optional<Quote>{};
                    }

                    return backend.quote_handler(symbol);
            };

            services.last_was_cached = [&backend] {
                return backend.cached;
            };

            services.redis_is_connected =
                [&backend] {
                    ++backend.connection_checks;
                    return backend.connected;
            };

            services.store_quote =
                [&backend](const Symbol& symbol, const Quote&) {
                    ++backend.quote_write_calls;
                    backend.quote_write_symbols.push_back(symbol);
                    return backend.quote_write_result;
            };

            services.update_order_book =
                [&backend] (const Symbol& symbol,
                        const std::map<Price, Volume>& bids,
                        const std::map<Price, Volume>& asks) {
                    ++backend.book_write_calls;
                    backend.last_bids = bids;
                    backend.last_asks = asks;

                    return backend.book_write_result;
            };
            return services;
        }

        std::string run_cli(FakeBackend& backend,
                const std::string& commands,
                std::vector<std::string> symbols = {"ibm", "aapl"},
                int default_poll_seconds = 0)
        {
            std::istringstream input{commands};
            std::ostringstream output;

            cli::FinCoreCli cli{
                make_services(backend),
                std::move(symbols),
                default_poll_seconds,
                input,
                output
            };

            EXPECT_EQ(cli.run(), 0);

            return output.str();
        }



        TEST(FincoreCliTest, PrintsStartupMessageAndExits)
        {
            FakeBackend backend;
            const std::string output = run_cli(
                backend,
                "Exit\n");

            EXPECT_NE(
                output.find("FinCore Interactive CLI"),
                std::string::npos);

            EXPECT_NE(
                output.find("Type 'help' to list commands."),
                std::string::npos);

            EXPECT_NE(output.find("fincore>"), std::string::npos);
            EXPECT_NE(output.find("Bye.", std::string::npos));
        }







    }
}
