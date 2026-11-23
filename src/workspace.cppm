// SPDX-License-Identifier: MIT
//
// square.workspace: what a workspace is, and the questions about it
// (docs/spec.md §2, §3: phase 1, read).
//
// A workspace is a folder holding SQ-INF/manifest.json with kind
// "workspace": an exploded cartridge (§2), its manifest read by sq.cart as
// the cartridge format asks (format, format_version 3, id, version, kind). Its manifest says what it was made
// from and where its parts are (consumers.square); SQ-INF/files.json, the
// provenance table, says for each path a template, kit or package wrote its
// class, origin and sha256 as written (§2.2, §9). square reads both and the
// files themselves, and writes nothing in this phase. It reads them as
// files, not by opening the whole folder as a cartridge: a workspace holds
// build output and links a cartridge may not, and square answers about the
// paths the table lists, not every file under the root.
//
// Every question answers a sq::data::JsonValue, or a Failure with a dotted
// code a tool can act on (§5): the words people see and the calls programs
// make are the same answers.

export module square.workspace;

import std;
import sq.data;
import sq.cart;

export namespace square {

using sq::data::JsonValue;

/// Why a question could not be answered: a dotted code, a sentence, and
/// detail a tool can act on.
struct Failure {
    std::string code;
    std::string message;
    JsonValue detail{JsonValue::Object{}};
};

using Answer = std::expected<JsonValue, Failure>;

/// A workspace, opened: its root (absolute) and what SQ-INF holds.
struct Workspace {
    std::filesystem::path root;
    JsonValue manifest;
    JsonValue files{JsonValue::Object{}};  ///< the provenance table; empty when there is none
};

namespace detail {

inline std::optional<std::string> read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

inline std::string text_of(const JsonValue* value) {
    const std::string* s = value != nullptr ? value->string_if() : nullptr;
    return s != nullptr ? *s : std::string();
}

/// The square section of the manifest: consumers.square, or nothing.
inline const JsonValue* square_section(const JsonValue& manifest) {
    const JsonValue* consumers = manifest.find("consumers");
    return consumers != nullptr ? consumers->find("square") : nullptr;
}

/// `dir`'s manifest, as sq.cart reads one, when it is a workspace's.
inline std::optional<sq::cart::Manifest> workspace_manifest(const std::filesystem::path& dir) {
    const auto text = read_text(dir / "SQ-INF" / "manifest.json");
    if (!text) return std::nullopt;
    auto manifest = sq::cart::parse_manifest(*text);
    if (!manifest || manifest->kind != "workspace") return std::nullopt;
    return std::move(*manifest);
}

}  // namespace detail

/// The workspace: `given` when there is one (--workspace, args.workspace),
/// else the nearest folder at or above `start` holding a workspace manifest,
/// as git finds .git (§2.3).
inline std::expected<Workspace, Failure> open_workspace(const std::optional<std::filesystem::path>& given,
                                                       const std::filesystem::path& start) {
    std::error_code ec;
    std::filesystem::path root;
    if (given) {
        root = std::filesystem::weakly_canonical(*given, ec);
        if (ec || !detail::workspace_manifest(root)) {
            return std::unexpected(Failure{"workspace.not_found",
                                           std::format("{} is not a workspace (no SQ-INF/manifest.json of kind workspace)",
                                                       given->string()),
                                           JsonValue(JsonValue::Object{{"path", JsonValue(given->string())}})});
        }
    } else {
        std::filesystem::path here = std::filesystem::weakly_canonical(start, ec);
        for (;;) {
            if (detail::workspace_manifest(here)) {
                root = here;
                break;
            }
            if (here == here.root_path() || !here.has_parent_path() || here.parent_path() == here) {
                return std::unexpected(Failure{"workspace.not_found",
                                               "no workspace here or above (a folder with SQ-INF/manifest.json of kind workspace)",
                                               JsonValue(JsonValue::Object{{"from", JsonValue(start.string())}})});
            }
            here = here.parent_path();
        }
    }
    Workspace ws;
    ws.root = root;
    const auto manifest = detail::workspace_manifest(root);
    if (!manifest) {
        return std::unexpected(Failure{"workspace.manifest.invalid", "SQ-INF/manifest.json changed while it was read",
                                       JsonValue(JsonValue::Object{})});
    }
    ws.manifest = sq::data::parse_json(manifest->json).value;
    if (const auto text = detail::read_text(root / "SQ-INF" / "files.json")) {
        const auto files = sq::data::parse_json(*text);
        if (!files || files.value.object_if() == nullptr) {
            return std::unexpected(Failure{"workspace.files.invalid", "SQ-INF/files.json is not a JSON object",
                                           JsonValue(JsonValue::Object{})});
        }
        ws.files = files.value;
    }
    return ws;
}

/// What the workspace is: its identity, what it was made from, its roles.
inline Answer info(const Workspace& ws) {
    const JsonValue* square = detail::square_section(ws.manifest);
    const JsonValue* made_from = square != nullptr ? square->find("made_from") : nullptr;
    const JsonValue* roles = square != nullptr ? square->find("roles") : nullptr;
    const JsonValue::Object* files = ws.files.object_if();
    return JsonValue(JsonValue::Object{
        {"workspace", JsonValue(ws.root.string())},
        {"id", JsonValue(detail::text_of(ws.manifest.find("id")))},
        {"title", JsonValue(detail::text_of(ws.manifest.find("title")))},
        {"version", JsonValue(detail::text_of(ws.manifest.find("version")))},
        {"made_from", made_from != nullptr ? *made_from : JsonValue(JsonValue::Object{})},
        {"roles", roles != nullptr ? *roles : JsonValue(JsonValue::Object{})},
        {"files", JsonValue(static_cast<std::int64_t>(files != nullptr ? files->size() : 0))},
    });
}

/// A role's path, relative to the workspace (§2.2: app, android, assets,
/// layouts, build, docs, as the template named them).
inline Answer where(const Workspace& ws, std::string_view role) {
    const JsonValue* square = detail::square_section(ws.manifest);
    const JsonValue* roles = square != nullptr ? square->find("roles") : nullptr;
    const JsonValue* path = roles != nullptr ? roles->find(role) : nullptr;
    if (path == nullptr || path->string_if() == nullptr) {
        JsonValue::Array known;
        if (roles != nullptr && roles->object_if() != nullptr) {
            for (const auto& [name, _] : *roles->object_if()) known.emplace_back(name);
        }
        return std::unexpected(Failure{"workspace.role.unknown", std::format("no role named {}", role),
                                       JsonValue(JsonValue::Object{{"roles", JsonValue(std::move(known))}})});
    }
    return JsonValue(JsonValue::Object{{"role", JsonValue(role)}, {"path", *path}});
}

namespace detail {

/// `path` as the provenance table names it: relative to the workspace, with
/// forward slashes. A relative path is taken from `from` (the caller's
/// folder), an absolute one as it is.
inline std::optional<std::string> workspace_path(const Workspace& ws, std::string_view path,
                                                 const std::filesystem::path& from) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.is_relative()) p = from / p;
    p = std::filesystem::weakly_canonical(p, ec).lexically_normal();
    const std::filesystem::path rel = p.lexically_relative(ws.root);
    if (rel.empty() || *rel.begin() == "..") return std::nullopt;
    return rel.generic_string();
}

