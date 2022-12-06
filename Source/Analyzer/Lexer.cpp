#ifndef KPLC_ANALYZER_LEXER_CPP
#define KPLC_ANALYZER_LEXER_CPP

#include "../Exception.cpp"
#include "../Utilities/Text.cpp"
#include "../Utilities/Dynar.cpp"
#include "Token.cpp"

enum class LexerModule : Nat8 {
    Alphabetic,
    Numeric,
    Natural,
    Binary,
    Hexadecimal,
    Real,
    Scientific,
    Symbolic,
};

enum LexerFlag : Bit8 {
    End = 1 << 0,
    Continue = 1 << 1,
};

enum class LexerError : Nat8 {
    WrongFormat = 1,
    Valueless,
    Incomplete,
    Inconvertible,
    OutOfRange,
};

class Lexer {
    TokenPoint point;
    Int64 index;
    const Text8 *source;
    Text8 peek;
    Token token;

public:
    [[nodiscard]]
    explicit constexpr
    Lexer(const Text8 *source)
        : point({1, 1}),
          index(-1),
          source(source),
          peek(source[0]),
          token() {}

    Void Destroy() { delete[] $ source; }

public:
    Token Lex() {
        while (U::Text::IsWhitespace($ peek))
            $ Advance();
        if ($ peek == '\0') {
            $ token.Symbol = TokenSymbol::End;
            goto EPILOGUE;
        }

        $ token.Start = $ point;
        if (U::Text::IsAlphabetic($ peek))
            $ LexAlphabetic();
        else if (U::Text::IsNumeric($ peek))
            $ LexNumeric();
        else $ LexSymbolic();

    EPILOGUE:
        $ token.End = {
            .Line = $ point.Line,
            .Column = $ point.Column - 1
        };
        return $ token;
    }

private:
    [[nodiscard]]
    constexpr
    Bool PeekIsValidIdentity()
    const noexcept { return U::Text::IsAlphabetic($ peek) || $ peek == '_'; }

    Void LexAlphabetic() {
        U::Dynar<Text8> buf;
        do {
            if (buf.Size() > 1024)
                $ Yeet(LexerModule::Alphabetic, LexerError::OutOfRange);
            buf.Put($ peek);
            $ Advance();
        } while (U::Text::IsNumeric($ peek) || $ PeekIsValidIdentity());
        buf.Put('\0');
        $ MatchWord(buf.Flush());
    }

    Void MatchWord(const Text8 *buf) {
        if (U::Text::Compare("procedure", buf) == 0)
            $ token.Symbol = TokenSymbol::Procedure;
        else if (U::Text::Compare("datum", buf) == 0)
            $ token.Symbol = TokenSymbol::Datum;
        else if (U::Text::Compare("give", buf) == 0)
            $ token.Symbol = TokenSymbol::Give;

        // TODO This shit could be more efficient.
        else if (U::Text::Compare("Nat8", buf) == 0)
            $ token.Symbol = TokenSymbol::Nat8;
        else if (U::Text::Compare("Nat16", buf) == 0)
            $ token.Symbol = TokenSymbol::Nat16;
        else if (U::Text::Compare("Nat32", buf) == 0)
            $ token.Symbol = TokenSymbol::Nat32;
        else if (U::Text::Compare("Nat64", buf) == 0)
            $ token.Symbol = TokenSymbol::Nat64;
        else if (U::Text::Compare("Int8", buf) == 0)
            $ token.Symbol = TokenSymbol::Int8;
        else if (U::Text::Compare("Int16", buf) == 0)
            $ token.Symbol = TokenSymbol::Int16;
        else if (U::Text::Compare("Int32", buf) == 0)
            $ token.Symbol = TokenSymbol::Int32;
        else if (U::Text::Compare("Int64", buf) == 0)
            $ token.Symbol = TokenSymbol::Int64;
        else {
            $ token.Symbol = TokenSymbol::Identity;
            $ token.Value.Identity = buf;
        }
    }

private:
    inline
    Void LexNumeric() {
        U::Dynar<Text8> buf;
        if ($ peek == '0') {
            $ Advance();
            switch ($ peek) {
            case '0':
                do $ Advance();
                while ($ peek == '0');
                break;
            case 'b':
            case 'B':
                $ Advance();
                return $ LexBinary(&buf);
            case 'x':
            case 'X':
                $ Advance();
                return $ LexHexadecimal(&buf);
            default:
                break;
            }
        }

        if ($ PeekIsValidNatural())
            return $ LexNatural(&buf);
        else if ($ peek == '.')
            return $ LexReal(&buf);
        $ token.Symbol = TokenSymbol::Natural;
        $ token.Value.Integer = 0;
    }

