#include <array>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

const std::string CRLF{"\x0d\x0a"};

using Opts = std::unordered_map<std::string, std::string>;

bool debug{true};

const std::string id_1st_chars{
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"_"};
const std::string id_chars{id_1st_chars + "0123456789"};

const std::set<std::string> c_keywords{
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline",
    "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
    "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local",
    //Not actually keywords, but usually used instead of underscore keywords:
    "alignas", "alignof", "bool", "complex", "imaginary", "noreturn",
    "static_assert", "thread_local",
    //Weird...
    "_Pragma",
    //Extensions
    "asm", "fortran",
};

const std::set<std::string> cpp_keywords{
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
    "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
    "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
    "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
    "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
    "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
    "protected", "public", "register", "reinterpret_cast", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "template", "this", "thread_local", "throw", "true",
    "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
    "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
    
    //Identifiers with special meaning
    "override", "final", "import", "module",
    
    //Weird
    "_Pragma",
};

//Is there a better way to do this?
const std::set<std::string> set_union(
        const std::set<std::string>& a,
        const std::set<std::string>& b)
{
    std::set<std::string> c{a};
    c.insert(b.begin(), b.end());
    return c;
}

const std::set<std::string> all_keywords{set_union(c_keywords, cpp_keywords)};

//This class is both a base class and used for text with default formatting.
class Text_run {
    public:
        static std::string escape(const std::string& text);

        Text_run() {}
        Text_run(const std::string& text): text_(text) {}
        std::string get_text() const { return text_; }

        void print(std::ostream& out) const
        {
            if (debug) {
                std::cerr << typeid(*this).name() << ": \"" << text_ << "\"" <<
                    '\n';
            }
            print_(out);
        }

    protected:
        virtual void print_(std::ostream& out) const
        {
            out << escape(text_);
        }

    private:
        std::string text_;
};

class Char_literal: public Text_run {
    public:
        Char_literal(const std::string& text)
            : Text_run(text.substr(1, text.size() - 2)) {}
    protected:
        void print_(std::ostream& out) const override
        {
            out << '\'';
            out << "{\\f1\\i ";
            out << escape(get_text());
            out << "}";
            out << '\'';
        }
};

class String_literal: public Text_run {
    public:
        String_literal(const std::string& text)
            : Text_run(text.substr(1, text.size() - 2)) {}
    protected:
        void print_(std::ostream& out) const override
        {
            out << '"';
            out << "{\\f1\\i ";
            out << escape(get_text());
            out << "}";
            out << '"';
        }
};

class Single_line_comment: public Text_run {
    public:
        Single_line_comment(const std::string& text)
            : Text_run(text.substr(2)) {}
    protected:
        void print_(std::ostream& out) const override
        {
            out << "//";
            out << "{\\f1\\i ";
            out << escape(get_text());
            out << "}";
        }
};

class Multiline_comment: public Text_run {
    public:
        enum Kind { complete, start, middle, end };
        Multiline_comment(Kind kind, const std::string& text)
            : Text_run(text), kind_(kind) {}
    protected:
        void print_(std::ostream& out) const override
        {
            if ((kind_ == complete) || (kind_ == start)) out << "/*";
            out << "{\\f1\\i ";
            out << escape(get_text());
            out << "}";
            if ((kind_ == complete) || (kind_ == end)) out << "*/";
        }
    private:
        Kind kind_;
};

//Identifiers that aren't preproc directives or keywords.
class Identifier: public Text_run {
    public:
        Identifier(const std::string& text)
            : Text_run(text) {}
    protected:
        void print_(std::ostream& out) const override
        {
            out << "{\\f1\\i ";
            out << escape(get_text());
            out << "}";
        }
};

class Keyword: public Text_run {
    public:
        Keyword(const std::string& text): Text_run(text) {}
    protected:
        void print_(std::ostream& out) const override
        {
            out << "{\\f1\\b ";
            out << escape(get_text());
            out << "}";
        }
};

std::string Text_run::escape(const std::string& in)
{
    std::ostringstream out;
    for (auto i: in) {
        if ((i == '\\') || (i == '{') || (i == '}'))
            out << '\\';
        out << i;
    }
    return out.str();
}

