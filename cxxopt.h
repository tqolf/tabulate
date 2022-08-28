/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Three APIs provided:
//
// 1) Simple functional api `getarg(...)`.
//    - No initialization required: (argc, argv) pair automatically retrieved.
//    - First argument is default option value, then all option indentifiers follow.
//
// int main() {
//     bool help = getarg( false, "-h", "--help", "-?" );
//     int version = getarg( 0, "-v", "--version", "--show-version" );
//     int depth = getarg( 1, "-d", "--depth", "--max-depth");
//     std::string file = getarg( "", "-f", "--file" );
//     [...]
// }
//
// 2) Simple OOP map-based api `cxxopt::parser class`. Initialization (argc, argv) pair required.
//
//    This cxxopt::parser class is a std::map replacement where key/value are std::string types.
//    Given invokation './app.out --user=me --pass=123 -h' this class delivers not only:
//    map[0] = "./app.out", map[1] = "--user=me", map[2]="--pass=123", map[3]='-h'
//    but also, map["--user"]="me", map["--pass"]="123" and also, map["-h"]=true
//
//    Additional API:
//    - .cmdline() for a print app invokation string
//    - .str() for pretty map printing
//    - .size() number of arguments (equivalent to argc), rather than std::map.size()
//
// int main( int argc, const char **argv ) {
//     cxxopt::parser args( argc, argv );
//     if( args.has("-h") || args.has("--help") || args.has("-?") || args.size() == 1 ) {
//         std::cout << args["0"] << " [-?|-h|--help] [-v|--version] [--depth=number]" << std::endl;
//         return 0;
//     }
//     if( args.has("-v") || args.has("--version") ) {
//         std::cout << args["0"] << " sample v1.0.0. Compiled on " << __DATE__ << std::endl;
//     }
//     if( args.has("--depth") ) {
//         int depth = atoi( args["--depth"].c_str() );
//         std::cout << "depth set to " << depth << std::endl;
//     }
//     [...]
// }
//
// 3) Full OOP api `cxxopt::Options class`. Initialization (argc, argv) pair required.
//

#pragma once

#include <map>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <exception>
#include <algorithm>

#include <stdio.h>
#include <string.h>

#define CXXOPT_TRACE(fmt, ...) // printf("%d: " fmt "\n", __LINE__, ##__VA_ARGS__)

#if defined(__APPLE__)
extern "C" {
int *_NSGetArgc(void);
char ***_NSGetArgv(void);
};
#elif defined(_WIN32)
#include <io.h>
#include <winsock2.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#else
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif

namespace cxxopt
{
namespace details
{
// string convertion

inline std::string trim(std::string s)
{
    std::string trimed = s;
    trimed.erase(trimed.begin(), std::find_if(trimed.begin(), trimed.end(), [](unsigned char ch) {
                     return !std::isspace(ch);
                 }));

    trimed.erase(std::find_if(trimed.rbegin(), trimed.rend(),
                              [](unsigned char ch) {
                                  return !std::isspace(ch);
                              })
                     .base(),
                 trimed.end());
    return trimed;
}

inline bool split(
    const std::string &self, const std::string &delimiters, std::function<bool(const std::string &)> consume_token,
    std::function<bool(const std::string &)> consume_delimiter = std::function<bool(const std::string &)>{})
{
    std::string token;
    bool has_quotes = delimiters.find('"') != std::string::npos, in_token = false;
    for (auto &ch : self) {
        if (ch == '"' && !has_quotes) {
            in_token = !in_token;
        }
        if (!in_token && delimiters.find_first_of(ch) != std::string::npos) {
            if (token.size()) {
                if (consume_token && !consume_token(token)) return false;
                token.clear();
            }
            if (consume_delimiter) {
                consume_delimiter(std::string(1, ch));
            }
        } else {
            token += ch;
        }
    }
    if (!token.empty() && consume_token) {
        return consume_token(token);
    }
    return true;
}

inline void derive(const std::string &self, bool &to)
{
    std::string formated = trim(self);
    const char *p = formated.c_str();

    char l = *p | ' ';
    if ((l == 't' && strncmp(++p, "rue\0", 4) == 0) || strncmp(p, "1\0", 2) == 0) {
        to = true;
    } else if (formated.empty() || (l == 'f' && strncmp(++p, "alse\0", 5) == 0) || strncmp(p, "0\0", 2) == 0) {
        to = false;
    } else {
        throw std::runtime_error("Not boolean value: \"" + self + "\"");
    }
}

inline void derive(const std::string &self, std::string &to)
{
    to = self;
}

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type * = nullptr>
inline void derive(const std::string &self, T &to)
{
    std::string formated = trim(self);
    if (formated.empty()) {
        to = 0;
    } else {
        bool negative = false;
        if (formated[0] == '-') {
            negative = true;
        }
        unsigned long long v = std::stoull(negative ? formated.substr(1) : formated, nullptr, 0);

        to = static_cast<T>(v * (negative ? -1 : 1));
    }
}

template <typename T>
struct has_istream {
    template <typename C>
    static std::true_type test(__typeof__(&C::operator>>));
    template <typename C>
    static std::false_type test(...);

