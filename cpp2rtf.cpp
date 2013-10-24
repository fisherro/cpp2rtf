#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

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
    "_Thread_local"
};

const std::set<std::string> cpp_keywords{
    "asm", "dynamic_cast", "namespace", "reinterpret_cast", "try", "bool",
    "explicit", "new", "static_cast", "typeid", "catch", "false", "operator",
    "template", "typename", "class", "friend", "private", "this", "using",
    "const_cast", "inline", "public", "throw", "virtual", "delete", "mutable",
    "protected", "true", "wchar_t", "and", "bitand", "compl", "not_eq", "or_eq",
    "xor_eq", "and_eq", "bitor", "not", "or", "xor", "alignas", "alignof",
    "char16_t", "char32_t", "constexpr", "decltype", "noexcept", "nullptr",
    "static_assert", "thread_local"
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
                    std::endl;
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
        virtual void print_(std::ostream& out) const
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
        virtual void print_(std::ostream& out) const
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
        virtual void print_(std::ostream& out) const
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
        virtual void print_(std::ostream& out) const
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
        virtual void print_(std::ostream& out) const
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
        virtual void print_(std::ostream& out) const
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
    out << "\\line" << std::endl;

}

void process(std::ostream& out, std::istream& in)
{
    //Write header.
    out << "{\\rtf1\\ansi" << std::endl;
    out << "{\\fonttbl\\f0\\fmodern Courier;\\f1\\froman Times;}" <<
        std::endl;
    out << "{\\f0 ";

    //Process lines.
    std::string line;
    while (std::getline(in, line)) {
        process_line(out, line);
    }

    //Write footer.
    out << "}}" << std::endl;
}

int main(int argc, char** argv)
{
    process(std::cout, std::cin);
    return EXIT_SUCCESS;
}