    inline
    Void PutNumericBuf(U::Dynar<Text8> *buf) {
        if ($ peek != '_')
            buf->Put($ peek);
        $ Advance();
    }

private:
    [[nodiscard]]
    constexpr
    Bool PeekIsValidBinary()
    const noexcept { return $ peek == '0' || $ peek == '1' || $ peek == '_'; }

    inline
    Void BinaryYeet(LexerError error) { $ Yeet(LexerModule::Binary, error); }

    inline
    Void LexBinary(U::Dynar<Text8> *buf) {
        $ token.Symbol = TokenSymbol::Machine;
        if ($ PeekIsValidBinary()) {
            do $ PutNumericBuf(buf);
            while ($ PeekIsValidBinary());
        } else $ BinaryYeet(LexerError::WrongFormat);

        if (buf->Size() == 0)
            $ BinaryYeet(LexerError::Valueless);
        buf->Put(0);
        try {
            $ token.Value.Machine = U::Text::ConvertToNatural(buf->Flush(), 2);
        } catch (const Exception::InvalidArgument &) {
            $ BinaryYeet(LexerError::Inconvertible);
        } catch (const Exception::OutOfRange &) {
            $ BinaryYeet(LexerError::OutOfRange);
        }
    }

private:
    [[nodiscard]]
    constexpr
    Bool PeekIsValidHexadecimal()
    const noexcept {
        return ($ peek >= 'a' && $ peek <= 'f') ||
               ($ peek >= 'A' && $ peek <= 'F') || $ PeekIsValidNatural();
    }

    inline
    Void HexadecimalYeet(LexerError error) { $ Yeet(LexerModule::Hexadecimal, error); }

    inline
    Void LexHexadecimal(U::Dynar<Text8> *buf) {
        $ token.Symbol = TokenSymbol::Machine;
        if ($ PeekIsValidHexadecimal()) {
            do $ PutNumericBuf(buf);
            while ($ PeekIsValidHexadecimal());
        } else $ HexadecimalYeet(LexerError::WrongFormat);

        if (buf->Size() == 0)
            HexadecimalYeet(LexerError::Valueless);
        buf->Put(0);
        try {
            $ token.Value.Machine = U::Text::ConvertToNatural(buf->Flush(), 16);
        }
        catch (const Exception::InvalidArgument &) {
            $ HexadecimalYeet(LexerError::Inconvertible);
        }
        catch (const Exception::OutOfRange &) {
            $ HexadecimalYeet(LexerError::OutOfRange);
        }
    }

private:
    [[nodiscard]]
    constexpr
    Bool PeekIsValidNatural()
    const noexcept { return U::Text::IsNumeric($ peek) || $ peek == '_'; }

    inline
    Void NaturalYeet(LexerError error) { $ Yeet(LexerModule::Natural, error); }

    Void LexNatural(U::Dynar<Text8> *buf) {
        do {
            $ PutNumericBuf(buf);
            if ($ peek == '.')
                return $ LexReal(buf);
        } while ($ PeekIsValidNatural());
        $ token.Symbol = TokenSymbol::Natural;

        if (buf->Size() == 0)
            $ NaturalYeet(LexerError::Valueless);
        buf->Put(0);
        try {
            $ token.Value.Integer = U::Text::ConvertToInteger(buf->Flush());
        } catch (const Exception::InvalidArgument &) {
            $ NaturalYeet(LexerError::Inconvertible);
        } catch (const Exception::OutOfRange &) {
            $ NaturalYeet(LexerError::OutOfRange);
        }
    }

private:
    inline
    Void RealYeet(LexerError error) { $ Yeet(LexerModule::Real, error); }

