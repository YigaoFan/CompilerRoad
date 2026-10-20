export module ParserCombinator;

import std;
import Parser;
import C11Spec;

using std::invoke_result_t;
using std::unexpected;
using std::vector;
using std::decay_t;
using std::move;
using std::format;

export namespace ParserCombinator
{
	constexpr auto Nullize = [](auto... x) -> std::nullptr_t
	{
		return nullptr;
	};

	constexpr auto SelectLeft = [](auto&& left, auto&& right)
	{
		return move(left);
	};

	constexpr auto SelectRight = [](auto&& left, auto&& right)
	{
		return move(right);
	};

	constexpr auto Id = [](auto&& obj)
	{
		return move(obj);
	};

	template <typename Parser>
	struct From
	{
		Parser parser;

		constexpr From(Parser parser) : parser(move(parser))
		{
		}

		template <typename Stream>
		constexpr auto operator()(Stream& stream) const
		{
			return parser(stream);
		}

		constexpr auto RightWith(auto parser1, auto resultProcess) const
		{
			auto outerParser = Concat(resultProcess, parser, parser1);
			return From<decltype(outerParser)>(outerParser);
		}
	};

	/// <summary>
	/// MoveNext first, then check the TokType
	/// </summary>
	auto ParseWord(TokType tokType, auto resultProcess)
	{
		return [=](auto& stream) -> ParserResult<decltype(resultProcess(tokType))>
		{
			if (not stream.MoveNext())
			{
				return unexpected(ParseFailResult{ .Message = format("input stream is empty when parsing {}", tokType) });
			}

			if (stream.Current().Type == tokType)
			{
				return resultProcess(tokType);
			}
			return unexpected(ParseFailResult{ .Message = format("expect {}, but got {}", tokType, stream.Current().Type) });
		};
	}

	auto Concat(auto resultProcess, auto parser0, auto parser1)
	{
		return [=]<typename Stream>(Stream& stream) -> ParserResult<invoke_result_t<decltype(resultProcess), typename invoke_result_t<decltype(parser0), Stream&>::value_type, typename invoke_result_t<decltype(parser1), Stream&>::value_type>>
		{
			auto r0 = parser0(stream);
			if (not r0.has_value())
			{
				return unexpected(move(r0.error()));
			}
			auto r1 = parser1(stream);
			if (not r1.has_value())
			{
				return unexpected(move(r1.error()));
			}
			return resultProcess(move(r0.value()), move(r1.value()));
		};
	}

	auto Or(auto resultProcess, auto parser, auto... parsers)
	{
		// call back the resultProcess to get the return type TODO check
		return[=]<typename Stream>(Stream & stream) -> ParserResult<invoke_result_t<decltype(resultProcess), typename invoke_result_t<decltype(parser), Stream&>::value_type>>
		{
			auto initPos = stream.CurrentPosition();
			vector<decltype(parser(stream))> results;
			results.reserve(sizeof...(parsers) + 1);

			results.push_back(parser(stream));
			if (results.back().has_value())
			{
				return resultProcess(move(results.back().value()));
			}
			
			static_cast<void>(((stream.RollbackTo(initPos), results.push_back(parsers(stream)), results.back().has_value()) or ...));
			if (results.back().has_value())
			{
				return resultProcess(move(results.back().value()));
			}

			ParseFailResult failResult{ .Message = "all options failed when parsing, errors:" };
			for (auto i = 0; auto const& x : results)
			{
				failResult.Message += format("\noption {}: {}", i++, x.error().Message);
			}

			return std::unexpected(move(failResult));
		};
	}
}