    enum { value = (sizeof(test<T>(0)) == sizeof(sizeof(std::true_type))) };
};

template <typename T, typename std::enable_if<(std::is_arithmetic<T>::value && !std::is_integral<T>::value)
                                              || has_istream<T>::value>::type * = nullptr>
inline void derive(const std::string &self, T &to)
{
    std::stringstream in(self);
    in >> to;
    if (!in) {
        throw std::runtime_error("Unable to convert");
    }
}

void derive(const bool &from, std::string &to)
{
    to = from ? "true" : "false";
}

void derive(const char *from, std::string &to)
{
    to = std::string(from);
}

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type * = nullptr>
void derive(const T &from, std::string &to)
{
    std::stringstream ss;
    ss << from;
    to = ss.str();
}

template <typename T>
void derive(const std::vector<T> &from, std::string &to)
{
    to = "[";
    const std::string delim = ", ";
    for (auto const &e : from) {
        std::string str;
        derive(e, str);
        to += str + delim;
    }
    if (from.size() != 0) {
        to.erase(to.size() - delim.size(), delim.size());
    }
    to += "]";
}

template <typename Key, typename Value, class Hasher = std::hash<Key>>
void derive(const std::map<Key, Value, Hasher> &from, std::string &to)
{
    for (auto const &e : from) {
        std::string key, value;
        derive(e.first, key);
        derive(e.second, value);
        to += key + ": " + value + "; ";
    }
    if (to.size() >= 2) {
        to.erase(to.size() - 2, 2); // pop last delimeters
    }
}

template <typename T>
inline std::vector<T> split(const std::string &self, const std::string &delimiters)
{
    std::vector<T> tokens;
    split(self, delimiters, [&](const std::string &token) {
        T t;
        derive(token, t);
        tokens.emplace_back(t);
        return true;
    });

    return tokens;
}

template <typename T>
inline void split(const std::string &self, const std::string &delimiters, std::vector<T> &tokens)
{
    split(self, delimiters, [&](const std::string &token) {
        T t;
        derive(token, t);
        tokens.emplace_back(t);
        return true;
    });
}

inline size_t split(std::vector<std::string> &tokens, const std::string &self, const std::string &delimiters)
{
    tokens.clear();
    auto consumer = [&](const std::string &token) {
        tokens.emplace_back(token);
        return true;
    };
    split(self, delimiters, consumer, consumer);
    return tokens.size();
};

template <typename T>
inline void derive(const std::string &self, std::vector<T> &repeated)
{
    char leading = self[0];
    if (leading == '[' || leading == '{') {
        if (self.back() == leading + 2 /* ']' or '}' */) {
            split(self.substr(1, self.size() - 2), ",", repeated);
        } else {
            throw std::runtime_error("Cann't convert to array");
        }
    } else {
        split(self, ",", repeated);
    }
}

template <typename T>
inline void derive(const std::string &self, std::vector<std::vector<T>> &matrix)
{
    int depth = 0;
    const char *s = self.c_str(), *p = s;
    const char *e = self.c_str() + self.length();

    while (p <= e) {
        char leading = *p;
        if (leading == '[' || leading == '{') {
            if (depth == 0) {
                s = ++p;
            } else {
                s = p++;
            }
            depth++;
            do {
                if (*p == '[' || *p == '{') {
                    depth++;
                } else if (*p == ']' || *p == '}') {
                    depth--;
                }
                p++;
            } while (*p != '\0' && depth != 1);

            std::vector<T> vec;
            derive(std::string(s, p++ - s), vec);
            matrix.push_back(vec);

            while (*p == ',' || *p == ' ') p++;
        } else {
            p++;
        }
    }
}

template <typename Key, typename Value>
inline void derive(const std::string &self, std::map<Key, Value> &mapped)
{
    split(self, ";", [&](const std::string &pair) {
        std::vector<std::string> tokens;
        split(pair, ": \t\r\n", [&](const std::string &token) {
            tokens.emplace_back(token);
            return true;
        });
        if (tokens.size() == 2) {
            Key key;
            Value value;
            derive(tokens[0], key);
            derive(tokens[1], value);
            mapped[key] = value;
            return true;
        } else {
            throw std::runtime_error("Invalid key:value pair: " + pair);
        }
    });
}

template <typename T>
inline T as(const std::string &self)
{
    T t;
    derive(self, t);
    return t;
}

template <>
inline const char *as(const std::string &self)
{
    return self.c_str();
}
template <>
inline std::string as(const std::string &self)
{
    return self;
}

template <typename T, typename std::enable_if<!std::is_same<T, std::string>::value>::type * = nullptr>
inline std::string as(const T &from)
{
    std::string rep;
    derive(from, rep);
    return rep;
}

inline std::vector<std::string> cmdline()
{
    std::vector<std::string> args;

#if defined(__APPLE__)
    int argc = *_NSGetArgc();
    char **argv = *_NSGetArgv();
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }
#elif defined(_WIN32)
    int argc;
    auto *list = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (list != nullptr) {
        for (int i = 0; i < argc; ++i) {
            std::wstring ws(list[i]);
            args.push_back(std::string(ws.begin(), ws.end()));
        }
        LocalFree(list);
    }
#else
    std::string line;
    {
        char fname[32] = {};
        sprintf(fname, "/proc/%d/cmdline", getpid());
        std::ifstream ifs(fname);
        if (ifs.good()) {
            std::stringstream ss;
            ifs >> ss.rdbuf();
            line = ss.str();
        }
    }
    for (auto end = line.size(), i = end - end; i < end; ++i) {
        auto st = i;
        while (i < line.size() && line[i] != '\0') ++i;
        args.push_back(line.substr(st, i - st));
    }
#endif

