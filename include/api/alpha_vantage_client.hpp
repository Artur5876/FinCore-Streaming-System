#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

class AlphaVantageClient {
private:
    std::string api_key_;

public:
    AlphaVantageClient(const std::string& api_key) : api_key_(api_key) {}

    //data collection struct(package)
    struct Quote {
        std::string symbol;
        double price{0.0};
        double open{0.0};
        double high{0.0};
        double low{0.0};
        long long volume{0};
        double change{0.0};
        double change_percent{0.0};
        std::string timestamp;
    };

    Quote get_global_quote(const std::string& symbol) {
        Quote quote;
        quote.symbol = symbol;

        if(symbol.empty()) {
            std::cerr << "ERROR: symbol cannot be empty!\n";
            return quote;
        }

        ///generating URL
        std::string url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol="
            + symbol + "&apikey=" + api_key_;

        //data fetch (through curl);
        std::string json = execute_curl(url);

        if (json.empty()) {
            std::cerr << "ERROR: Alpha Vantage API return an error\n";
            return quote;
        }

        //parsing JSON response;
        parse_global_quote(json, quote);

        return quote;
    }

private:
    std::string execute_curl(const std::string& url) {
        std::string command = "curl -s \"" + url + "\"";
        FILE* pipe = popen(command.c_str(), "r");
        if(!pipe) return "";

        char buffer[4096];
        std::string result;

        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer; //result accumulation
        }
        pclose(pipe);
        return result;
    }

    void parse_global_quote(const std::string& json, Quote& quote) {
        ///parsing data from the struct to .json (manually);
        parse_json_field(json, "\"02. open\": \"", quote.open);
        parse_json_field(json, "\"03. high\": \"", quote.high);
        parse_json_field(json, "\"04. low\": \"", quote.low);
        parse_json_field(json, "\"05. price\": \"", quote.price);

        //parse volume
        std::string volume_str;
        if (parse_json_string_field(json, "\"06. volume\": \"", volume_str)) {
            try {
                quote.volume = std::stoll(volume_str);
            } catch(const std::exception& e) { quote.volume = 0; }
        }

        //timestamp
        parse_json_string_field(json, "\"07. latest trading day\": \"", quote.timestamp);

        //previous close
        ////parse_json_field(json, "\"08. previous close\": \"", quote.previous_close);

        //Parse change
        parse_json_field(json, "\"09. change\": \"", quote.change);

        //change percent
        std::string change_percent_str;
        if (parse_json_string_field(json, "\"10. change percent\": \"", change_percent_str)) {
            if (!change_percent_str.empty() && change_percent_str.back() == '%') {
                change_percent_str.pop_back();
            }
            try { quote.change_percent = std::stod(change_percent_str); }
            catch (const std::exception& e) { quote.change_percent = 0.0; }
        }
    }


    bool parse_json_field(const std::string& json,
            const std::string& field_name, double& value)
    {
        size_t pos = json.find(field_name);
        if(pos == std::string::npos) return false;

        size_t start = pos + field_name.length();
        size_t end = json.find('"', start);
        if (end == std::string::npos) return false;

        std::string value_str = json.substr(start, end - start);
        try {
            value = std::stod(value_str);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool parse_json_string_field(const std::string& json,
            const std::string& field_name, std::string& value)
    {
        size_t pos = json.find(field_name);
        if (pos == std::string::npos) return false;

        size_t start = pos + field_name.length();
        size_t end = json.find('"', start);

        if(end == std::string::npos) return false;

        value = json.substr(start, end - start);

        return true;
    }
};