    Void LexReal(U::Dynar<Text8> *buf) {
        $ token.Symbol = TokenSymbol::Real;
        do {
            $ PutNumericBuf(buf);
            if ($ peek == '.')
                $ RealYeet(LexerError::WrongFormat);
        } while ($ PeekIsValidNatural());

        buf->Put(0);
        try {
            $ token.Value.Real = U::Text::ConvertToReal(buf->Flush());
        } catch (const Exception::InvalidArgument &) {
            $ RealYeet(LexerError::Inconvertible);
        } catch (const Exception::OutOfRange &) {
            $ RealYeet(LexerError::OutOfRange);
        }
    }

private:
    Void LexSymbolic() {
        switch ($ peek) {
        case '<':
            switch ($ Peek(2)) {
            case '=': $ token.Symbol = TokenSymbol::LesserEquivalent;
                goto DOUBLE;
            case '<': $ token.Symbol = TokenSymbol::LeftShift;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Lesser;
                goto SINGLE;
            }
        case '>':
            switch ($ Peek(2)) {
            case '=': $ token.Symbol = TokenSymbol::GreaterEquivalent;
                goto DOUBLE;
            case '<': $ token.Symbol = TokenSymbol::RightShift;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Greater;
                goto SINGLE;
            }
        case ':':
            switch ($ Peek(2)) {
            case ':': $ token.Symbol = TokenSymbol::DoubleColon;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Colon;
                goto SINGLE;
            }
        case '+':
            switch ($ Peek(2)) {
            case '+': $ token.Symbol = TokenSymbol::Increment;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Plus;
                goto SINGLE;
            }
        case '-':
            switch ($ Peek(2)) {
            case '-': $ token.Symbol = TokenSymbol::Increment;
                goto DOUBLE;
            case '>': $ token.Symbol = TokenSymbol::RightArrow;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Minus;
                goto SINGLE;
            }
        case '&':
            switch ($ Peek(2)) {
            case '&': $ token.Symbol = TokenSymbol::DoubleAnd;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::And;
                goto SINGLE;
            }
        case '|':
            switch ($ Peek(2)) {
            case '+': $ token.Symbol = TokenSymbol::DoubleLine;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Line;
                goto SINGLE;
            }
        case '\\':
            switch ($ Peek(2)) {
            case '\\': $ token.Symbol = TokenSymbol::Comment;
                do $ Advance();
                while ($ peek != '\n' && $ peek != '\0');
                return;
            case '*': $ token.Symbol = TokenSymbol::Comment;
                do $ Advance();
                while ($ peek != '*' && $ Peek(2) != '\\' && $ peek != '\0');
                return;
            default: $ token.Symbol = TokenSymbol::Slosh;
                goto SINGLE;
            }
        case '=':
            switch ($ Peek(2)) {
            case '=': $ token.Symbol = TokenSymbol::Equivalent;
                goto DOUBLE;
            default: $ token.Symbol = TokenSymbol::Equal;
                goto SINGLE;
            }
        case '{':
        case '}':
        case '(':
        case ')':
        case '[':
        case ']':
        case '\"':
        case ',':
        case ';':
        case '!':
        case '?':
        case '@': $ token.Symbol = (TokenSymbol) $ peek;
            goto SINGLE;
        default:
            $ token.Symbol = TokenSymbol::None;
            $ token.Value.None = $ peek;
            goto SINGLE;
        }

    DOUBLE:
        $ Advance();

    SINGLE:
        $ Advance();
    }

private:
    Void Yeet(LexerModule way, LexerError error) {
        const Text8 *description = "";
        throw Exception(
            CompilerModule::Lexer, (Nat64) error + (Nat64) way,
            description
        );
    }

private:
    [[nodiscard]]
    constexpr
    Text8 Peek(Nat64 offset = 1)
    const noexcept { return $ source[$ index + offset]; }

    constexpr
    Void Advance()
    noexcept {
        $ index++;
        $ peek = $ Peek();
        if ($ peek == '\n') {
            $ point.Line++;
            $ point.Column = 0;
        }
        $ point.Column++;
    }
};

#endif // KPLC_ANALYZER_LEXER_CPP