    return args;
}
} // namespace details

struct parser : public std::map<std::string, std::string> {
    using super = std::map<std::string, std::string>;

    parser(int argc, const char **argv) : super()
    {
        // reconstruct vector
        std::vector<std::string> args(argc, std::string());
        for (int i = 0; i < argc; ++i) {
            args[i] = argv[i];
        }
        // create key=value and key= args as well
        for (auto &it : args) {
            std::vector<std::string> tokens;
            auto size = details::split(tokens, it, "=");

            if (size == 3 && tokens[1] == "=") {
                (*this)[tokens[0]] = tokens[2];
            } else if (size == 2 && tokens[1] == "=") {
                (*this)[tokens[0]] = "true";
            } else if (size == 1 && tokens[0] != argv[0]) {
                (*this)[tokens[0]] = "true";
            }
        }
        // recreate args
        while (argc--) {
            (*this)[std::to_string(argc)] = std::string(argv[argc]);
        }
    }

    parser(const std::vector<std::string> &args) : super()
    {
        std::vector<const char *> argv;
        for (auto &it : args) {
            argv.push_back(it.c_str());
        }
        *this = parser(argv.size(), argv.data());
    }

    size_t size() const
    {
        unsigned i = 0;
        while (has(std::to_string(i))) ++i;
        return i;
    }

    bool has(const std::string &op) const
    {
        return this->find(op) != this->end();
    }

    std::string str() const
    {
        std::stringstream ss;
        std::string sep;
        for (auto &it : *this) {
            ss << sep << it.first << "=" << it.second;
            sep = ',';
        }
        return ss.str();
    }

    std::string cmdline() const
    {
        std::stringstream cmd;
        std::string sep;
        // concatenate args
        for (auto end = size(), arg = end - end; arg < end; ++arg) {
            cmd << sep << this->find(std::to_string(arg))->second;
            sep = ' ';
        }
        return cmd.str();
    }
};
} // namespace cxxopt

namespace cxxopt
{
class Values : public std::enable_shared_from_this<Values> {
  public:
    std::shared_ptr<Values> clone() const
    {
        return std::make_shared<Values>(*this);
    }