//Find the end of a string literal.
std::string::size_type find_end(
		const std::string& s,
		std::string::size_type start,
		char end_char)
{
	bool ignore_next = true;
	for (auto pos(start); pos < s.size(); ++pos) {
		if (!ignore_next) {
			if (s[pos] == end_char) return pos;
			if (s[pos] == '\\') ignore_next = true;
		} else {
			ignore_next = false;
		}
	}
	return std::string::npos;
}

static bool contains(
        const std::set<std::string>& set,
        const std::set<std::string>::key_type& x)
{
    //Could write this as a template, but I'm being lazy.
    return !(set.find(x) == set.end());
}

bool in_multiline_comment = false;

void parse(std::vector<std::unique_ptr<Text_run>>& list,
        const std::string& line, bool first = false)
{
    if (in_multiline_comment) {
        auto end(line.find("*/"));
        if (end != std::string::npos) {
            list.push_back(std::unique_ptr<Text_run>(
                        new Multiline_comment(
                            Multiline_comment::end,
                            line.substr(0, end))));
            in_multiline_comment = false;
            parse(list, line.substr(end + 2));
        } else {
            list.push_back(std::unique_ptr<Text_run>(
                        new Multiline_comment(
                            Multiline_comment::middle,
                            line)));
        }
        return;
    }

    //Scan for string literals, char literals, and comments.
    auto dquote(line.find('"'));
    auto squote(line.find('\''));
    auto slashslash(line.find("//"));
    auto slashstar(line.find("/*"));

    //Figure out which one came first.
    std::array<decltype(dquote), 4> specials =
    { dquote, squote, slashslash, slashstar };
    std::sort(specials.begin(), specials.end());
    const auto first_special = specials[0];

    //Handle the first special, if any.
    if (first_special != std::string::npos) {
        if (first_special == squote) {
            auto end(find_end(line, squote, '\''));
            parse(list, line.substr(0, squote), first);
            list.push_back(std::unique_ptr<Text_run>(new Char_literal(
                            line.substr(squote, (end - squote) + 1))));
            if (end != std::string::npos) parse(list, line.substr(end + 1));
            return;
        } else if (first_special == dquote) {
            //Find end of string literal.
            auto end(find_end(line, dquote, '"'));
            //Parse before literal.
            parse(list, line.substr(0, dquote), first);
            //Add literal
            list.push_back(std::unique_ptr<Text_run>(new String_literal(
                            line.substr(dquote, (end - dquote) + 1))));
            //Parse after literal.
            if (end != std::string::npos) parse(list, line.substr(end + 1));
            //Done with this line.
            return;
        } else if (first_special == slashslash) {
            //Parse beginning of line
            parse(list, line.substr(0, slashslash), first);
            //Add comment
            list.push_back(std::unique_ptr<Text_run>(
                        new Single_line_comment(line.substr(slashslash))));
            //Done with this line.
            return;
        } else if (first_special == slashstar) {
            parse(list, line.substr(0, slashstar), first);
            auto end(line.find("*/", slashstar + 2));
            if (end != std::string::npos) {
                list.push_back(std::unique_ptr<Text_run>(
                            new Multiline_comment(
                                Multiline_comment::complete,
                                line.substr(slashstar + 2,
                                    (end - slashstar) - 2))));
                parse(list, line.substr(end + 2));
            } else {
                list.push_back(std::unique_ptr<Text_run>(
                            new Multiline_comment(
                                Multiline_comment::start,
                                line.substr(slashstar + 2))));
                in_multiline_comment = true;
            }
            return;
        }
    }
    //At this point, we've gotten all the char/string literals and comments.

    if (first) {
        //Check for preproc directive
        //[ \t]*#[ \t]*[A-Za-z]+
        //Not a single object...
        std::string::size_type pound = line.find_first_not_of(" \t");
        if ((pound != std::string::npos) && (line[pound] == '#')) {
            auto start = line.find_first_not_of(
                    " \t", pound + 1);
            auto end = line.find_first_not_of(id_1st_chars,
                    start + 1);
            list.push_back(std::unique_ptr<Text_run>(
                        new Text_run(line.substr(0, start))));
            list.push_back(std::unique_ptr<Text_run>(
                        new Keyword(line.substr(start,
                                end - start))));
            if (end != std::string::npos) {
                parse(list, line.substr(end), false);
            }
            return;
        }
    }

    //Find identifiers
    auto id_start = line.find_first_of(id_1st_chars);
    if (id_start != std::string::npos) {
        parse(list, line.substr(0, id_start), first);
        auto id_end = line.find_first_not_of(id_chars, id_start + 1);
        std::string id = line.substr(id_start, id_end - id_start);
        //Check if identifier is a keyword.
        std::unique_ptr<Text_run> p;
        if (contains(all_keywords, id)) {
            p.reset(new Keyword(id));
        } else {
            p.reset(new Identifier(id));
        }
        list.push_back(std::move(p));
        if (id_end != std::string::npos) {
            parse(list, line.substr(id_end), false);
        }
        return;
    }

    list.push_back(std::unique_ptr<Text_run>(new Text_run(line)));
}

