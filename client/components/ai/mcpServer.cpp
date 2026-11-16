#include <global_root.h>
#include "mcpServer.h"
#include <lua/bridgeHook.h>
#include <logEvent.h>
#include <grid/setup.h>
#include <image/converter/png.h>

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef HAS_WINDOW
	#include "../cameraControl.h"
#endif

namespace Mcp
{
	using namespace DeltaWorks;

	namespace
	{
		//---------------------------------------------------------------------------------------------------------
		// minimal JSON (the engine's io/json.h only parses and does not expose what it parsed)
		//---------------------------------------------------------------------------------------------------------

		struct JValue;
		typedef std::shared_ptr<JValue>	PJValue;

		struct JValue
		{
			enum Type {Null, Bool, Number, Text, Array, Object}	type = Null;
			bool								b = false;
			double								n = 0;
			std::string							s;
			std::vector<PJValue>				a;
			std::vector<std::pair<std::string,PJValue> >	o;

			const JValue*	Get(const std::string&key) const
			{
				if (type != Object)
					return nullptr;
				for (const auto&kv : o)
					if (kv.first == key)
						return kv.second.get();
				return nullptr;
			}
			std::string		Str(const std::string&key, const std::string&otherwise = std::string()) const
			{
				const JValue*v = Get(key);
				return v && v->type == Text ? v->s : otherwise;
			}
			double			Num(const std::string&key, double otherwise) const
			{
				const JValue*v = Get(key);
				return v && v->type == Number ? v->n : otherwise;
			}
		};

		struct JParser
		{
			const char*p, *end;
			bool		failed = false;

			void	Skip()	{while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;}
			bool	Fail()	{failed = true; return false;}