    int last() const
    {
        for (int i = values.size() - 1; i >= 0; i--) {
            if (values[i].has) {
                return i;
            }
        }
        return -1;
    }

    bool has(int level = -1) const
    {
        if (level <= -1) {
            return !values.empty();
        } else if (values.size() > static_cast<unsigned>(level) && values[level].has) {
            return true;
        } else {
            return false;
        }
    }

    template <typename T = std::string>
    T get(int level = -1) const
    {
        if (level <= -1) {
            level = last();
        }
        if (has(level)) {
            CXXOPT_TRACE("GET(%d): %s", level, values[level].value.c_str());
            return details::as<T>(values[level].value);
        } else {
            CXXOPT_TRACE("GET(%d): exception", level);
            throw "Invalid Arguments: data not exists";
        }
    }

    template <typename T = std::string>
    std::shared_ptr<Values> set(T v, int level = -1)
    {
        if (level <= -1) {
            level = values.size();
        }
        if (static_cast<unsigned>(level + 1) >= values.size()) {
            values.resize(level + 1);
        }
        CXXOPT_TRACE("SET(%d): %s", level, values[level].value.c_str());
        details::derive(v, values[level].value);
        values[level].has = true;

        return shared_from_this();
    }

    std::shared_ptr<Values> set_optarg(const char *arg, int level = -1)
    {
        if (level <= -1) {
            level = values.size();
        }
        if (static_cast<unsigned>(level + 1) >= values.size()) {
            values.resize(level + 1);
        }
        values[level].optarg = arg;
        values[level].value = std::string(arg);
        values[level].has = true;

        return shared_from_this();
    }

    const char *get_optarg(int level = -1) const
    {
        if (level <= -1) {
            level = values.size();
        }

        return values.at(level).optarg;
    }

    std::shared_ptr<Values> clear(int level = -1)
    {
        if (level <= -1) {
            level = last();
        }
        if (static_cast<unsigned>(level) < values.size()) {
            values[level].has = false;
        }

        return shared_from_this();
    }

    std::shared_ptr<Values> clear_all()
    {
        for (size_t i = 0; i < values.size(); i++) {
            values[i].has = false;
        }

        return shared_from_this();
    }

    template <typename T>
    std::string format(int level = -1) const
    {
        if (std::is_same<T, std::string>::value) {
            return get(level);
        } else {
            return std::make_shared<Values>()->set<T>(get<T>(level))->get();
        }
    }

    template <typename T = std::string>
    std::string description() const
    {
        int level = last();
        if (level >= 0) {
            std::string desc;
            bool first = true;
            while (level >= 0) {
                if (has(level)) {
                    if (!first) {
                        desc += ", ";
                    }
                    first = false;
                    desc += "\"" + format<T>(level) + "\"";
                    switch (level) {
                        case 0:
                            desc += "[default]";
                            break;
                        case 1:
                            desc += "[implicit]";
                            break;
                        case 2:
                            desc += "[explicit]";
                            break;
                        default:
                            desc += "\"[" + std::to_string(level) + "]";
                            break;
                    }
                }
                level--;
            }
            return desc;
        } else {
            return "<No Values>";
        }
    }

#define DEFINE_BY_LEVEL(level, suffix)                          \
    bool has##suffix() const                                    \
    {                                                           \
        return has(level);                                      \
    }                                                           \
    std::shared_ptr<Values> clear##suffix()                     \
    {                                                           \
        return clear(level);                                    \
    }                                                           \
    template <typename T = std::string>                         \
    std::shared_ptr<Values> set##suffix(T v)                    \
    {                                                           \
        return set<T>(v, level);                                \
    }                                                           \
    template <typename T = std::string>                         \
    T get##suffix() const                                       \
    {                                                           \
        if (!has(level)) {                                      \
            CXXOPT_TRACE("exception");                          \
            throw "Invalid Argument: value not setted";         \
        }                                                       \
        return get<T>(level);                                   \
    }                                                           \
    std::shared_ptr<Values> set_optarg##suffix(const char *arg) \
    {                                                           \
        return set_optarg(arg, level);                          \
    }                                                           \
    const char *get_optarg##suffix()                            \
    {                                                           \
        return get_optarg(level);                               \
    }

    DEFINE_BY_LEVEL(0, _default)
    DEFINE_BY_LEVEL(1, _implicit)
    DEFINE_BY_LEVEL(2, _explicit)

