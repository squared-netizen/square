// SPDX-License-Identifier: MIT
//
// square: the one tool that knows what a workspace is (docs/spec.md). Phase
// 1, read: the questions, as words for people and as one JSON call for
// programs (`square call`), the same answers either way (§5).
//
//   square info                       what this workspace is
//   square where ROLE                 a role's path: app, android, layouts, ...
//   square owner PATH                 a path's class, origin, and whether it changed
//   square verify                     what changed since it was written
//   square resources [KIND]           templates, kits, packages and assets found
//   square show ID[@VERSION]          one of them: what it is and needs
//   square call < REQUEST.json        one request, one response (the SDK's protocol)
//   square describe                   what it answers
//
//   --workspace DIR   that workspace, rather than the one here or above
//   --resources PATH  search PATH[:PATH...] for resources ($SQUARE_RESOURCES)
//   --json            the answer as JSON (the call's result)

#include <stdio.h>  // stderr, stdout: macros, which import std does not bring

import std;
import sq.data;
import square.workspace;
import square.call;

namespace {

int usage(std::FILE* out = stderr, int status = 2) {
    std::println(out,
                 "usage: square [--workspace DIR] [--json] info\n"
                 "       square [--workspace DIR] [--json] where ROLE\n"
                 "       square [--workspace DIR] [--json] owner PATH\n"
                 "       square [--workspace DIR] [--json] verify\n"
                 "       square [--resources PATH] [--json] resources [KIND]\n"
                 "       square [--resources PATH] [--json] show ID[@VERSION]\n"
                 "       square call < REQUEST.json\n"
                 "       square describe\n"
                 "       square --help | --version");
    return status;
}

std::string text(const sq::data::JsonValue* value) {
    const std::string* s = value != nullptr ? value->string_if() : nullptr;
    return s != nullptr ? *s : std::string();
}

std::string json(const sq::data::JsonValue& value, bool pretty) {
    auto written = sq::data::write_json(value, {.pretty = pretty});
    return written ? written.text : std::string("{}");
}

/// The answer as words: what a person asked for, and nothing a program needs.
void say(std::string_view command, const sq::data::JsonValue& r) {
    if (command == "where") {
        std::println("{}", text(r.find("path")));
    } else if (command == "owner") {
        const sq::data::JsonValue* changed = r.find("changed");
        const bool was_changed = changed != nullptr && changed->boolean_if() != nullptr && *changed->boolean_if();
        const std::string origin = text(r.find("origin"));
        std::println("{}  {}{}{}", text(r.find("path")), text(r.find("class")),
                     origin.empty() ? "" : "  from " + origin, was_changed ? "  (changed since written)" : "");
    } else if (command == "info") {
        std::println("{}  {} {}", text(r.find("title")), text(r.find("id")), text(r.find("version")));
        if (const sq::data::JsonValue* made = r.find("made_from")) {
            std::println("  template   {}", text(made->find("template")));
            for (const char* list : {"kits", "packages"}) {
                const sq::data::JsonValue* items = made->find(list);
                if (items == nullptr || items->array_if() == nullptr || items->array_if()->empty()) continue;
                std::string joined;
                for (const auto& item : *items->array_if()) joined += (joined.empty() ? "" : ", ") + text(&item);
                std::println("  {:<10} {}", list, joined);
            }
            std::println("  generator  {}", text(made->find("generator")));
        }
        if (const sq::data::JsonValue* roles = r.find("roles"); roles != nullptr && roles->object_if() != nullptr) {
            for (const auto& [role, path] : *roles->object_if()) std::println("  {:<10} {}", role, text(&path));
        }
    } else if (command == "verify") {
        const sq::data::JsonValue* findings = r.find("findings");
        const auto* list = findings != nullptr ? findings->array_if() : nullptr;
        if (list == nullptr || list->empty()) {
            std::println("nothing changed since it was written");
            return;
        }
        for (const auto& f : *list) {
            const sq::data::JsonValue* overwrites = f.find("update_overwrites");
            const bool generated = overwrites != nullptr && overwrites->boolean_if() != nullptr && *overwrites->boolean_if();
            const std::string state = text(f.find("state"));
            const std::string cls = text(f.find("class"));
            if (generated) {
                std::println("{:<8} {}  ({}; an update would overwrite it)", state, text(f.find("path")), cls);
            } else {
                // The developer's: edited or removed on purpose, as seeded means.
                std::println("{:<8} {}  ({}: yours)", state == "changed" ? "edited" : state, text(f.find("path")), cls);
            }
        }
    } else if (command == "resources") {
        const sq::data::JsonValue* list = r.find("resources");
        if (list != nullptr && list->array_if() != nullptr) {
            for (const auto& c : *list->array_if()) {
                std::println("{:<9} {:<26} {:<9} {}", text(c.find("kind")), text(c.find("id")), text(c.find("version")),
                             text(c.find("title")));
            }
        }
        if (list == nullptr || list->array_if() == nullptr || list->array_if()->empty()) {
            std::println("no resources found (search: --resources PATH or $SQUARE_RESOURCES)");
        }
        if (const sq::data::JsonValue* problems = r.find("problems"); problems != nullptr && problems->array_if() != nullptr) {
            for (const auto& p : *problems->array_if()) {
                std::println(stderr, "square: refused {}: {}", text(p.find("path")), text(p.find("message")));
            }
        }
    } else if (command == "show") {
        std::println("{} {}  {}  {}", text(r.find("id")), text(r.find("version")), text(r.find("kind")), text(r.find("title")));
        if (const std::string d = text(r.find("description")); !d.empty()) std::println("  {}", d);
        for (const char* key : {"requires", "requires_features"}) {
            const sq::data::JsonValue* v = r.find(key);
            if (v == nullptr || v->is_null()) continue;
            if (v->array_if() != nullptr && v->array_if()->empty()) continue;
            if (v->object_if() != nullptr && v->object_if()->empty()) continue;
            std::println("  {:<9} {}", std::string_view(key) == "requires" ? "requires" : "features", json(*v, false));
        }
        const sq::data::JsonValue* files = r.find("files");
        std::println("  files     {}", files != nullptr ? json(*files, false) : "?");
        std::println("  path      {}", text(r.find("path")));
    } else {
        std::println("{}", json(r, true));
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    std::optional<std::string> workspace;
    std::optional<std::string> resource_path;
    bool as_json = false;
    std::vector<std::string_view> words;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h" || args[i] == "help") return usage(stdout, 0);
        if (args[i] == "--version" || args[i] == "version") {
            std::println("square {}", square::tool_version);
            return 0;
        }
        if (args[i] == "--json") {
            as_json = true;
        } else if (args[i] == "--workspace") {
            if (i + 1 >= args.size()) return usage();
            workspace = std::string(args[++i]);
        } else if (args[i] == "--resources") {
            if (i + 1 >= args.size()) return usage();
            resource_path = std::string(args[++i]);
        } else if (args[i].starts_with("--")) {
            return usage();
        } else {
            words.push_back(args[i]);
        }
    }
    if (words.empty()) return usage();
    const std::filesystem::path here = std::filesystem::current_path();
    const std::string_view command = words[0];

    if (command == "call") {
        const std::string request((std::istreambuf_iterator<char>(std::cin)), {});
        const sq::data::JsonValue response = square::call(request, here);
        std::println("{}", json(response, false));
        const sq::data::JsonValue* ok = response.find("ok");
        return ok != nullptr && ok->boolean_if() != nullptr && *ok->boolean_if() ? 0 : 1;
    }

    sq::data::JsonValue::Object request_args;
    if (workspace) request_args.emplace("workspace", sq::data::JsonValue(*workspace));
    if (resource_path) request_args.emplace("search", sq::data::JsonValue(*resource_path));
    if (command == "where" || command == "owner" || command == "show") {
        if (words.size() != 2) return usage();
        request_args.emplace(command == "where" ? "role" : command == "owner" ? "path" : "id", sq::data::JsonValue(words[1]));
    } else if (command == "resources") {
        if (words.size() > 2) return usage();
        if (words.size() == 2) request_args.emplace("kind", sq::data::JsonValue(words[1]));
    } else if (words.size() != 1) {
        return usage();
    }
    const sq::data::JsonValue args_value(std::move(request_args));
    const square::Answer answer = square::run(command, &args_value, here);
    if (!answer) {
        std::println(stderr, "square: {}", answer.error().message);
        if (as_json) std::println(stderr, "{}", json(answer.error().detail, false));
        return 1;
    }
    if (as_json || command == "describe") {
        std::println("{}", json(*answer, true));
    } else {
        say(command, *answer);
    }
    // verify composes into a check: non-zero when a generated file changed
    // or went missing (what an update would undo), not for the developer's
    // own edits to seeded files.
    if (command == "verify") {
        const sq::data::JsonValue* conflicts = answer->find("conflicts");
        const auto* n = conflicts != nullptr ? conflicts->signed_integer_if() : nullptr;
        const auto* u = conflicts != nullptr ? conflicts->unsigned_integer_if() : nullptr;
        return (n != nullptr && *n > 0) || (u != nullptr && *u > 0) ? 1 : 0;
    }
    return 0;
}
