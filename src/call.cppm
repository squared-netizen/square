// SPDX-License-Identifier: MIT
//
// square.call: the SDK's tool protocol (squared's docs/developer/sdk-tools.md;
// square's spec §5). One JSON request, one JSON response:
//
//   {"sq": 1, "command": "where", "args": {"role": "layouts", "workspace": "..."}}
//   {"sq": 1, "ok": true, "result": {"role": "layouts", "path": "sq_app/layouts"}}
//   {"sq": 1, "ok": false, "error": {"code": "workspace.role.unknown", "message": "...", "detail": {...}}}
//
// The same answers people get as words (tools/square.cpp).

export module square.call;

import std;
import sq.data;
import square.workspace;
import square.resources;

export namespace square {

inline constexpr std::string_view tool_version = "0.2.0";

/// What square answers: its commands and their arguments.
inline JsonValue describe() {
    const auto command = [](std::string_view name, std::string_view args, std::string_view does) {
        return JsonValue(JsonValue::Object{{"name", JsonValue(name)}, {"args", JsonValue(args)}, {"does", JsonValue(does)}});
    };
    return JsonValue(JsonValue::Object{
        {"tool", JsonValue("square")},
        {"version", JsonValue(tool_version)},
        {"protocol", JsonValue(std::int64_t{1})},
        {"commands", JsonValue(JsonValue::Array{
                         command("info", "{workspace?}", "the workspace: id, title, what it was made from, its roles"),
                         command("where", "{role, workspace?}", "a role's path, relative to the workspace"),
                         command("owner", "{path, workspace?}", "a path's class and origin, and whether it changed"),
                         command("verify", "{workspace?}", "every listed path that changed or went missing"),
                         command("resources", "{kind?, search?}", "the templates, kits, packages and assets on the search path"),
                         command("show", "{id, search?}", "one resource (id or id@version): what it is and needs"),
                         command("describe", "{}", "this"),
                     })},
    });
}

namespace detail {

inline JsonValue response_ok(JsonValue result) {
    return JsonValue(JsonValue::Object{{"sq", JsonValue(std::int64_t{1})}, {"ok", JsonValue(true)}, {"result", std::move(result)}});
}

inline JsonValue response_error(const Failure& f) {
    return JsonValue(JsonValue::Object{
        {"sq", JsonValue(std::int64_t{1})},
        {"ok", JsonValue(false)},
        {"error", JsonValue(JsonValue::Object{{"code", JsonValue(f.code)}, {"message", JsonValue(f.message)}, {"detail", f.detail}})},
    });
}

/// sq: 1, as a signed or an unsigned number (sq.data keeps a positive one as unsigned).
inline bool is_one(const JsonValue* sq) {
    if (sq == nullptr) return false;
    if (const auto* u = sq->unsigned_integer_if()) return *u == 1;
    if (const auto* i = sq->signed_integer_if()) return *i == 1;
    return false;
}

inline std::optional<std::string> arg(const JsonValue* args, std::string_view name) {
    const JsonValue* value = args != nullptr ? args->find(name) : nullptr;
    const std::string* s = value != nullptr ? value->string_if() : nullptr;
    return s != nullptr ? std::optional<std::string>(*s) : std::nullopt;
}

}  // namespace detail

/// One command, by name, with its arguments: what both `call` and the words run.
inline Answer run(std::string_view command, const JsonValue* args, const std::filesystem::path& from) {
    if (command == "describe") return describe();
    if (command == "resources") return resources(detail::arg(args, "search"), detail::arg(args, "kind"));
    if (command == "show") {
        const auto id = detail::arg(args, "id");
        if (!id) return std::unexpected(Failure{"request.args", "show: args.id, a resource's id"});
        return show(detail::arg(args, "search"), *id);
    }
    const auto given = detail::arg(args, "workspace");
    const auto ws = open_workspace(given ? std::optional<std::filesystem::path>(*given) : std::nullopt, from);
    if (!ws) return std::unexpected(ws.error());
    if (command == "info") return info(*ws);
    if (command == "verify") return verify(*ws);
    if (command == "where") {
        const auto role = detail::arg(args, "role");
        if (!role) return std::unexpected(Failure{"request.args", "where: args.role, a role's name"});
        return where(*ws, *role);
    }
    if (command == "owner") {
        const auto path = detail::arg(args, "path");
        if (!path) return std::unexpected(Failure{"request.args", "owner: args.path, a path in the workspace"});
        return owner(*ws, *path, from);
    }
    JsonValue::Array known;
    for (const char* name : {"info", "where", "owner", "verify", "resources", "show", "describe"}) known.emplace_back(name);
    return std::unexpected(Failure{"request.command.unknown", std::format("square answers no command {}", command),
                                   JsonValue(JsonValue::Object{{"commands", JsonValue(std::move(known))}})});
}

/// A whole request, as text, into its response, as a JsonValue.
inline JsonValue call(std::string_view request, const std::filesystem::path& from) {
    const auto parsed = sq::data::parse_json(request);
    if (!parsed) return detail::response_error({"request.json", "the request is not JSON"});
    if (!detail::is_one(parsed.value.find("sq"))) {
        return detail::response_error({"request.protocol", "not an SDK request (sq: 1)"});
    }
    const JsonValue* command = parsed.value.find("command");
    if (command == nullptr || command->string_if() == nullptr) {
        return detail::response_error({"request.command", "no command"});
    }
    const Answer answer = run(*command->string_if(), parsed.value.find("args"), from);
    return answer ? detail::response_ok(*answer) : detail::response_error(answer.error());
}

}  // namespace square
