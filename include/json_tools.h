/*
 * Copyright © 2012 Jørgen Lind

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.

 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
*/

#ifndef JSON_TOOLS_H
#define JSON_TOOLS_H

#include <stddef.h>
#include <functional>
#include <vector>
#include <string>

#include <assert.h>

namespace JT {

struct DataRef
{
    DataRef()
        : data("")
        , size(0)
    {}

    DataRef(const char *data, size_t size)
        : data(data)
        , size(size)
    {}

    template <size_t N>
    static DataRef asDataRef(const char (&data)[N])
    {
        return DataRef(data, N - 1);
    }

    static DataRef asDataRef(const std::string &str)
    {
        return DataRef(str.c_str(), str.size());
    }

    const char *data;
    size_t size;
};

struct Token
{
    enum Type {
        Error,
        String,
        Ascii,
        Number,
        ObjectStart,
        ObjectEnd,
        ArrayStart,
        ArrayEnd,
        Bool,
        Null
    };

    Token();

    Type name_type;
    DataRef name;
    Type value_type;
    DataRef value;
};

struct IntermediateToken
{
    IntermediateToken()
    { }

    void clear() {
        if (!active)
            return;
        active = false;
        name_type_set = false;
        data_type_set = false;
        name_type = Token::Error;
        data_type = Token::Error;
        name.clear();
        data.clear();
    }

    bool active = false;
    bool name_type_set = false;
    bool data_type_set = false;
    Token::Type name_type = Token::Error;
    Token::Type data_type = Token::Error;
    std::string name;
    std::string data;
};

class TypeChecker
{
public:
    TypeChecker()
        : m_null("null")
        , m_true("true")
        , m_false("false")
    {}

    Token::Type type(Token::Type type, const char *data, size_t length) const {
        if (type != Token::Ascii)
            return type;
        if (m_null.size() == length) {
            if (strncmp(m_null.c_str(),data,length) == 0) {
                return Token::Null;
            } else if (strncmp(m_true.c_str(),data,length) == 0) {
                return Token::Bool;
            }
        }
        if (m_false.size() == length) {
            if (strncmp(m_false.c_str(),data,length) == 0)
                return Token::Bool;
        }
        return Token::Ascii;
    }
private:
    const std::string m_null;
    const std::string m_true;
    const std::string m_false;
};

enum class Error {
        NoError,
        NeedMoreData,
        InvalidToken,
        ExpectedPropertyName,
        ExpectedDelimiter,
        ExpectedDataToken,
        ExpectedObjectStart,
        ExpectedObjectEnd,
        ExpectedArrayStart,
        ExpectedArrayEnd,
        IlligalPropertyName,
        IlligalPropertyType,
        IlligalDataValue,
        EncounteredIlligalChar,
        CouldNotCreateNode,
        NodeNotFound,
        MissingPropertyName,
        UnknownPropertyName,
        UnknownError
};

class Tokenizer
{
public:
    Tokenizer();
    ~Tokenizer();

    void allowAsciiType(bool allow);
    void allowNewLineAsTokenDelimiter(bool allow);
    void allowSuperfluousComma(bool allow);

    void addData(const char *data, size_t size);
    size_t registered_buffers() const;
    void registerNeedMoreDataCallback(std::function<void(Tokenizer &)> callback, bool oneShotCallback);
    void registerRelaseCallback(std::function<void(const char *)> callback);

    Error nextToken(Token &next_token);
    void registerTokenTransformer(std::function<void(Token &next_token)> token_transformer);

private:
    enum InTokenState {
        FindingName,
        FindingDelimiter,
        FindingData,
        FindingTokenEnd
    };

    enum InPropertyState {
        NoStartFound,
        FindingEnd,
        FoundEnd
    };

