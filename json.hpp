#pragma once
#include <stack>
#include <string>
#include <algorithm>
#include <functional>

namespace json
{

class exception : public std::logic_error
{
public:
    explicit exception(const char *what, std::size_t row = 0, std::size_t column = 0)
        : std::logic_error(what),
          m_row(row),
          m_column(column)
    {
    }

    std::size_t row() const
    {
        return m_row;
    }

    std::size_t column() const
    {
        return m_column;
    }

private:
    const std::size_t m_row;
    const std::size_t m_column;
};

enum class item
{
    object_begin,
    object_end,
    array_begin,
    array_end,
    key,
    string_value,
    integer_value,
    real_value,
    null_value,
    true_value,
    false_value
};

template <typename Char,
          typename Traits = std::char_traits<Char>,
          typename Allocator = std::allocator<Char>>
class basic_reader
{
public:
    using char_type = Char;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using string_type = std::basic_string<char_type, traits_type, allocator_type>;
    using callback_type =  std::function<void(json::item, const string_type &, std::size_t, std::size_t)>;

public:
    basic_reader(callback_type callback)
        : m_callback(callback)
    {
    }

    void operator ()(char_type input)
    {
        while (!scan(input))
            continue;

        move(input);
    }

    template <typename InputIterator>
    void read(InputIterator first, InputIterator last)
    {
        std::for_each(first, last, *this);
    }

private:
    enum token
    {
        object_begin_token,
        object_end_token,
        array_begin_token,
        array_end_token,
        colon_token,
        comma_token,
        string_token,
        integer_token,
        real_token,
        null_token,
        true_token,
        false_token
    };

    enum char_state
    {
        default_state,
        string_state,
        escape_state,
        unicode_1_state,
        unicode_2_state,
        unicode_3_state,
        unicode_4_state,
        integer_1_state,
        integer_n_state,
        integer_z_state,
        fractional_1_state,
        fractional_n_state,
        exponent_state,
        exponent_1_state,
        exponent_n_state,
        null_1_state,
        null_2_state,
        null_3_state,
        true_1_state,
        true_2_state,
        true_3_state,
        false_1_state,
        false_2_state,
        false_3_state,
        false_4_state
    };

    enum token_state
    {
        object_1_state,
        object_2_state,
        object_3_state,
        object_v_state,
        object_n_state,
        array_1_state,
        array_v_state,
        array_n_state,
    };

private:
    inline void move(char_type input)
    {
        if (input == '\n')
            m_column = 1, m_row++;
        else
            m_column++;
    }

