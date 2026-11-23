// SPDX-License-Identifier: MIT
//
// libsquare in process: square::call with a JsonValue request, as sqpg and
// sqgui make one, on the workspace and resources tests/run_check.sh makes
// (argv[1], argv[2]).

#include <stdio.h>  // stderr

import std;
import sq.data;
import square;

using sq::data::JsonValue;

namespace {

int failures = 0;

void check(bool ok, std::string_view what) {
    std::println("  {}   {}", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

JsonValue request(std::string_view command, JsonValue::Object args) {
    return JsonValue(JsonValue::Object{{"sq", JsonValue(std::int64_t{1})},
                                       {"command", JsonValue(command)},
                                       {"args", JsonValue(std::move(args))}});
}

std::string text(const JsonValue* v) {
    const std::string* s = v != nullptr ? v->string_if() : nullptr;
    return s != nullptr ? *s : std::string();
}

bool ok(const JsonValue& response) {
    const JsonValue* o = response.find("ok");
    return o != nullptr && o->boolean_if() != nullptr && *o->boolean_if();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::println(stderr, "usage: test_libsquare WORKSPACE RESOURCES");
        return 2;
    }
    const std::filesystem::path ws = argv[1], res = argv[2];

    const JsonValue where = square::call(request("where", {{"role", JsonValue("layouts")}}), ws / "sq_app");
    check(ok(where) && text(where.find("result")->find("path")) == "sq_app/layouts", "where, from a folder inside");

    const JsonValue info = square::call(request("info", {{"workspace", JsonValue(ws.string())}}), "/");
    check(ok(info) && text(info.find("result")->find("id")) == "dev.me.demo", "info, by args.workspace");

    const JsonValue unknown = square::call(request("where", {{"role", JsonValue("nothing")}}), ws);
    check(!ok(unknown) && text(unknown.find("error")->find("code")) == "workspace.role.unknown", "an error's code");

    const JsonValue shown = square::call(request("show", {{"id", JsonValue("kit.demo")}, {"search", JsonValue(res.string())}}), ws);
    check(ok(shown) && text(shown.find("result")->find("version")) == "0.2.0", "show, on a search path");

    const square::Answer direct = square::where(*square::open_workspace(ws, ws), "app");
    check(direct.has_value() && text(direct->find("path")) == "sq_app", "a question called directly");

    if (failures != 0) return 1;
    std::println("libsquare passed");
    return 0;
}