    void resetForNewToken();
    void resetForNewValue();
    Error findStringEnd(const DataRef &json_data, size_t *chars_ahead);
    Error findAsciiEnd(const DataRef &json_data, size_t *chars_ahead);
    Error findNumberEnd(const DataRef &json_data, size_t *chars_ahead);
    Error findStartOfNextValue(Token::Type *type,
                               const DataRef &json_data,
                               size_t *chars_ahead);
    Error findDelimiter(const DataRef &json_data, size_t *chars_ahead);
    Error findTokenEnd(const DataRef &json_data, size_t *chars_ahead);
    void requestMoreData();
    void releaseFirstDataRef();
    Error populateFromDataRef(DataRef &data, Token::Type *type, const DataRef &json_data);
    static void populate_annonymous_token(const DataRef &data, Token::Type type, Token &token);
    Error populateNextTokenFromDataRef(Token &next_token, const DataRef &json_data);

    std::vector<DataRef> data_list;
    std::vector<std::function<void(const char *)>> release_callbacks;
    std::vector<std::pair<std::function<void(Tokenizer &)>, bool>> need_more_data_callabcks;
    size_t cursor_index = 0;
    size_t current_data_start = 0;
    InTokenState token_state = FindingName;
    InPropertyState property_state = NoStartFound;
    Token::Type property_type = Token::Error;
    bool is_escaped = false;
    bool allow_ascii_properties = false;
    bool allow_new_lines = false;
    bool allow_superfluous_comma = false;
    bool expecting_prop_or_annonymous_data = false;
    bool continue_after_need_more_data = false;
    IntermediateToken intermediate_token;
    std::function<void(Token &next_token)> token_transformer;
    TypeChecker type_checker;
};

class SerializerOptions
{
public:
    SerializerOptions(bool pretty = false, bool ascii = false);

    int shiftSize() const;

    bool pretty() const;
    void setPretty(bool pretty);

    int depth() const;
    void setDepth(int depth);

    bool ascii() const;
    void setAscii(bool ascii);

    void skipDelimiter(bool skip);

    const std::string &prefix() const;
    const std::string &tokenDelimiter() const;
    const std::string &valueDelimiter() const;
    const std::string &postfix() const;

private:
    int m_shift_size;
    int m_depth;
    bool m_pretty;
    bool m_ascii;

    std::string m_prefix;
    std::string m_token_delimiter;
    std::string m_value_delimiter;
    std::string m_postfix;
};

class SerializerBuffer
{
public:
    bool free() const { return size - used; }
    bool append(const char *data, size_t size);
    char *buffer;
    size_t size;
    size_t used;
};

class Serializer
{
public:
    Serializer();
    Serializer(char *buffer, size_t size);

    void appendBuffer(char *buffer, size_t size);
    void setOptions(const SerializerOptions &option);
    SerializerOptions options() const { return m_option; }

    bool write(const Token &token);
    void registerTokenTransformer(std::function<const Token&(const Token &)> token_transformer);

    void addRequestBufferCallback(std::function<void(Serializer *)> callback);
    const std::vector<SerializerBuffer> &buffers() const;
    void clearBuffers();
private:
    void askForMoreBuffers();
    void markCurrentSerializerBufferFull();
    bool writeAsString(const DataRef &data);
    bool write(Token::Type type, const DataRef &data);
    bool write(const char *data, size_t size);
    bool write(const std::string &str) { return write(str.c_str(), str.size()); }

    std::vector <std::function<void(Serializer *)>> m_request_buffer_callbacks;
    std::vector <SerializerBuffer *> m_unused_buffers;
    std::vector <SerializerBuffer> m_all_buffers;