    inline bool scan(char_type input)
    {
        switch (m_char_state)
        {
        case default_state:
            if (std::isspace(input))
                return next(default_state);

            if (input == '{')
                return token(object_begin_token), next(default_state);

            if (input == '}')
                return token(object_end_token), next(default_state);

            if (input == '[')
                return token(array_begin_token), next(default_state);

            if (input == ']')
                return token(array_end_token), next(default_state);

            if (input == ':')
                return token(colon_token), next(default_state);

            if (input == ',')
                return token(comma_token), next(default_state);

            if (input == '"')
                return next(string_state);

            if (input == '-')
                return take(input), next(integer_1_state);

            if (input == 'n')
                return next(null_1_state);

            if (input == 't')
                return next(true_1_state);

            if (input == 'f')
                return next(false_1_state);

            if (std::isdigit(input))
                return take(input), next(input == '0' ? integer_z_state : integer_n_state);

            break;

        case string_state:
            if (input == '"')
                return token(string_token), next(default_state);

            if (input == '\\')
                return next(escape_state);

            return take(input), next(string_state);

        case escape_state:
            if (input == '"')
                return take('"'), next(string_state);

            if (input == '\\')
                return take('\\'), next(string_state);

            if (input == '/')
                return take('/'), next(string_state);

            if (input == 'b')
                return take('\b'), next(string_state);

            if (input == 'f')
                return take('\f'), next(string_state);

            if (input == 'n')
                return take('\n'), next(string_state);

            if (input == 'r')
                return take('\r'), next(string_state);

            if (input == 't')
                return take('\t'), next(string_state);

            if (input == 'u')
                return next(unicode_1_state);

            break;

        case unicode_1_state:
            if (std::isxdigit(input))
                return unicode(input), next(unicode_2_state);

            break;

        case unicode_2_state:
            if (std::isxdigit(input))
                return unicode(input), next(unicode_3_state);

            break;

        case unicode_3_state:
            if (std::isxdigit(input))
                return unicode(input), next(unicode_4_state);

            break;

        case unicode_4_state:
            if (std::isxdigit(input))
                return unicode(input), take_unicode(), next(string_state);

            break;

        case integer_1_state:
            if (std::isdigit(input))
                return take(input), next(input == '0' ? integer_z_state : integer_n_state);

            break;

        case integer_n_state:
            if (std::isdigit(input))
                return take(input), next(integer_n_state);

            // no break

        case integer_z_state:
            if (input == '.')
                return take(input), next(fractional_1_state);

            if (input == 'e' || input == 'E')
                return take(input), next(exponent_state);

            return token(integer_token), repeat(default_state);

        case fractional_1_state:
            if (std::isdigit(input))
                return take(input), next(fractional_n_state);

            break;

        case fractional_n_state:
            if (std::isdigit(input))
                return take(input), next(fractional_n_state);

            if (input == 'e' || input == 'E')
                return take(input), next(exponent_state);

            return token(real_token), repeat(default_state);

        case exponent_state:
            if (std::isdigit(input))
                return take(input), next(exponent_n_state);

            if (input == '-' || input == '+')
                return take(input), next(exponent_1_state);

            break;

        case exponent_1_state:
            if (std::isdigit(input))
                return take(input), next(exponent_n_state);

            break;

        case exponent_n_state:
            if (std::isdigit(input))
                return take(input), next(exponent_n_state);

            return token(real_token), repeat(default_state);

        case null_1_state:
            if (input == 'u')
                return next(null_2_state);

            break;

        case null_2_state:
            if (input == 'l')
                return next(null_3_state);

            break;

        case null_3_state:
            if (input == 'l')
                return token(null_token), next(default_state);

            break;

        case true_1_state:
            if (input == 'r')
                return next(true_2_state);

            break;

        case true_2_state:
            if (input == 'u')
                return next(true_3_state);

            break;

        case true_3_state:
            if (input == 'e')
                return token(true_token), next(default_state);

            break;

        case false_1_state:
            if (input == 'a')
                return next(false_2_state);

            break;

        case false_2_state:
            if (input == 'l')
                return next(false_3_state);

            break;

        case false_3_state:
            if (input == 's')
                return next(false_4_state);

            break;

        case false_4_state:
            if (input == 'e')
                return token(false_token), next(default_state);

            break;
        }

        throw json::exception("unexpected character", m_row, m_column);
    }

    inline bool next(char_state state)
    {
        m_char_state = state;
        return true;
    }

    inline bool repeat(char_state state)
    {
        m_char_state = state;
        return false;
    }

    inline void take(char_type input)
    {
        m_data.push_back(input);
    }

    inline void take_unicode()
    {
        if (m_unicode < 0x80)
        {
            m_data.push_back(char_type(m_unicode));
        }
        else if (m_unicode < 0x800)
        {
            m_data.push_back(char_type(0xC0 | ((m_unicode >> 6) & 0x1F)));
            m_data.push_back(char_type(0x80 | ((m_unicode) & 0x3F)));
        }
        else
        {
            m_data.push_back(char_type(0xE0 | ((m_unicode >> 12) & 0x0F)));
            m_data.push_back(char_type(0x80 | ((m_unicode >> 6) & 0x3F)));
            m_data.push_back(char_type(0x80 | ((m_unicode) & 0x3F)));
        }

        m_unicode = 0;
    }

    inline void unicode(char_type input)
    {
        std::uint16_t digit = 0;

        if (input >= '0' && input <= '9')
            digit = input - '0';
        else if (input >= 'a' && input <= 'f')
            digit = input - 'a' + 10;
        else if (input >= 'A' && input <= 'F')
            digit = input - 'A' + 10;
        else
            throw std::bad_exception();

        m_unicode = (m_unicode << 4) | digit;
    }

