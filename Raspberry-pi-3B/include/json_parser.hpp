// File: json_parser.hpp
#ifndef JSON_PARSER_HPP
#define JSON_PARSER_HPP

#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cctype>

class SimpleJSON {
public:
    using Object = std::unordered_map<std::string, std::string>;
    
    static Object parse(const std::string& json) {
        Object result;
        std::string cleaned = json;
        
        // Remove whitespace and braces
        cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), 
            [](char c) { return c == '{' || c == '}' || c == '\n' || c == '\r'; }), cleaned.end());
        
        // Parse key-value pairs
        std::stringstream ss(cleaned);
        std::string pair;
        
        while (std::getline(ss, pair, ',')) {
            size_t colonPos = pair.find(':');
            if (colonPos != std::string::npos) {
                std::string key = trim(pair.substr(0, colonPos));
                std::string value = trim(pair.substr(colonPos + 1));
                
                // Remove quotes
                key.erase(std::remove(key.begin(), key.end(), '"'), key.end());
                value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
                
                result[key] = value;
            }
        }
        
        return result;
    }
    
    static std::string stringify(const Object& obj) {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        
        for (const auto& [key, value] : obj) {
            if (!first) ss << ",";
            ss << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        
        ss << "}";
        return ss.str();
    }
    
private:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t");
        return str.substr(first, (last - first + 1));
    }
};