    SerializerOptions m_option;
    bool m_first = true;
    bool m_token_start = true;
    std::function<const Token&(const Token &)> m_token_transformer;
};

// IMPLEMENTATION

inline Token::Token()
    : name_type(String)
    , name()
    , value_type(String)
    , value()
{

}

inline Tokenizer::Tokenizer()
{}
inline Tokenizer::~Tokenizer()
{}

inline void Tokenizer::allowAsciiType(bool allow)
{
    allow_ascii_properties = allow;
}

inline void Tokenizer::allowNewLineAsTokenDelimiter(bool allow)
{
    allow_new_lines = allow;
}

inline void Tokenizer::allowSuperfluousComma(bool allow)
{
    allow_superfluous_comma = allow;
}
inline void Tokenizer::addData(const char *data, size_t data_size)
{
    data_list.push_back(DataRef(data, data_size));
}

inline size_t Tokenizer::registered_buffers() const
{
    return data_list.size();
}

inline void Tokenizer::registerNeedMoreDataCallback(std::function<void(Tokenizer &)> callback, bool oneShotCallback)
{
    need_more_data_callabcks.push_back(std::make_pair(callback, oneShotCallback));
}
inline void Tokenizer::registerRelaseCallback(std::function<void(const char *)> callback)
{
    release_callbacks.push_back(callback);
}

inline Error Tokenizer::nextToken(Token &next_token)
{
    if (data_list.empty()) {
        requestMoreData();
    }

    if (data_list.empty()) {
        return Error::NeedMoreData;
    }

    if (!continue_after_need_more_data)
        resetForNewToken();

    Error error = Error::NeedMoreData;
    while (error == Error::NeedMoreData && data_list.size()) {
        const DataRef &json_data = data_list.front();
        error = populateNextTokenFromDataRef(next_token, json_data);

        if (error != Error::NoError) {
            releaseFirstDataRef();
        }
        if (error == Error::NeedMoreData) {
            requestMoreData();
        }
    }

    continue_after_need_more_data = error == Error::NeedMoreData;

    if (error == Error::NoError && token_transformer != nullptr) {
        token_transformer(next_token);
    }

    return error;
}

inline void Tokenizer::registerTokenTransformer(std::function<void(Token &next_token)> token_transformer)
{
    token_transformer = token_transformer;
}

inline void Tokenizer::resetForNewToken()
{
    intermediate_token.clear();
    resetForNewValue();
}

inline void Tokenizer::resetForNewValue()
{
    property_state = NoStartFound;
    property_type = Token::Error;
    current_data_start = 0;
}

inline Error Tokenizer::findStringEnd(const DataRef &json_data, size_t *chars_ahead)
{
    for (size_t end = cursor_index; end < json_data.size; end++) {
        if (is_escaped) {
            is_escaped = false;
            continue;
        }
        switch (*(json_data.data + end)) {
        case '\\':
            is_escaped = true;
            break;
        case '"':
            *chars_ahead = end + 1 - cursor_index;
            return Error::NoError;

        default:
            break;
        }
    }
    return Error::NeedMoreData;
}

inline Error Tokenizer::findAsciiEnd(const DataRef &json_data, size_t *chars_ahead)
{
    assert(property_type == Token::Ascii);
    for (size_t end = cursor_index; end < json_data.size; end++) {
        char ascii_code = *(json_data.data + end);
        if ((ascii_code >= 'A' && ascii_code <= 'Z') ||
            (ascii_code >= '^' && ascii_code <= 'z') ||
            (ascii_code >= '0' && ascii_code <= '9')) {
            continue;
        } else if (ascii_code == '\0') {
            *chars_ahead = end - cursor_index;
            return Error::NeedMoreData;
        } else {
            *chars_ahead = end - cursor_index;
            return Error::NoError;
        }
    }
    return Error::NeedMoreData;
}

inline Error Tokenizer::findNumberEnd(const DataRef &json_data, size_t *chars_ahead)
{
    for (size_t end = cursor_index; end < json_data.size; end++) {
        char number_code = *(json_data.data + end);
        if ((number_code >= '0' && number_code <= '9'))
            continue;
        switch(number_code) {
        case '.':
        case '+':
        case '-':
        case 'e':
        case 'E':
            continue;
        default:
            *chars_ahead = end - cursor_index;
            return Error::NoError;
        }
    }
    return Error::NeedMoreData;
}

inline Error Tokenizer::findStartOfNextValue(Token::Type *type,
                                      const DataRef &json_data,
                                      size_t *chars_ahead)
{

    assert(property_state == NoStartFound);

    for (size_t current_pos  = cursor_index; current_pos < json_data.size; current_pos++) {
        switch (*(json_data.data + current_pos)) {
        case ' ':
        case '\n':
        case '\0':
            break;
        case '"':
            *type = Token::String;
            *chars_ahead = current_pos - cursor_index + 1;
            return Error::NoError;
        case '{':
            *type = Token::ObjectStart;
            *chars_ahead = current_pos - cursor_index;
            return Error::NoError;
        case '}':
            *type = Token::ObjectEnd;
            *chars_ahead = current_pos - cursor_index;
            return Error::NoError;
        case '[':
            *type = Token::ArrayStart;
            *chars_ahead = current_pos - cursor_index;
            return Error::NoError;
        case ']':
            *type = Token::ArrayEnd;
            *chars_ahead = current_pos - cursor_index;
            return Error::NoError;
        case '-':
        case '+':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            *type = Token::Number;
            *chars_ahead = current_pos - cursor_index;
            return Error::NoError;
        default:
            char ascii_code = *(json_data.data + current_pos);
            if ((ascii_code >= 'A' && ascii_code <= 'Z') ||
                (ascii_code >= '^' && ascii_code <= 'z')) {
                *type = Token::Ascii;
                *chars_ahead = current_pos - cursor_index;;
                return Error::NoError;
            } else {
                *chars_ahead = current_pos - cursor_index;
                return Error::EncounteredIlligalChar;
            }
            break;
        }

    }
    return Error::NeedMoreData;
}

inline Error Tokenizer::findDelimiter(const DataRef &json_data, size_t *chars_ahead)
{
    for (size_t end = cursor_index; end < json_data.size; end++) {
        switch(*(json_data.data + end)) {
        case ':':
            token_state = FindingData;
            *chars_ahead = end + 1 - cursor_index;
            return Error::NoError;
        case ',':
            token_state = FindingName;
            *chars_ahead = end + 1 - cursor_index;
            return Error::NoError;
        case ']':
            token_state = FindingName;
            *chars_ahead = end - cursor_index;
            return Error::NoError;
        case ' ':
        case '\n':
            break;
        default:
            return Error::ExpectedDelimiter;
            break;
        }
    }
    return Error::NeedMoreData;
}

inline Error Tokenizer::findTokenEnd(const DataRef &json_data, size_t *chars_ahead)
{
    for (size_t end = cursor_index; end < json_data.size; end++) {
        switch(*(json_data.data + end)) {
        case ',':
            expecting_prop_or_annonymous_data = true;
            *chars_ahead = end + 1 - cursor_index;
            return Error::NoError;
        case '\n':
            if (allow_new_lines) {
                *chars_ahead = end + 1 - cursor_index;
                return Error::NoError;
            }
            break;
        case ']':
        case '}':
            *chars_ahead = end - cursor_index;
            return Error::NoError;
        case ' ':
        case '\0':
            break;
        default:
            *chars_ahead = end + 1 - cursor_index;
            return Error::InvalidToken;
        }
    }
    return Error::NeedMoreData;
}

inline void Tokenizer::requestMoreData()
{
    for (auto pairs: need_more_data_callabcks)
    {
        pairs.first(*this);
    }
    need_more_data_callabcks.erase(std::remove_if(need_more_data_callabcks.begin(),
                              need_more_data_callabcks.end(),
                              [](decltype(*need_more_data_callabcks.begin()) &pair)
                                { return pair.second; }), need_more_data_callabcks.end());
}

inline void Tokenizer::releaseFirstDataRef()
{
    const char *data_to_release = 0;
    if (!data_list.empty()) {
        const DataRef &json_data = data_list.front();
        data_to_release = json_data.data;
        data_list.erase(data_list.begin());
    }
    for (auto function : release_callbacks) {
        function(data_to_release);
    }
    cursor_index = 0;
    current_data_start = 0;
}

inline Error Tokenizer::populateFromDataRef(DataRef &data, Token::Type *type, const DataRef &json_data)
{
    size_t diff = 0;
    Error error = Error::NoError;
    data.size = 0;
    data.data = json_data.data + cursor_index;
    if (property_state == NoStartFound) {
        Error error = findStartOfNextValue(type, json_data, &diff);
        if (error != Error::NoError) {
            *type = Token::Error;
            return error;
        }

        data.data = json_data.data + cursor_index + diff;
        current_data_start = cursor_index + diff;
        cursor_index += diff + 1;
        property_type = *type;


        if (*type == Token::ObjectStart || *type == Token::ObjectEnd
            || *type == Token::ArrayStart || *type == Token::ArrayEnd) {
            data.size = 1;
            property_state = FoundEnd;
        } else {
            property_state = FindingEnd;
        }
    }

    int size_adjustment = 0;
    if (property_state == FindingEnd) {
        switch (*type) {
        case Token::String:
            error = findStringEnd(json_data, &diff);
            size_adjustment = -1;
            break;
        case Token::Ascii:
            error = findAsciiEnd(json_data, &diff);
            break;
        case Token::Number:
            error = findNumberEnd(json_data, &diff);
            break;
        default:
            return Error::InvalidToken;
        }

        if (error != Error::NoError) {
            return error;
        }

        cursor_index += diff;
        data.size = cursor_index - current_data_start + size_adjustment;
        property_state = FoundEnd;
    }

    return Error::NoError;
}

inline void Tokenizer::populate_annonymous_token(const DataRef &data, Token::Type type, Token &token)
{
    token.name = DataRef();
    token.name_type = Token::Ascii;
    token.value = data;
    token.value_type = type;
}

inline Error Tokenizer::populateNextTokenFromDataRef(Token &next_token, const DataRef &json_data)
{
    Token tmp_token;
    while (cursor_index < json_data.size) {
        size_t diff = 0;
        DataRef data;
        Token::Type type;
        Error error;
        switch (token_state) {
        case FindingName:
            type = intermediate_token.name_type;
            error = populateFromDataRef(data, &type, json_data);
            if (error == Error::NeedMoreData) {
                if (property_state > NoStartFound) {
                    intermediate_token.active = true;
                    size_t to_null = strnlen(data.data , json_data.size - current_data_start);
                    intermediate_token.name.append(data.data , to_null);
                    if (!intermediate_token.name_type_set) {
                        intermediate_token.name_type = type;
                        intermediate_token.name_type_set = true;
                    }
                }
                return error;
            } else if (error != Error::NoError) {
                return error;
            }

            if (intermediate_token.active) {
                intermediate_token.name.append(data.data, data.size);
                data = DataRef::asDataRef(intermediate_token.name);
                type = intermediate_token.name_type;
            }

            if (type == Token::ObjectEnd || type == Token::ArrayEnd
                || type == Token::ArrayStart || type == Token::ObjectStart) {
                switch (type) {
                case Token::ObjectEnd:
                case Token::ArrayEnd:
                    if (expecting_prop_or_annonymous_data && !allow_superfluous_comma) {
                        return Error::ExpectedDataToken;
                    }
                    populate_annonymous_token(data,type,next_token);
                    token_state = FindingTokenEnd;
                    return Error::NoError;

                case Token::ObjectStart:
                case Token::ArrayStart:
                    populate_annonymous_token(data,type,next_token);
                    expecting_prop_or_annonymous_data = false;
                    token_state = FindingName;
                    return Error::NoError;
                default:
                    return Error::UnknownError;
                }
            } else {
                tmp_token.name = data;
            }

            tmp_token.name_type = type_checker.type(type, tmp_token.name.data,
                                                    tmp_token.name.size);
            token_state = FindingDelimiter;
            resetForNewValue();
            break;

        case FindingDelimiter:
            error = findDelimiter(json_data, &diff);
            if (error != Error::NoError) {
                if (intermediate_token.active == false) {
                    intermediate_token.name.append(tmp_token.name.data, tmp_token.name.size);
                    intermediate_token.name_type = tmp_token.name_type;
                    intermediate_token.active = true;
                }
                return Error::NeedMoreData;
            }
            cursor_index += diff;
            resetForNewValue();
            expecting_prop_or_annonymous_data = false;
            if (token_state == FindingName) {
                populate_annonymous_token(tmp_token.name, tmp_token.name_type, next_token);
                return Error::NoError;
            } else {
                if (tmp_token.name_type != Token::String) {
                    if (!allow_ascii_properties || tmp_token.name_type != Token::Ascii) {
                        return Error::IlligalPropertyName;
                    }
                }
            }
            break;

        case FindingData:
            type = intermediate_token.data_type;
            error = populateFromDataRef(data, &type, json_data);
            if (error == Error::NeedMoreData) {
                if (intermediate_token.active == false) {
                    intermediate_token.name.append(tmp_token.name.data, tmp_token.name.size);
                    intermediate_token.name_type = tmp_token.name_type;
                    intermediate_token.active = true;
                }
                if (property_state > NoStartFound) {
                    size_t data_length = strnlen(data.data , json_data.size - current_data_start);
                    intermediate_token.data.append(data.data, data_length);
                    if (!intermediate_token.data_type_set) {
                        intermediate_token.data_type = type;
                        intermediate_token.data_type_set = true;
                    }
                }
                return error;
            } else if (error != Error::NoError) {
                return error;
            }

            if (intermediate_token.active) {
                intermediate_token.data.append(data.data, data.size);
                if (!intermediate_token.data_type_set) {
                    intermediate_token.data_type = type;
                    intermediate_token.data_type_set = true;
                }
                tmp_token.name = DataRef::asDataRef(intermediate_token.name);
                tmp_token.name_type = intermediate_token.name_type;
                data = DataRef::asDataRef(intermediate_token.data);
                type = intermediate_token.data_type;
            }

            tmp_token.value = data;
            tmp_token.value_type = type_checker.type(type, tmp_token.value.data, tmp_token.value.size);

            if (tmp_token.value_type  == Token::Ascii && !allow_ascii_properties)
                return Error::IlligalDataValue;

            if (type == Token::ObjectStart || type == Token::ArrayStart) {
                token_state = FindingName;
            } else {
                token_state = FindingTokenEnd;
            }
            next_token = tmp_token;
            return Error::NoError;
        case FindingTokenEnd:
            error = findTokenEnd(json_data, &diff);
            if (error != Error::NoError) {
                return error;
            }
            cursor_index += diff;
            token_state = FindingName;
            break;
        }
    }
    return Error::NeedMoreData;
}

#define JT_FIELD(name) JT::make_memberinfo(#name, &T::name)
#define JT_SUPER_CLASSES(...) JT::member_fields_tuple<__VA_ARGS__>::create()

#define JT_STRUCT(type, ...) \
    template<typename T> \
    struct json_tools_base \
    { \
       static decltype(std::make_tuple(__VA_ARGS__)) _fields() \
       { auto ret = std::make_tuple(__VA_ARGS__); return ret; } \
    };

#define JT_STRUCT_WITH_SUPER(type, super_list, ...) \
    template<typename T> \
    struct json_tools_base \
    { \
        static decltype(std::tuple_cat(std::make_tuple(__VA_ARGS__), super_list)) _fields() \
        { auto ret = std::tuple_cat(std::make_tuple(__VA_ARGS__), super_list); return ret; } \
    };

template <typename T>
struct has_json_tools_base {
    typedef char yes[1];
    typedef char no[2];