    inline void token(token input)
    {
        if (m_token_state.empty())
        {
            if (input == object_begin_token)
                return call(json::item::object_begin), push(object_1_state);

            if (input == array_begin_token)
                return call(json::item::array_begin), push(array_1_state);

            throw json::exception("\"{\" or \"[\" expected", m_row, m_column);
        }

        switch (m_token_state.top())
        {
        case object_1_state:
            if (input == object_end_token)
                return call(json::item::object_end), pop();

        case object_2_state:
            if (input == string_token)
                return call(json::item::key), jump(object_3_state);

            throw json::exception("key or \"}\" expected", m_row, m_column);

        case object_3_state:
            if (input == colon_token)
                return jump(object_v_state);

            throw json::exception("\":\" expected", m_row, m_column);

        case object_v_state:
            jump(object_n_state);

            if (input == object_begin_token)
                return call(json::item::object_begin), push(object_1_state);

            if (input == array_begin_token)
                return call(json::item::array_begin), push(array_1_state);

            if (input == string_token)
                return call(json::item::string_value);

            if (input == integer_token)
                return call(json::item::integer_value);

            if (input == real_token)
                return call(json::item::real_value);

            if (input == null_token)
                return call(json::item::null_value);

            if (input == true_token)
                return call(json::item::true_value);

            if (input == false_token)
                return call(json::item::false_value);

            throw json::exception("value expected", m_row, m_column);

        case object_n_state:
            if (input == object_end_token)
                return call(json::item::object_end), pop();

            if (input == comma_token)
                return jump(object_2_state);

            throw json::exception("\"}\" or \",\" expected", m_row, m_column);

        case array_1_state:
            if (input == array_end_token)
                return call(json::item::array_end), pop();

        case array_v_state:
            jump(array_n_state);

            if (input == object_begin_token)
                return call(json::item::object_begin), push(object_1_state);

            if (input == array_begin_token)
                return call(json::item::array_begin), push(array_1_state);

            if (input == string_token)
                return call(json::item::string_value);

            if (input == integer_token)
                return call(json::item::integer_value);

            if (input == real_token)
                return call(json::item::real_value);

            if (input == null_token)
                return call(json::item::null_value);

            if (input == true_token)
                return call(json::item::true_value);

            if (input == false_token)
                return call(json::item::false_value);

            throw json::exception("value or \"]\" expected", m_row, m_column);

        case array_n_state:
            if (input == array_end_token)
                return call(json::item::array_end), pop();

            if (input == comma_token)
                return jump(array_v_state);

            throw json::exception("\"]\" or \",\" expected", m_row, m_column);
        }

        throw std::bad_exception();
    }

    inline void push(token_state state)
    {
        m_token_state.push(state);
    }

    inline void jump(token_state state)
    {
        m_token_state.top() = state;
    }

    inline void pop()
    {
        m_token_state.pop();
    }

    inline void call(json::item item)
    {
        m_callback(item, m_data, m_row, m_column);
        m_data.clear();
    }

private:
    const callback_type m_callback;
    std::stack<token_state> m_token_state;
    char_state m_char_state = default_state;
    string_type m_data;
    std::size_t m_row = 1;
    std::size_t m_column = 1;
    std::uint16_t m_unicode = 0;
};

using reader = basic_reader<char>;
using wreader = basic_reader<wchar_t>;

template <typename Char,
          typename Traits = std::char_traits<Char>,
          typename Allocator = std::allocator<Char>>
class basic_writer
{
public:
    using char_type = Char;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using string_type = std::basic_string<char_type, traits_type, allocator_type>;

public:
    void indent(std::size_t indent)
    {
        m_indent = indent;
    }

    std::size_t indent() const
    {
        return m_indent;
    }

