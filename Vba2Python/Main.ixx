import std;
import Lexer;
import Base;
import Parser;
import VbaSpec;

using std::string;
using std::pair;
using std::vector;

int main()
{
    auto l = Lexer<TokType>::New(lexRules);
    auto filename = "vba-demo.vba";
    std::ifstream file(filename);
    string content;
    if (file.is_open())
    {
        content.reserve(std::filesystem::file_size(filename));
        std::string line;

        while (std::getline(file, line))
        {
            content.append(move(line));
            content.push_back('\n');
        }
        file.close();
        auto toks = l.Lex(content);// | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::Comment; }) | to<vector<Token<TokType>>>();
        //toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
    }
}