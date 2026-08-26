export module Parser:HtmlLogger;

import std;
import Base;

using std::string_view;
using std::string;
using std::vector;
using std::stack;
using std::list;
using std::unique_ptr;
using std::move;
using std::format;

enum class Level
{
	In,
	Out,
	Here,
};

class HtmlDetail
{
private:
	string summary;
	// it should exist order between description and subDetails TODO
	vector<string> description;
	list<HtmlDetail> subDetails;

public:
	HtmlDetail(string summary)
		: summary(move(summary)), description(), subDetails()
	{
	}

	auto AppendDescription(string description) -> void
	{
		this->description.push_back(move(description));
	}

	auto AddChild(HtmlDetail subDetail) -> HtmlDetail*
	{
		subDetails.push_back(move(subDetail));
		return &subDetails.back();
	}

	auto ToHtml() const -> std::string
	{
		string result;
		result += "<details><summary>";
		result += summary;
		result += "</summary>";
		for (auto const& x : description)
		{
			result.append(x);
			result.append("<br>");
		}
		for (auto const& child : subDetails)
		{
			result += child.ToHtml();
		}
		result += "</details>";
		return result;
	}
};

class HtmlLogger
{
private:
	static const bool EnableHtmlLogger = true;
	String filename;
	vector<string> toks;
	unique_ptr<HtmlDetail> rootDetail; // workingDetails will keep the address of rootDetail, so we use heap to keep address identical when move(HtmlLogger)
	stack<HtmlDetail*> workingDetails;
public:
	static auto NewFromCurrentTime() -> HtmlLogger
	{
		auto now = std::chrono::system_clock::now();
		auto local = std::chrono::current_zone()->to_local(now);
		//auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		auto filename = format("parse-log-{:%Y-%m-%d_%H-%M-%S}.html", local);

		return HtmlLogger(String(filename));
	}

	HtmlLogger(String filename)
		: filename(move(filename)), toks(), rootDetail(std::make_unique<HtmlDetail>("root")), workingDetails()
	{
		workingDetails.push(rootDetail.get());
	}

	HtmlLogger(HtmlLogger&&) = default;
	HtmlLogger(HtmlLogger const&) = delete;
	HtmlLogger& operator=(HtmlLogger&&) = default;
	HtmlLogger& operator=(HtmlLogger const&) = delete;

	template <template <typename> class ActualStream, typename Tok>
	auto LogCode(ActualStream<Tok> stream) -> void
	{
		do
		{
			toks.push_back(stream.Current().Value);
		} while (stream.MoveNext());
	}

	template <typename... Args>
	auto Log(Level indent, std::format_string<Args...> format, Args&&... args) -> void
	{
		using std::ranges::views::split;
		using std::ranges::views::join_with;
		using std::ranges::to;
		using namespace std::string_view_literals;

		string msg = std::format(format, std::forward<Args>(args)...);
		msg = msg | split("\n"sv) | join_with("<br>"sv) | to<string>();
		switch (indent)
		{
		case Level::In:
		{
			auto* newDetail = workingDetails.top()->AddChild(HtmlDetail(move(msg)));
			workingDetails.push(newDetail);
			break;
		}
		case Level::Out:
			workingDetails.top()->AppendDescription(move(msg));
			workingDetails.pop();
			break;
		case Level::Here:
			// TODO fix this if, when root is done
			if (not workingDetails.empty())
			{
				workingDetails.top()->AppendDescription(move(msg));
			}
			break;
		default:
			throw std::invalid_argument(std::format("invalid indent type: {}", static_cast<int>(indent)));
		}
	}

	~HtmlLogger()
	{
		if (filename.Empty())
		{
			return;
		}
		if (EnableHtmlLogger)
		{
#if defined(_DEBUG) || !defined(NDEBUG)
			std::println("HtmlLogger destructor called, flushing to file: {}", filename);
#endif
			Flush();
		}
	}
private:
	auto Flush() -> void
	{
		std::ofstream ofs{std::string(filename), std::ios::out | std::ios::trunc};
		ofs << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>parser log</title>
<style>
details {
    padding-left: 10px;
}
</style>
</head>
<body>)";
		if (not toks.empty())
		{
			ofs << "<table border='1'><tr>";
			for (auto const& x : toks)
			{
				ofs << "<td>" << x << "</td>";
			}
			ofs << "</tr><tr>";
			for (size_t i = 0; i < toks.size(); ++i)
			{
				ofs << "<td>" << i << "</td>";
			}
			ofs << "</tr></table>";
		}
		ofs << rootDetail->ToHtml();
		ofs << R"(<script>
var genColorBit = function() {
    return parseInt(Math.random() * 128) + 100;
};
var es = document.getElementsByTagName('summary');
console.log('len', es.length);
for (var i = 0; i < es.length; i++) {
    console.log('set', i);
    var e = es[i];
    e.setAttribute("style", "background-color: rgb(" + genColorBit() + ',' + genColorBit() + ',' + genColorBit() + ');');
}
document.addEventListener('keydown', function(ev) {
    if (!ev.ctrlKey) {
		return;
	}
    var el = document.elementFromPoint(lastMouseX, lastMouseY);
    if (!el) {
        return;
    }
    var detail = el.closest('details');
    if (!detail) {
        return;
    }
    detail.open = true;
    while (true) {
        var children = detail.querySelectorAll(':scope > details');
        if (children.length !== 1) {
            break;
        }
        children[0].open = true;
        detail = children[0];
    }
});
var lastMouseX = 0, lastMouseY = 0;
document.addEventListener('mousemove', function(ev) {
    lastMouseX = ev.clientX;
    lastMouseY = ev.clientY;
});

var expandTilLastDetails = function() {
    var root = document.querySelector("body > details");
	root.open = true;
	while (root) {
		var children = root.querySelectorAll(':scope > details');
		if (children.length === 0) {
			break;
		}
		children[children.length - 1].open = true;
		root = children[children.length - 1];
	}
};
expandTilLastDetails();
</script>
</body>
</html>)";
	}
};

export
{
	class HtmlLogger;
	enum class Level;
}