  private:
    struct optional_value {
        bool has = false;
        std::string value;
        const char *optarg;
    };
    std::vector<optional_value> values;
};

template <typename T = void>
inline std::shared_ptr<Values> Value()
{
    return std::make_shared<Values>();
}

enum {
    NOARG = 0,    // no argument
    REQUIRED = 1, // required argument
    OPTIONAL = 2, // optional argument
};

struct Option {
    int id;
    int type;
    const char *name;
    const char *brief;
    std::shared_ptr<Values> value;

    Option(int id, int type, const char *name, const char *brief, std::shared_ptr<Values> val)
        : id(id), type(type), name(name), brief(brief), value(std::move(val))
    {
    }
};

struct Results {
    std::map<int, std::string> id_to_name;
    std::map<int, std::shared_ptr<Values>> results_by_id;
    std::map<std::string, std::shared_ptr<Values>> results_by_name;

    Results() = default;
    ~Results() = default;

    bool has(int id)
    {
        return results_by_id.count(id) > 0;
    }

    bool has(const std::string &name)
    {
        return results_by_name.count(name) > 0;
    }

    const Values &operator[](int id) const
    {
        return *results_by_id.at(id);
    }

    const Values &operator[](const std::string &name) const
    {
        return *results_by_name.at(name);
    }

    template <typename T = std::string>
    T operator()(int id)
    {
        return results_by_id[id]->get<T>(-1);
    }

    template <typename T = std::string>
    T operator()(const std::string &name)
    {
        return results_by_name[name]->get<T>(-1);
    }

    std::string description(int indent = 0) const
    {
        std::string info;
        for (auto item : results_by_id) {
            info += std::string(indent, ' ');
            if (isprint(item.first)) {
                info += "-";
                info += static_cast<char>(item.first);
            }
            if (id_to_name.count(item.first)) {
                if (isprint(item.first)) {
                    info += ", ";
                }
                info += "--" + id_to_name.at(item.first);
            }
            info += ": " + item.second->description() + "\n";
        }
        return info;
    }
};

class Options {
  public:
    Options() {}
    Options(std::string program, std::string description) : program(program), description(description) {}

    Option &add(const char *opts, const char *brief, std::shared_ptr<Values> val, int type,
                const std::string &group = "")
    {
        int id;
        const char *longopt = nullptr;

        {
            const char *p = opts;
            if (isalnum(*p) && (*(p + 1) == ',' || *(p + 1) == '\0')) {
                id = *p++;
                if (*p != '\0') {
                    p++;
                }
            } else {
                id = automatic_id++;
            }
            while (*p == ' ') {
                p++;
            }

            if (isalnum(*p)) {
                const char *store = p++;
                while (isalnum(*p) || *p == '-' || *p == '_') {
                    p++;
                }
                if (*p == '\0') {
                    longopt = store;
                } else {
                    CXXOPT_TRACE("exception");
                    throw "Invalid Arguments: option format error";
                }
            }
        }

        auto &options = options_by_group[group];
        options.emplace_back(id, type, longopt, brief, std::move(val));

        return options[options.size() - 1];
    }

    class GroupAdder {
      public:
        GroupAdder(Options &options, std::string group) : options_(options), group_(std::move(group)) {}
        GroupAdder &operator()(const char *opts, const char *brief)
        {
            options_.add(opts, brief, Value(), NOARG, group_);
            return *this;
        }

        GroupAdder &operator()(const char *opts, const char *brief, std::shared_ptr<Values> val, int type = OPTIONAL)
        {
            options_.add(opts, brief, val, type, group_);
            return *this;
        }

      private:
        Options &options_;
        std::string group_;
    };
    GroupAdder add_group(std::string group = "")
    {
        return GroupAdder(*this, group);
    }

    class Parser {
      public:
        Parser(int argc, char **argv, const std::vector<Option> &options)
            : argc(argc), argv(argv), options(options), optarg(nullptr), optind(0), opterr(0), optwhere(0)
        {
        }

