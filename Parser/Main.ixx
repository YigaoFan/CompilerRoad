import std;
import TableDrivenParser;

int main()
{
    auto l = "Lexer";
    auto p = TableDrivenParser::ConstructFrom("program", { }, { });
    auto s = "Stream";
    //s | l | p;
}