    template <typename C>
    static yes& test(typename C::template json_tools_base<C>*);

    template <typename>
    static no& test(...);

    static constexpr const bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
};

template<typename T, typename U>
struct memberinfo
{
    const char *name;
    size_t name_size;
    T U::* member;
};

template<typename T, typename U, size_t N_SIZE>
constexpr memberinfo<T, U> make_memberinfo(const char (&name)[N_SIZE], T U::* member)
{
    return {name, N_SIZE - 1, member};
}

template<typename... Args>
struct member_fields_tuple;

template<typename T, typename... Args>
struct member_fields_tuple<T, Args...>
{
    static auto create() -> decltype(std::tuple_cat(T::template json_tools_base<T>::_fields(), member_fields_tuple<Args...>::create()))
    {
        static_assert(has_json_tools_base<T>::value, "Type is not a json struct type");
        return std::tuple_cat(T::template json_tools_base<T>::_fields(), member_fields_tuple<Args...>::create());
    }
};

template<>
struct member_fields_tuple<>
{
    static auto create() -> decltype(std::make_tuple())
    {
        return std::make_tuple();
    }
};

template<typename T, typename Field>
Error unpackField(T &to_type, Field field, Tokenizer &tokenizer, Token &token)
{
    if (memcmp(field.name, token.name.data, field.name_size) == 0)
    {
        std::string name(token.name.data, token.name.size);
        fprintf(stderr, "found name %s\n", name.c_str());
        unpackToken(to_type.*field.member, tokenizer, token);
        return Error::NoError;
    }
    return Error::UnknownPropertyName;
}

template<typename T, typename Fields, size_t INDEX>
struct FieldChecker
{
    static Error unpackFields(T &to_type, Fields fields, Tokenizer &tokenizer, Token &token)
    {
        Error error = unpackField(to_type, std::get<INDEX>(fields), tokenizer, token);
        if (error != Error::UnknownPropertyName)
            return error;

        return FieldChecker<T, Fields, INDEX - 1>::unpackFields(to_type, fields, tokenizer, token);
    }
};

template<typename T, typename Fields>
struct FieldChecker<T, Fields, 0>
{
    static Error unpackFields(T &to_type, Fields fields, Tokenizer &tokenizer, Token &token)
    {
        return unpackField(to_type, std::get<0>(fields), tokenizer, token);
    }
};

template<typename T>
Error unpackToken(T &to_type, Tokenizer &tokenizer, Token &token)
{
    static_assert(has_json_tools_base<T>::value, "Given type has no unpackToken specialisation or a specified the JT_STRUCT with its fields\n");

    if (token.value_type != JT::Token::ObjectStart)
        return Error::ExpectedObjectStart;
    JT::Error error = tokenizer.nextToken(token);
    if (error != JT::Error::NoError)
        return error;
    auto fields = T::template json_tools_base<T>::_fields();
    while(token.value_type != JT::Token::ObjectEnd)
    {
        error = FieldChecker<T, decltype(fields), std::tuple_size<decltype(fields)>::value - 1>::unpackFields(to_type, fields, tokenizer, token);
        if (error != Error::NoError)
            return error;
        error = tokenizer.nextToken(token);
        if (error != Error::NoError)
            return error;
    }
    return Error::NoError;
}

template<>
Error unpackToken(std::string &to_type, Tokenizer &tokenizer, Token &token)
{
    to_type = std::string(token.value.data, token.value.size);
    return Error::NoError;
}

template<>
Error unpackToken(double &to_type, Tokenizer &tokenizer, Token &token)
{
    to_type = 4;
    return Error::NoError;
}

template<>
Error unpackToken(bool &to_type, Tokenizer &tokenizer, Token &token)
{
    to_type = true;
    return Error::NoError;
}

template<typename T>
T parseData(T &to_type, Tokenizer &tokenizer, Error &error)
{
    Token token;
    error = tokenizer.nextToken(token);
    if (error != JT::Error::NoError)
        return to_type;
    error = unpackToken<T>(to_type, tokenizer, token);
    return to_type;
}

template<typename T>
T parseData(const char *json_data, size_t size, Error &error)
{
    T ret;
    Tokenizer tokenizer;
    //just doing this to ensure code works
    tokenizer.registerNeedMoreDataCallback([json_data, size](Tokenizer &tokenizer) {
                                               tokenizer.addData(json_data, size);
                                           }, true);
    return parseData(ret, tokenizer, error);
}

} //Namespace
#endif //JSON_TOOLS_H