        const Option *next()
        {
            /* first, deal with silly parameters and easy stuff */
            if (argc == 0 || argv == nullptr || options.size() == 0 || optind >= argc || argv[optind] == nullptr) {
                opterr = EOF;
                return nullptr;
            }
            if (strcmp(argv[optind], "--") == 0) {
                optind++;
                opterr = EOF;
                CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                return nullptr;
            }
            CXXOPT_TRACE("START: %s", debuginfo().c_str());

            /* if this is our first time through */
            if (optind == 0) {
                optind = optwhere = 1;
            }
            int nonopts_index = 0, nonopts_count = 0, nextarg_offset = 0;

            /* find our next option, if we're at the beginning of one */
            if (optwhere == 1) {
                nonopts_index = optind;
                nonopts_count = 0;
                while (!is_option(argv[optind])) {
                    CXXOPT_TRACE("skip non-option `%s`", argv[optind]);
                    optind++;
                    nonopts_count++;
                }
                if (argv[optind] == nullptr) {
                    /* no more options */
                    CXXOPT_TRACE("no more options");
                    optind = nonopts_index;
                    opterr = EOF;
                    CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                } else if (strcmp(argv[optind], "--") == 0) {
                    /* no more options, but have to get `--' out of the way */
                    CXXOPT_TRACE("no more options, but have to get `--` out of the way");
                    permute(argv + nonopts_index, nonopts_count, 1);
                    optind = nonopts_index + 1;
                    opterr = EOF;
                    CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                }
                CXXOPT_TRACE("next option `%s`", argv[optind]);
            }

        try_again:
            /* End of option list? */
            if (argv[optind] == nullptr) {
                opterr = EOF;
                optind -= nonopts_count;
                CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                return nullptr;
            }
            CXXOPT_TRACE("got an option `%s`, where = %d", argv[optind], optwhere);

            /* we've got an option, so parse it */
            int match_index = -1;
            char *possible_arg = 0;
            const Option *matched = nullptr;

            /* first, is it a long option? */
            if (memcmp(argv[optind], "--", 2) == 0 && optwhere == 1) {
                CXXOPT_TRACE("long option `%s`", argv[optind]);
                optwhere = 2;
                match_index = -1;
                possible_arg = strchr(argv[optind] + optwhere, '=');
                size_t match_chars = 0;
                if (possible_arg == nullptr) {
                    /* no =, so next argv might be arg */
                    CXXOPT_TRACE("no =, so next argv might be arg");
                    match_chars = strlen(argv[optind]);
                    possible_arg = argv[optind] + match_chars;
                    match_chars = match_chars - optwhere;
                } else {
                    CXXOPT_TRACE("possible_arg `%s`", possible_arg);
                    match_chars = (possible_arg - argv[optind]) - optwhere;
                }
                for (decltype(options.size()) optindex = 0; optindex < options.size(); ++optindex) {
                    if (options[optindex].name != nullptr
                        && memcmp(argv[optind] + optwhere, options[optindex].name, match_chars) == 0) {
                        /* do we have an exact match? */
                        if (match_chars == strlen(options[optindex].name)) {
                            CXXOPT_TRACE("have an exact matched option `%s`", argv[optind] + optwhere);
                            match_index = optindex;
                            matched = &options[match_index];
                            break;
                        } else {
                            CXXOPT_TRACE("haven't an exact match, option `%s`, given `%s`", argv[optind] + optwhere,
                                         options[optindex].name);
                        }
                    }
                }
                if (match_index < 0) {
                    CXXOPT_TRACE("skip options `%s`, optind = %d", argv[optind], optind);
                    optind++;
                    nonopts_count++;
                    optwhere = 1;

                    goto try_again;
                }
            } else if (argv[optind][0] == '-') {
                /* if we didn't find a long option, is it a short option? */
                auto it = std::find_if(options.begin(), options.end(), [&](const Option &opt) {
                    return opt.id == argv[optind][optwhere];
                });
                if (it == options.end()) {
                    /* couldn't find option in shortopts */
                    CXXOPT_TRACE("couldn't find option in shortopts: `%c`", argv[optind][optwhere]);
                    optwhere++;
                    if (argv[optind][optwhere] == '\0') {
                        CXXOPT_TRACE("end of short options `%s`", argv[optind] + 1);
                        optind++;
                        optwhere = 1;
                    }
                    opterr = '?';
                    CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                    return nullptr;
                }
                match_index = std::distance(options.begin(), it);
                possible_arg = argv[optind] + optwhere + 1;
                matched = &options[match_index];
                CXXOPT_TRACE("match short option: `%c`, possible_arg = %s", argv[optind][optwhere], possible_arg);
            }

            /* get argument and reset optwhere */
            if (matched != nullptr) {
                switch (matched->type) {
                    case OPTIONAL:
                        if (*possible_arg == '=') {
                            possible_arg++;
                        }
                        optarg = (*possible_arg != '\0') ? possible_arg : 0;
                        optwhere = 1;
                        CXXOPT_TRACE("(OPTIONAL)arg = `%s`", optarg ? optarg : "<null>");
                        break;
                    case REQUIRED:
                        if (*possible_arg == '=') {
                            possible_arg++;
                        }
                        if (*possible_arg != '\0') {
                            optarg = possible_arg;
                            optwhere = 1;
                        } else if (optind + 1 >= argc) {
                            optind++;
                            opterr = ':';
                            CXXOPT_TRACE("END: %s\n", debuginfo().c_str());
                            return nullptr;
                        } else {
                            optarg = argv[optind + 1];
                            nextarg_offset = 1;
                            optwhere = 1;
                        }
                        CXXOPT_TRACE("(REQUIRED)arg = `%s`", optarg);
                        break;
                    default: /* shouldn't happen */
                    case NOARG:
                        CXXOPT_TRACE("(NOARG) %c(where = %d)", argv[optind][optwhere], optwhere);
                        optwhere++;
                        if (argv[optind][optwhere] == '\0') {
                            optwhere = 1;
                        }
                        optarg = 0;
                        break;
                }
            } else {
                optwhere = 1;
            }

            /* do we have to permute or otherwise modify optind? */
            CXXOPT_TRACE("num-nonopts = %d", nonopts_count);
            if (nonopts_count != 0) {
                CXXOPT_TRACE("(PERMUTE)from = %d, length = %d, gap = %d]", nonopts_index, nonopts_count,
                             1 + nextarg_offset);
                CXXOPT_TRACE("before permute: %s", debuginfo().c_str());
                permute(argv + nonopts_index, nonopts_count, 1 + nextarg_offset);
                optind = nonopts_index + 1 + nextarg_offset;
                CXXOPT_TRACE("after permute: %s", debuginfo().c_str());
            } else if (optwhere == 1) {
                optind = optind + 1 + nextarg_offset;
            }
            CXXOPT_TRACE("END: %s\n", debuginfo().c_str());

            /* finally return */
            opterr = 0;
            return matched;
        }