    template <typename OutputIterator>
    OutputIterator write(json::item item, const string_type &data, OutputIterator output)
    {
        static const string_type object_begin_data = {'{'};
        static const string_type object_end_data = {'}'};
        static const string_type array_begin_data = {'['};
        static const string_type array_end_data = {']'};
        static const string_type comma_data = {','};
        static const string_type colon_data = {':', ' '};
        static const string_type null_data = {'n', 'u', 'l', 'l'};
        static const string_type true_data = {'t', 'r', 'u', 'e'};
        static const string_type false_data = {'f', 'a', 'l', 's', 'e'};

        if (m_state.empty())
        {
            if (item == json::item::object_begin)
                return token(object_begin_data, output), push(object_1_state), output;

            if (item == json::item::array_begin)
                return token(array_begin_data, output), push(array_1_state), output;

            throw json::exception("object or array begin expected");
        }

        switch (m_state.top())
        {
        case object_1_state:
            if (item == json::item::object_end)
                return pop(), token(object_end_data, output), output;

            if (item == json::item::key)
                return endl(output), string(data, output), token(colon_data, output), jump(object_v_state), output;

            throw json::exception("object end or key expected");

        case object_v_state:
            if (item == json::item::object_begin)
                return token(object_begin_data, output), jump(object_n_state), push(object_1_state), output;

            if (item == json::item::array_begin)
                return token(array_begin_data, output), jump(object_n_state), push(array_1_state), output;

            if (item == json::item::string_value)
                return string(data, output), jump(object_n_state), output;

            if (item == json::item::integer_value)
                return token(data, output), jump(object_n_state), output;

            if (item == json::item::real_value)
                return token(data, output), jump(object_n_state), output;

            if (item == json::item::null_value)
                return token(null_data, output), jump(object_n_state), output;

            if (item == json::item::true_value)
                return token(true_data, output), jump(object_n_state), output;

            if (item == json::item::false_value)
                return token(false_data, output), jump(object_n_state), output;

            throw json::exception("value expected");

        case object_n_state:
            if (item == json::item::object_end)
                return pop(), endl(output), token(object_end_data, output), output;

            if (item == json::item::key)
                return token(comma_data, output), endl(output), string(data, output), token(colon_data, output), jump(object_v_state), output;

            throw json::exception("object end of key expected");

        case array_1_state:
            if (item == json::item::array_end)
                return pop(), token(array_end_data, output), output;

            if (item == json::item::object_begin)
                return endl(output), token(object_begin_data, output), jump(array_n_state), push(object_1_state), output;

            if (item == json::item::array_begin)
                return endl(output), token(array_begin_data, output), jump(array_n_state), push(array_1_state), output;

            if (item == json::item::string_value)
                return endl(output), string(data, output), jump(array_n_state), output;

            if (item == json::item::integer_value)
                return endl(output), token(data, output), jump(array_n_state), output;

            if (item == json::item::real_value)
                return endl(output), token(data, output), jump(array_n_state), output;

            if (item == json::item::null_value)
                return endl(output), token(null_data, output), jump(array_n_state), output;

            if (item == json::item::true_value)
                return endl(output), token(true_data, output), jump(array_n_state), output;

            if (item == json::item::false_value)
                return endl(output), token(false_data, output), jump(array_n_state), output;

            throw json::exception("array end of value expected");

        case array_n_state:
            if (item == json::item::array_end)
                return pop(), endl(output), token(array_end_data, output), output;

            if (item == json::item::object_begin)
                return token(comma_data, output), endl(output), token(object_begin_data, output), push(object_1_state), output;

            if (item == json::item::array_begin)
                return token(comma_data, output), endl(output), token(array_begin_data, output), push(array_1_state), output;

            if (item == json::item::string_value)
                return token(comma_data, output), endl(output), string(data, output), output;

            if (item == json::item::integer_value)
                return token(comma_data, output), endl(output), token(data, output), output;

            if (item == json::item::real_value)
                return token(comma_data, output), endl(output), token(data, output), output;

            if (item == json::item::null_value)
                return token(comma_data, output), endl(output), token(null_data, output), output;

            if (item == json::item::true_value)
                return token(comma_data, output), endl(output), token(true_data, output), output;

            if (item == json::item::false_value)
                return token(comma_data, output), endl(output), token(false_data, output), output;

            throw json::exception("array end of value expected");
        }

        throw std::bad_exception();
    }

private:
    enum token_state
    {
        object_1_state,
        object_v_state,
        object_n_state,
        array_1_state,
        array_n_state,
    };

private:
    inline void push(token_state state)
    {
        m_state.push(state);
    }

    inline void jump(token_state state)
    {
        m_state.top() = state;
    }

    inline void pop()
    {
        m_state.pop();
    }

    template <typename OutputIterator>
    inline void endl(OutputIterator &output)
    {
        if (m_indent)
        {
            *output++ = '\n';
            output = std::fill_n(output, m_state.size() * m_indent, ' ');
        }
    }

    template <typename OutputIterator>
    void string(const string_type &data, OutputIterator &output)
    {
        auto encode = [&output](char_type input)
        {
            switch (input)
            {
            case '"':
                *output++ = '\\';
                *output++ = '"';
                break;

            case '\\':
                *output++ = '\\';
                *output++ = '\\';
                break;

            case '/':
                *output++ = '\\';
                *output++ = '/';
                break;

            case '\b':
                *output++ = '\\';
                *output++ = 'b';
                break;

            case '\f':
                *output++ = '\\';
                *output++ = 'f';
                break;

            case '\n':
                *output++ = '\\';
                *output++ = 'n';
                break;

            case '\r':
                *output++ = '\\';
                *output++ = 'r';
                break;

            case '\t':
                *output++ = '\\';
                *output++ = 't';
                break;

            default:
                *output++ = input;
            }
        };

        *output++ = '"';
        std::for_each(data.begin(), data.end(), encode);
        *output++ = '"';
    }

    template <typename OutputIterator>
    inline void token(const string_type &data, OutputIterator &output)
    {
        output = std::copy(data.begin(), data.end(), output);
    }

private:
    std::size_t m_indent = 2;
    std::stack<token_state> m_state;
};

using writer = basic_writer<char>;
using wwriter = basic_writer<wchar_t>;

}