/// A file's SHA-256 (what sha256sum prints), read in pieces.
inline std::optional<std::string> file_sha256(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    sq::cart::Sha256 hash;
    std::array<char, 65536> buffer{};
    while (in) {
        in.read(buffer.data(), buffer.size());
        hash.update(std::string_view(buffer.data(), static_cast<std::size_t>(in.gcount())));
    }
    if (in.bad()) return std::nullopt;
    return sq::cart::to_hex(hash.finish());
}

}  // namespace detail

/// Who a path belongs to: its class and origin from the provenance table,
/// and whether it changed since it was written. A path the table does not
/// list is the developer's (class user, no origin).
inline Answer owner(const Workspace& ws, std::string_view path, const std::filesystem::path& from) {
    const auto rel = detail::workspace_path(ws, path, from);
    if (!rel) {
        return std::unexpected(Failure{"workspace.path.outside", std::format("{} is not inside the workspace", path),
                                       JsonValue(JsonValue::Object{{"workspace", JsonValue(ws.root.string())}})});
    }
    const JsonValue* entry = ws.files.find(*rel);
    if (entry == nullptr) {
        return JsonValue(JsonValue::Object{{"path", JsonValue(*rel)},
                                           {"class", JsonValue("user")},
                                           {"origin", JsonValue(nullptr)},
                                           {"exists", JsonValue(std::filesystem::exists(ws.root / *rel))}});
    }
    const std::string recorded = detail::text_of(entry->find("sha256"));
    const auto now = detail::file_sha256(ws.root / *rel);
    return JsonValue(JsonValue::Object{
        {"path", JsonValue(*rel)},
        {"class", JsonValue(detail::text_of(entry->find("class")))},
        {"origin", JsonValue(detail::text_of(entry->find("origin")))},
        {"exists", JsonValue(now.has_value())},
        {"changed", JsonValue(!now || *now != recorded)},
    });
}

/// Every path the provenance table lists that changed or went missing since
/// it was written, and which of them an update would overwrite (generated
/// ones; seeded and user files are the developer's and never touched).
inline Answer verify(const Workspace& ws) {
    JsonValue::Array findings;
    std::int64_t checked = 0;
    if (const JsonValue::Object* files = ws.files.object_if()) {
        for (const auto& [path, entry] : *files) {
            ++checked;
            const std::string cls = detail::text_of(entry.find("class"));
            const auto now = detail::file_sha256(ws.root / path);
            const char* state = !now ? "missing" : *now != detail::text_of(entry.find("sha256")) ? "changed" : nullptr;
            if (state == nullptr) continue;
            findings.emplace_back(JsonValue::Object{
                {"path", JsonValue(path)},
                {"state", JsonValue(state)},
                {"class", JsonValue(cls)},
                {"origin", JsonValue(detail::text_of(entry.find("origin")))},
                {"update_overwrites", JsonValue(cls == "generated")},
            });
        }
    }
    return JsonValue(JsonValue::Object{{"workspace", JsonValue(ws.root.string())},
                                       {"checked", JsonValue(checked)},
                                       {"findings", JsonValue(std::move(findings))}});
}

}  // namespace square