        int ind() const
        {
            return optind;
        }

        int err() const
        {
            return opterr;
        }

        char *arg() const
        {
            return optarg;
        }

        std::string debuginfo() const
        {
            std::string info;
            for (int i = 1; i < argc; i++) {
                if (i == optind && optwhere == 1) {
                    info += "^";
                    info += argv[i];
                } else {
                    info += argv[i];
                }
                info += " ";
            }
            info.pop_back();

            return info;
        }

      private:
        int argc;
        char **argv;
        const std::vector<Option> &options;

        char *optarg;
        int optind, opterr, optwhere;

        static void reverse(char **argv, int num)
        {
            for (int i = 0; i < (num >> 1); i++) {
                char *tmp = argv[i];
                argv[i] = argv[num - i - 1];
                argv[num - i - 1] = tmp;
            }
        }

        static void permute(char *const argv[], int len1, int len2)
        {
            reverse((char **)argv, len1);
            reverse((char **)argv, len1 + len2);
            reverse((char **)argv, len2);
        }

        /* is this argv-element an option or the end of the option list? */
        int is_option(char *arg)
        {
            return (arg == nullptr) || (arg[0] == '-');
        }
    };

    Results parse(int &argc, char **&argv)
    {
        std::vector<Option> all_options;
        for (auto const &group : options_by_group) {
            all_options.insert(all_options.end(), group.second.begin(), group.second.end());
        }

        Results ret;
        const Option *opt;
        Parser parser(argc, argv, all_options);
        while (parser.err() != EOF) {
            if ((opt = parser.next()) == nullptr) {
                CXXOPT_TRACE("cann't get an option");
                continue;
            }
            if (!ret.has(opt->id)) {
                auto val = opt->value->clone();
                ret.results_by_id[opt->id] = val;
                if (opt->name != nullptr) {
                    ret.results_by_name[opt->name] = val;
                }
            }
            if (opt->type != NOARG) {
                if (parser.arg() != nullptr) {
                    CXXOPT_TRACE("arg = %s", parser.arg());
                    ret.results_by_id[opt->id]->set_optarg_explicit(parser.arg());
                    if (opt->name != nullptr) {
                        ret.results_by_name[opt->name]->set_optarg_explicit(parser.arg());
                    }
                }
            }
        }

        for (auto &group : options_by_group) {
            for (auto &opt : group.second) {
                if (opt.value->has_default() && !ret.has(opt.id)) {
                    auto val = opt.value->clone()->clear_implicit();
                    ret.results_by_id[opt.id] = val;
                    if (opt.name != nullptr) {
                        ret.results_by_name[opt.name] = val;
                    }
                }
                if (opt.name != nullptr && opt.value->has()) {
                    ret.id_to_name[opt.id] = opt.name;
                }
            }
        }
        argc -= parser.ind();
        argv += parser.ind();

        return ret;
    }