void process_line(std::ostream& out, const std::string& line)
{
    std::vector<std::unique_ptr<Text_run>> list;
    parse(list, line, true);
    for (auto const& i: list) i->print(out);
    //Write line break.
    out << "\\line" << CRLF;
}

std::string get_opt(
        const Opts& opts,
        const std::string& key,
        const std::string& default_value)
{
    auto iter{opts.find(key)};
    if (opts.end() == iter) return default_value;
    return iter->second;
}

void process(const Opts& opts, std::ostream& out, std::istream& in)
{
    std::string mono{get_opt(opts, "mono", "Courier")};
    std::string prop{get_opt(opts, "prop", "Times")};

    //Write header.
    out << "{\\rtf1\\ansi" << CRLF;
    out << "{\\fonttbl\\f0\\fmodern " << mono
        << ";\\f1\\froman " << prop << ";}" << CRLF;
    out << "{\\f0 ";

    //Process lines.
    std::string line;
    while (std::getline(in, line)) {
        process_line(out, line);
    }

    //Write footer.
    out << "}}" << CRLF;
}

bool starts_with(std::string_view look_in, std::string_view look_for)
{
    return (look_in.size() >= look_for.size()) and
        (0 == look_in.compare(0, look_for.size(), look_for));
}

void usage(std::string_view name)
{
    std::cout << "usage: " << name << " [--option=value ...] [files ...]\n";
    std::cout << "--mono=<name>\tMonospaced font name\n";
    std::cout << "--prop=<name>\tProportional font name\n";
    std::exit(EXIT_SUCCESS);
}

struct Parsed_args {
    Opts opts;
    std::vector<std::string> args;
};

void dump_args(const Parsed_args& args)
{
    std::cerr << "Opts:\n";
    for (const auto& opt: args.opts) {
        std::cerr << '\t' << opt.first << '=' << opt.second << '\n';
    }
    std::cerr << "Args:\n";
    for (const auto& arg: args.args) {
        std::cerr << '\t' << arg << '\n';
    }
}

/*
 * If I'm not going to use a library for this, let's keep it simple.
 * Options always start with "--".
 * If an option takes a value, it is always provided via '-'.
 */
Parsed_args parse_args(int argc, const char** argv)
{
    Parsed_args parsed;
    std::vector<std::string_view> args{argv + 1, argv + argc};
    for (std::string_view arg: args) {
        if ("--help" == arg) {
            usage(argv[0]); //Doesn't return
        }

        if (starts_with(arg, "--")) {
            arg.remove_prefix(2);
            auto equals{arg.find('=')};
            if (arg.npos != equals) {
                std::string key{arg.substr(0, equals)};
                std::string value{arg.substr(equals + 1)};
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                parsed.opts[key] = value;
            } else {
                std::string key{arg};
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                parsed.opts[key] = "";
            }
        } else {
            parsed.args.push_back(std::string{arg});
        }
    }
    return parsed;
}

int main(int argc, const char** argv)
{
    Parsed_args args{parse_args(argc, argv)};
    dump_args(args);
    process(args.opts, std::cout, std::cin);
    return EXIT_SUCCESS;
}