			static void	AppendUTF8(std::string&out, unsigned cp)
			{
				if (cp < 0x80) out += char(cp);
				else if (cp < 0x800) {out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F));}
				else if (cp < 0x10000) {out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F));}
				else {out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F));}
			}

			bool	Hex4(unsigned&cp)
			{
				if (end - p < 4) return Fail();
				cp = 0;
				for (int i = 0; i < 4; i++)
				{
					const char c = *p++;
					cp <<= 4;
					if (c >= '0' && c <= '9') cp |= c - '0';
					else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
					else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
					else return Fail();
				}
				return true;
			}

			bool	ParseString(std::string&out)
			{
				if (p >= end || *p != '"') return Fail();
				p++;
				while (p < end && *p != '"')
				{
					char c = *p++;
					if (c != '\\') {out += c; continue;}
					if (p >= end) return Fail();
					c = *p++;
					switch (c)
					{
						case '"': out += '"'; break;
						case '\\': out += '\\'; break;
						case '/': out += '/'; break;
						case 'b': out += '\b'; break;
						case 'f': out += '\f'; break;
						case 'n': out += '\n'; break;
						case 'r': out += '\r'; break;
						case 't': out += '\t'; break;
						case 'u':
						{
							unsigned cp;
							if (!Hex4(cp)) return false;
							if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u')
							{
								p += 2;
								unsigned lo;
								if (!Hex4(lo)) return false;
								cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
							}
							AppendUTF8(out, cp);
							break;
						}
						default: return Fail();
					}
				}
				if (p >= end) return Fail();
				p++;
				return true;
			}

			PJValue	Parse(int depth = 0)
			{
				if (depth > 64) {Fail(); return nullptr;}
				Skip();
				if (p >= end) {Fail(); return nullptr;}
				PJValue v(new JValue());
				const char c = *p;
				if (c == '{')
				{
					v->type = JValue::Object;
					p++;
					Skip();
					if (p < end && *p == '}') {p++; return v;}
					while (true)
					{
						Skip();
						std::string key;
						if (!ParseString(key)) return nullptr;
						Skip();
						if (p >= end || *p != ':') {Fail(); return nullptr;}
						p++;
						PJValue item = Parse(depth+1);
						if (!item) return nullptr;
						v->o.push_back(std::make_pair(key, item));
						Skip();
						if (p < end && *p == ',') {p++; continue;}
						if (p < end && *p == '}') {p++; return v;}
						Fail(); return nullptr;
					}
				}
				if (c == '[')
				{
					v->type = JValue::Array;
					p++;
					Skip();
					if (p < end && *p == ']') {p++; return v;}
					while (true)
					{
						PJValue item = Parse(depth+1);
						if (!item) return nullptr;
						v->a.push_back(item);
						Skip();
						if (p < end && *p == ',') {p++; continue;}
						if (p < end && *p == ']') {p++; return v;}
						Fail(); return nullptr;
					}
				}
				if (c == '"')
				{
					v->type = JValue::Text;
					if (!ParseString(v->s)) return nullptr;
					return v;
				}
				if (end - p >= 4 && !strncmp(p, "true", 4)) {p += 4; v->type = JValue::Bool; v->b = true; return v;}
				if (end - p >= 5 && !strncmp(p, "false", 5)) {p += 5; v->type = JValue::Bool; return v;}
				if (end - p >= 4 && !strncmp(p, "null", 4)) {p += 4; return v;}
				if (c == '-' || (c >= '0' && c <= '9'))
				{
					const char*start = p;
					while (p < end && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' || (*p >= '0' && *p <= '9'))) p++;
					v->type = JValue::Number;
					v->n = atof(std::string(start, p).c_str());
					return v;
				}
				Fail();
				return nullptr;
			}
		};

		PJValue	ParseJSON(const std::string&text)
		{
			JParser parser;
			parser.p = text.data();
			parser.end = text.data() + text.size();
			PJValue rs = parser.Parse();
			parser.Skip();
			if (!rs || parser.failed || parser.p != parser.end)
				return nullptr;
			return rs;
		}

		/** JSON string literal (with quotes) of UTF-8 text */
		std::string	Quote(const std::string&text)
		{
			std::string rs = "\"";
			for (unsigned char c : text)
			{
				switch (c)
				{
					case '"': rs += "\\\""; break;
					case '\\': rs += "\\\\"; break;
					case '\n': rs += "\\n"; break;
					case '\r': rs += "\\r"; break;
					case '\t': rs += "\\t"; break;
					default:
						if (c < 0x20)
						{
							char buf[8];
							sprintf_s(buf, "\\u%04x", c);
							rs += buf;
						}
						else
							rs += char(c);
				}
			}
			return rs + "\"";
		}

		/** Re-serializes a parsed value (used to echo request ids exactly) */
		std::string	Serialize(const JValue*v)
		{
			if (!v)
				return "null";
			switch (v->type)
			{
				case JValue::Bool: return v->b ? "true" : "false";
				case JValue::Number:
				{
					char buf[64];
					if (v->n == double(INT64(v->n)))
						sprintf_s(buf, "%lld", (long long)INT64(v->n));
					else
						sprintf_s(buf, "%.17g", v->n);
					return buf;
				}
				case JValue::Text: return Quote(v->s);
				case JValue::Array:
				{
					std::string rs = "[";
					for (size_t i = 0; i < v->a.size(); i++)
						rs += (i ? "," : "") + Serialize(v->a[i].get());
					return rs + "]";
				}
				case JValue::Object:
				{
					std::string rs = "{";
					for (size_t i = 0; i < v->o.size(); i++)
						rs += (i ? "," : "") + Quote(v->o[i].first) + ":" + Serialize(v->o[i].second.get());
					return rs + "}";
				}
				default: return "null";
			}
		}

		std::string	Base64(const std::vector<unsigned char>&data)
		{
			static const char*digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string rs;
			rs.reserve((data.size() + 2) / 3 * 4);
			size_t i = 0;
			for (; i + 2 < data.size(); i += 3)
			{
				const unsigned v = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
				rs += digits[(v >> 18) & 63]; rs += digits[(v >> 12) & 63]; rs += digits[(v >> 6) & 63]; rs += digits[v & 63];
			}
			if (i + 1 == data.size())
			{
				const unsigned v = data[i] << 16;
				rs += digits[(v >> 18) & 63]; rs += digits[(v >> 12) & 63]; rs += "==";
			}
			elif (i + 2 == data.size())
			{
				const unsigned v = (data[i] << 16) | (data[i+1] << 8);
				rs += digits[(v >> 18) & 63]; rs += digits[(v >> 12) & 63]; rs += digits[(v >> 6) & 63]; rs += '=';
			}
			return rs;
		}

		//---------------------------------------------------------------------------------------------------------
		// the bridge, in memory (written and read by bridge.lua through Game.BridgeWrite/BridgeRead)
		//---------------------------------------------------------------------------------------------------------

		std::mutex					lock;
		std::condition_variable		changed;
		std::map<std::string,std::string>	files;
		std::string					lastState, helpName;
		std::atomic<bool>			on(false);

		/** "state_p1_120.txt" -> "p1_120"; empty if not a state file name */
		std::string	TurnKey(const std::string&stateName)
		{
			const std::string pre = "state_", post = ".txt";
			if (stateName.size() <= pre.size() + post.size() || stateName.compare(0, pre.size(), pre) || stateName.compare(stateName.size() - post.size(), post.size(), post))
				return std::string();
			return stateName.substr(pre.size(), stateName.size() - pre.size() - post.size());
		}

		/** Lock must be held. The turn waiting for an answer (key as above), or empty */
		std::string	PendingTurn()
		{
			const std::string key = TurnKey(lastState);
			if (key.empty())
				return std::string();
			if (files.count("orders_" + key + ".lua") || files.count("orders_" + key + ".none"))
				return std::string();
			return key;
		}

		/** Also keep the outside player's answers next to the bridge files, if HEXAVERSE_AI_BRIDGE names a folder */
		void	RecordFile(const std::string&name, const std::string&text)
		{
			#pragma warning(suppress: 4996)
			const char*folder = std::getenv("HEXAVERSE_AI_BRIDGE");
			if (!folder || !*folder)
				return;
			std::ofstream out(std::string(folder) + "\\" + name, std::ios::binary | std::ios::trunc);
			if (out)
				out << text;
		}

		bool	HookWrite(const std::string&name, const std::string&text)
		{
			{
				std::lock_guard<std::mutex> guard(lock);
				files[name] = text;
				if (!name.compare(0, 6, "state_"))
					lastState = name;
				elif (!name.compare(0, 5, "help_"))
					helpName = name;
			}
			changed.notify_all();
			return true;
		}

		std::string	HookRead(const std::string&name)
		{
			std::lock_guard<std::mutex> guard(lock);
			auto it = files.find(name);
			return it != files.end() ? it->second : std::string();
		}

		//---------------------------------------------------------------------------------------------------------
		// requests to the main / render thread (Visual Client)
		//---------------------------------------------------------------------------------------------------------

		std::atomic<bool>			hasWindow(false);
		std::atomic<bool>			wantShot(false);
		bool						shotReady = false;
		Image						shot;
		std::atomic<int>			lookCell(0), lookZoom(0);
		std::atomic<bool>			lookPending(false);

		//---------------------------------------------------------------------------------------------------------
		// tools
		//---------------------------------------------------------------------------------------------------------

		struct ToolResult
		{
			std::string	contentJSON;	//JSON array of content blocks
			bool		isError = false;
		};

		ToolResult	Text(const std::string&text, bool isError = false)
		{
			ToolResult rs;
			rs.contentJSON = "[{\"type\":\"text\",\"text\":" + Quote(text) + "}]";
			rs.isError = isError;
			return rs;
		}

		std::string	TurnRound(const std::string&key)
		{
			const size_t at = key.rfind('_');
			return at == std::string::npos ? key : key.substr(at + 1);
		}

		/** Waits (lock held via the given unique_lock) until the result of the given turn has been written */
		ToolResult	AwaitResult(std::unique_lock<std::mutex>&guard, const std::string&key)
		{
			const std::string name = "result_" + key + ".txt";
			changed.wait_for(guard, std::chrono::seconds(120), [&]() {return files.count(name) > 0;});
			auto it = files.find(name);
			if (it == files.end())
				return Text("Orders for round " + TurnRound(key) + " handed over; the game has not reported back within 120 seconds.");
			const bool error = it->second.find("ERROR") != std::string::npos;
			return Text("Round " + TurnRound(key) + ": " + it->second, error);
		}

		ToolResult	Answer(const std::string&suffix, const std::string&text)
		{
			std::unique_lock<std::mutex> guard(lock);
			const std::string key = PendingTurn();
			if (key.empty())
				return Text("It is not your turn: the game is not waiting for orders right now. Call wait_for_turn first.", true);
			const std::string name = "orders_" + key + suffix;
			files[name] = text;
			RecordFile(name, text);
			return AwaitResult(guard, key);
		}

		ToolResult	WaitForTurn(double seconds)
		{
			if (!(seconds > 0)) seconds = 120;
			if (seconds > 600) seconds = 600;
			std::unique_lock<std::mutex> guard(lock);
			changed.wait_for(guard, std::chrono::milliseconds(INT64(seconds * 1000)), []() {return !PendingTurn().empty();});
			const std::string key = PendingTurn();
			if (key.empty())
				return Text(lastState.empty() ? "The game has not asked yet (no state so far). Call wait_for_turn again." : "Not your turn yet (the game is still playing out rounds). Call wait_for_turn again.");
			return Text(files[lastState]);
		}

		void	Downscale(const Image&in, Image&out, unsigned maxWidth)
		{
			const unsigned w = in.GetWidth(), h = in.GetHeight(), c = in.GetChannels();
			if (w <= maxWidth || maxWidth == 0)
			{
				out = in;
				return;
			}
			const unsigned ow = maxWidth, oh = std::max<unsigned>(1, unsigned(UINT64(h) * maxWidth / w));
			out.SetSize(ow, oh, BYTE(c));
			for (unsigned y = 0; y < oh; y++)
			{
				const unsigned y0 = unsigned(UINT64(y) * h / oh), y1 = std::max(y0 + 1, unsigned(UINT64(y + 1) * h / oh));
				for (unsigned x = 0; x < ow; x++)
				{
					const unsigned x0 = unsigned(UINT64(x) * w / ow), x1 = std::max(x0 + 1, unsigned(UINT64(x + 1) * w / ow));
					unsigned sum[4] = {0, 0, 0, 0}, n = 0;
					for (unsigned yy = y0; yy < y1 && yy < h; yy++)
						for (unsigned xx = x0; xx < x1 && xx < w; xx++)
						{
							const BYTE*px = in.GetPixel(xx, yy);
							for (unsigned k = 0; k < c && k < 4; k++)
								sum[k] += px[k];
							n++;
						}
					BYTE*o = out.GetPixel(x, y);
					for (unsigned k = 0; k < c && k < 4; k++)
						o[k] = BYTE(n ? sum[k] / n : 0);
				}
			}
		}

		ToolResult	Screenshot(double maxWidth)
		{
			if (!hasWindow)
				return Text("No window: this is the Console Client, which renders nothing. Screenshots need the Visual Client.", true);
			Image frame;
			{
				std::unique_lock<std::mutex> guard(lock);
				shotReady = false;
				wantShot = true;
				if (!changed.wait_for(guard, std::chrono::seconds(10), []() {return shotReady;}))
				{
					wantShot = false;
					return Text("The window did not deliver a frame within 10 seconds (minimized or loading?).", true);
				}
				frame = shot;
				shot = Image();
			}
			if (frame.GetChannels() > 3)
				frame.SetChannels(3);
			Image scaled;
			Downscale(frame, scaled, unsigned(maxWidth > 0 ? std::min(maxWidth, 4096.0) : 1280.0));
			Ctr::Array<BYTE> png;
			PNG::CompressToArray(scaled, png);
			std::vector<unsigned char> bytes(png.pointer(), png.pointer() + png.Count());
			ToolResult rs;
			rs.contentJSON = "[{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":\"" + Base64(bytes) + "\"}]";
			return rs;
		}

		ToolResult	LookAt(const JValue*args)
		{
			if (!hasWindow)
				return Text("No window: this is the Console Client, which has no camera.", true);
			const int cell = args ? int(args->Num("cell", 0)) : 0;
			if (cell < 1 || cell > int(Grid::LowSetup::GridType::CellCount))
				return Text("look_at needs 'cell', a cell index between 1 and " + std::to_string(int(Grid::LowSetup::GridType::CellCount)) + " (as in the state's PORTS AND POLES list).", true);
			lookCell = cell;
			lookZoom = args ? int(args->Num("zoom", 0)) : 0;
			lookPending = true;
			return Text("Camera moving to cell " + std::to_string(cell) + ". Take a screenshot in a moment.");
		}

		const char*ToolList =
			"{\"tools\":["
			"{\"name\":\"get_help\",\"description\":\"How to play through this interface: the order helpers (Bridge.Take, Bridge.BuildAt, ...) and the structure types.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
			"{\"name\":\"get_state\",\"description\":\"The last state the game wrote (round, resources, vital structures, ports, regions). Does not wait.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
			"{\"name\":\"wait_for_turn\",\"description\":\"Blocks until the game asks for orders (it pauses meanwhile) and returns the state. Returns earlier with a note if the timeout passes.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"timeout_seconds\":{\"type\":\"number\",\"description\":\"Default 120, at most 600\"}}}},"
			"{\"name\":\"orders\",\"description\":\"Answers the current turn with Lua orders (as in the help: Bridge.* helpers or plain Lua with the game API, inside the AI's sandbox). Returns the result, including any error.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"lua\":{\"type\":\"string\"}},\"required\":[\"lua\"]}},"
			"{\"name\":\"end_turn\",\"description\":\"Answers the current turn without new orders: standing orders carry on.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
			"{\"name\":\"screenshot\",\"description\":\"The game window's current frame as a PNG (Visual Client only).\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"max_width\":{\"type\":\"number\",\"description\":\"Downscale to at most this width in pixels (default 1280)\"}}}},"
			"{\"name\":\"look_at\",\"description\":\"Turns the camera to a cell (Visual Client only); optional zoom steps, positive zooms in.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"cell\":{\"type\":\"number\"},\"zoom\":{\"type\":\"number\"}},\"required\":[\"cell\"]}}"
			"]}";

		ToolResult	CallTool(const std::string&name, const JValue*args)
		{
			if (name == "get_help")
			{
				std::lock_guard<std::mutex> guard(lock);
				return Text(helpName.empty() ? std::string("No help yet: the AI script has not started the bridge (is the faction running lua/AI/bridge.lua?).") : files[helpName]);
			}
			if (name == "get_state")
			{
				std::lock_guard<std::mutex> guard(lock);
				return Text(lastState.empty() ? std::string("No state yet.") : files[lastState]);
			}
			if (name == "wait_for_turn")
				return WaitForTurn(args ? args->Num("timeout_seconds", 120) : 120);
			if (name == "orders")
			{
				const std::string lua = args ? args->Str("lua") : std::string();
				if (lua.empty())
					return Text("orders needs 'lua', the Lua text of the orders.", true);
				return Answer(".lua", lua);
			}
			if (name == "end_turn")
				return Answer(".none", "carry on");
			if (name == "screenshot")
				return Screenshot(args ? args->Num("max_width", 1280) : 1280);
			if (name == "look_at")
				return LookAt(args);
			return Text("Unknown tool '" + name + "'", true);
		}

		//---------------------------------------------------------------------------------------------------------
		// JSON-RPC
		//---------------------------------------------------------------------------------------------------------

		std::string	RpcError(const std::string&idJSON, int code, const std::string&message)
		{
			return "{\"jsonrpc\":\"2.0\",\"id\":" + idJSON + ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":" + Quote(message) + "}}";
		}

		std::string	RpcResult(const std::string&idJSON, const std::string&resultJSON)
		{
			return "{\"jsonrpc\":\"2.0\",\"id\":" + idJSON + ",\"result\":" + resultJSON + "}";
		}

		/** Handles one JSON-RPC message; returns the response, or empty for a notification */
		std::string	HandleMessage(const JValue&msg)
		{
			const JValue*id = msg.Get("id");
			const std::string idJSON = Serialize(id);
			const JValue*methodV = msg.Get("method");
			if (msg.type != JValue::Object || !methodV || methodV->type != JValue::Text)
				return id ? RpcError(idJSON, -32600, "Invalid Request") : std::string();
			const std::string method = methodV->s;
			const JValue*params = msg.Get("params");
			if (!id)
				return std::string();	//notifications (notifications/initialized, cancelled, ...): nothing to answer
			if (method == "initialize")
			{
				std::string version = params ? params->Str("protocolVersion", "2025-03-26") : std::string("2025-03-26");
				return RpcResult(idJSON, "{\"protocolVersion\":" + Quote(version) + ",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"hexaverse\",\"version\":\"1.0\"},"
					"\"instructions\":" + Quote("You play a Hexaverse faction. Call get_help once, then loop: wait_for_turn, decide, orders (or end_turn). The game pauses while it waits for you.") + "}");
			}
			if (method == "ping")
				return RpcResult(idJSON, "{}");
			if (method == "tools/list")
				return RpcResult(idJSON, ToolList);
			if (method == "tools/call")
			{
				const std::string name = params ? params->Str("name") : std::string();
				if (name.empty())
					return RpcError(idJSON, -32602, "tools/call needs params.name");
				const ToolResult r = CallTool(name, params->Get("arguments"));
				return RpcResult(idJSON, "{\"content\":" + r.contentJSON + ",\"isError\":" + (r.isError ? "true" : "false") + "}");
			}
			return RpcError(idJSON, -32601, "Method not found: " + method);
		}

		//---------------------------------------------------------------------------------------------------------
		// HTTP on 127.0.0.1
		//---------------------------------------------------------------------------------------------------------

		bool	SendAll(SOCKET s, const std::string&data)
		{
			size_t at = 0;
			while (at < data.size())
			{
				const int n = send(s, data.data() + at, int(std::min<size_t>(data.size() - at, 1 << 20)), 0);
				if (n <= 0)
					return false;
				at += size_t(n);
			}
			return true;
		}

		void	Respond(SOCKET s, const char*status, const std::string&body, const std::string&contentType = "application/json", const std::string&extraHeaders = std::string())
		{
			std::string head = std::string("HTTP/1.1 ") + status + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n";
			if (!body.empty())
				head += "Content-Type: " + contentType + "\r\n";
			head += extraHeaders + "\r\n";
			SendAll(s, head + body);
		}

		std::string	Lower(std::string s)
		{
			for (char&c : s)
				if (c >= 'A' && c <= 'Z')
					c = char(c - 'A' + 'a');
			return s;
		}

		void	HandleConnection(SOCKET s)
		{
			DWORD timeout = 30000;
			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
			std::string data;
			char buf[8192];
			size_t headerEnd = std::string::npos;
			while (headerEnd == std::string::npos)
			{
				const int n = recv(s, buf, sizeof(buf), 0);
				if (n <= 0)
					return;
				data.append(buf, size_t(n));
				headerEnd = data.find("\r\n\r\n");
				if (data.size() > 65536 && headerEnd == std::string::npos)
					return;
			}
			std::istringstream head(data.substr(0, headerEnd));
			std::string requestLine;
			std::getline(head, requestLine);
			std::string method, path;
			{
				std::istringstream rl(requestLine);
				rl >> method >> path;
			}
			size_t contentLength = 0;
			std::string origin;
			for (std::string line; std::getline(head, line);)
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				const size_t colon = line.find(':');
				if (colon == std::string::npos)
					continue;
				const std::string key = Lower(line.substr(0, colon));
				std::string value = line.substr(colon + 1);
				while (!value.empty() && value.front() == ' ')
					value.erase(0, 1);
				if (key == "content-length")
					contentLength = size_t(atoll(value.c_str()));
				elif (key == "origin")
					origin = Lower(value);
			}
			//(against DNS rebinding: a browser page may only talk to us from a local origin)
			if (!origin.empty() && origin.find("://localhost") == std::string::npos && origin.find("://127.0.0.1") == std::string::npos)
			{
				Respond(s, "403 Forbidden", "", "");
				return;
			}
			const size_t q = path.find('?');
			if (q != std::string::npos)
				path.resize(q);
			if (path != "/mcp")
			{
				Respond(s, "404 Not Found", "", "");
				return;
			}
			if (method == "GET" || method == "DELETE")
			{
				Respond(s, "405 Method Not Allowed", "", "", "Allow: POST\r\n");
				return;
			}
			if (method != "POST")
			{
				Respond(s, "405 Method Not Allowed", "", "", "Allow: POST\r\n");
				return;
			}
			if (contentLength > (16u << 20))
			{
				Respond(s, "413 Payload Too Large", "", "");
				return;
			}
			std::string body = data.substr(headerEnd + 4);
			while (body.size() < contentLength)
			{
				const int n = recv(s, buf, sizeof(buf), 0);
				if (n <= 0)
					return;
				body.append(buf, size_t(n));
			}
			body.resize(contentLength);

			PJValue msg = ParseJSON(body);
			if (!msg)
			{
				Respond(s, "400 Bad Request", RpcError("null", -32700, "Parse error"));
				return;
			}
			std::string reply;
			if (msg->type == JValue::Array)
			{
				std::string all;
				for (const auto&item : msg->a)
				{
					const std::string one = HandleMessage(*item);
					if (!one.empty())
						all += (all.empty() ? "" : ",") + one;
				}
				reply = all.empty() ? std::string() : "[" + all + "]";
			}
			else
				reply = HandleMessage(*msg);
			if (reply.empty())
				Respond(s, "202 Accepted", "", "");
			else
				Respond(s, "200 OK", reply, "application/json", "Mcp-Session-Id: hexaverse\r\n");
		}

		void	Serve(unsigned port)
		{
			WSADATA wsa;
			WSAStartup(MAKEWORD(2,2), &wsa);
			SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (listener == INVALID_SOCKET)
			{
				LogEvent(EventClass::Preset::scriptError, "MCP: unable to create a socket");
				return;
			}
			sockaddr_in addr = {};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(USHORT(port));
			inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);	//never other interfaces
			BOOL exclusive = TRUE;
			setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
			if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listener, 4) != 0)
			{
				LogEvent(EventClass::Preset::scriptError, "MCP: unable to listen on 127.0.0.1:" + String(port) + " (port in use?)");
				closesocket(listener);
				return;
			}
			LogEvent(EventClass::Preset::common, "MCP: listening on http://127.0.0.1:" + String(port) + "/mcp");
			while (true)
			{
				SOCKET s = accept(listener, nullptr, nullptr);
				if (s == INVALID_SOCKET)
					continue;
				//one client at a time: each request is handled to the end before the next is accepted
				try
				{
					HandleConnection(s);
				}
				catch (const std::exception&e)
				{
					LogEvent(EventClass::Preset::scriptError, "MCP: request failed: " + String(e.what()));
				}
				shutdown(s, SD_SEND);
				closesocket(s);
			}
		}
	}

	void	Start(unsigned port)
	{
		if (on.exchange(true))
			return;
		BridgeHook::write = HookWrite;
		BridgeHook::read = HookRead;
		std::thread(Serve, port).detach();
	}

	bool	IsOn()
	{
		return on;
	}

	void	SetHasWindow()
	{
		hasWindow = true;
	}

	void	MainThreadFrame()
	{
		#ifdef HAS_WINDOW
			hasWindow = true;
			if (lookPending.exchange(false))
			{
				const int cell = lookCell;
				Camera::TranslateTo(Grid::ToWorld(Grid::LCellIdI(index_t(cell - 1), 0)));
				const int zoom = lookZoom;
				for (int i = 0; i < zoom && i < 20; i++)
					Camera::Event::ZoomIn();
				for (int i = 0; i > zoom && i > -20; i--)
					Camera::Event::ZoomOut();
			}
		#endif
	}

	bool	ScreenshotWanted()
	{
		return wantShot;
	}

	void	DeliverScreenshot(const Image&frame)
	{
		{
			std::lock_guard<std::mutex> guard(lock);
			shot = frame;
			shotReady = true;
			wantShot = false;
		}
		changed.notify_all();
	}
}