    std::string usage() const
    {
        std::string str;
        str += program + " - " + description + "\n\n";
        str += "Mandatory arguments to long options are madatory for short options too.\n\n";

        auto headlen = head_length();
        if (options_by_group.count("")) {
            str += group_usage(options_by_group.at(""), headlen) + "\n";
        }

        for (auto const &group : options_by_group) {
            if (group.first == "") {
                continue;
            }
            str += " " + group.first + " options\n";
            str += group_usage(group.second, headlen) + "\n";
        }

        return str;
    }

  private:
    int automatic_id = 10000;
    std::string program, description;
    std::map<std::string, std::vector<Option>> options_by_group;

    size_t head_length() const
    {
        size_t headlen = 0;
        for (auto const &group : options_by_group) {
            for (auto const &opt : group.second) {
                if (opt.name != nullptr) {
                    headlen = std::max(headlen, strlen(opt.name));
                }
            }
        }
        headlen += 10; /* -x, --xxxx [+:.] */

        return headlen;
    }

    std::string group_usage(const std::vector<Option> &options, size_t headlen) const
    {
        std::string str;
        for (auto const &opt : options) {
            size_t len = headlen;
            str += std::string(2, ' ');
            if (std::isprint(opt.id)) {
                str += '-';
                str += static_cast<char>(opt.id);
                len -= 2;

                if (opt.name != nullptr) {
                    str += ", ";
                    len -= 2;
                }
            } else {
                str += "    ";
                len -= 4;
            }

            if (opt.name != nullptr) {
                str += "--";
                str += opt.name;
                len -= 2 + strlen(opt.name);
            }

            str += std::string(len - 4, ' ');

            str += ' ';
            str += '[';
            str += ".*:"[opt.type];
            str += ']';
            len -= 4;

            if (opt.brief != nullptr) {
                str += "   ";
                {
                    std::string line;
                    std::stringstream ss(opt.brief);
                    while (std::getline(ss, line, '\n')) {
                        if (str[str.length() - 1] == '\n') {
                            str += std::string(2 + headlen + 3, ' ');
                        }
                        str += line + "\n";
                    }
                }
            } else {
                str += '\n';
            }
        }
        return str;
    }
};
} // namespace cxxopt

template <typename T>
inline T getarg(const T &defaults, const char *argv)
{
    static struct cxxopt::parser args(cxxopt::details::cmdline());
    return args.has(argv) ? cxxopt::details::as<T>(args[argv]) : defaults;
}

template <typename T, typename... Args>
inline T getarg(const T &defaults, const char *arg0, Args... argv)
{
    T t = getarg<T>(defaults, arg0);
    return t == defaults ? getarg<T>(defaults, argv...) : t;
}

inline bool hasarg(const char *argv)
{
    static struct cxxopt::parser args(cxxopt::details::cmdline());
    return args.has(argv);
}

template <typename... Args>
inline bool hasarg(const char *arg0, Args... argv)
{
    return hasarg(arg0) ? true : hasarg(argv...);
}

inline const char *getarg(const char *defaults, const char *argv)
{
    static struct cxxopt::parser args(cxxopt::details::cmdline());
    return args.has(argv) ? cxxopt::details::as<const char *>(args[argv]) : defaults;
}

template <typename... Args>
inline const char *getarg(const char *defaults, const char *arg0, Args... argv)
{
    const char *t = getarg(defaults, arg0);
    return t == defaults ? getarg(defaults, argv...) : t;
}
