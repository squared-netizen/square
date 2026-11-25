// SPDX-License-Identifier: MIT
//
// square.resources: the cartridges a workspace can be made from or grow
// by: templates, kits, packages and assets (docs/spec.md §3, phase 1).
//
// They are found on a search path, each folder of which holds cartridges
// itself or in templates/, kits/, packages/ and assets/ (as squared-pg
// installs its resources). A cartridge is a folder holding
// SQ-INF/manifest.json (exploded) or a .sq file (packed); sq.cart opens
// both. The search path is, in order:
//
//   args.search / --resources PATH[:PATH...]
//   $SQUARE_RESOURCES         PATH[:PATH...]
//   ${XDG_DATA_HOME:-~/.local/share}/squared-pg/resources   (your own: what
//                             `sqmake install` puts there from a workspace
//                             made with template.kit, .package or .template;
//                             sqpg searches it too)
//   $PREFIX/share/squared-pg/resources     (sqterm's prefix)
//   /usr/local/share/squared-pg/resources
//
// A cartridge sq.cart refuses is a finding, not a failure: the rest are
// still listed.

export module square.resources;

import std;
import sq.data;
import sq.cart;
import square.workspace;

export namespace square {

/// The folders searched for cartridges, those that exist, first to last.
inline std::vector<std::filesystem::path> resource_path(const std::optional<std::string>& given) {
    std::vector<std::string> words;
    const auto add_list = [&](std::string_view list) {
        for (const auto part : std::views::split(list, ':')) {
            if (!part.empty()) words.emplace_back(part.begin(), part.end());
        }
    };
    if (given) {
        add_list(*given);
    } else if (const char* env = std::getenv("SQUARE_RESOURCES"); env != nullptr && *env != '\0') {
        add_list(env);
    } else {
        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && *xdg != '\0') {
            words.push_back(std::string(xdg) + "/squared-pg/resources");
        } else if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
            words.push_back(std::string(home) + "/.local/share/squared-pg/resources");
        }
        if (const char* prefix = std::getenv("PREFIX"); prefix != nullptr && *prefix != '\0') {
            words.push_back(std::string(prefix) + "/share/squared-pg/resources");
        }
        words.emplace_back("/usr/local/share/squared-pg/resources");
    }
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (const auto& w : words) {
        auto p = std::filesystem::weakly_canonical(w, ec);
        if (ec || !std::filesystem::is_directory(p, ec)) continue;
        if (std::ranges::find(out, p) == out.end()) out.push_back(std::move(p));
    }
    return out;
}

namespace detail {

/// One cartridge, opened: its manifest (as JSON), where it is, how many files.
struct Cartridge {
    std::filesystem::path path;
    JsonValue manifest;
    std::size_t files = 0;
};

template <class C>
inline Cartridge cartridge_of(const std::filesystem::path& path, const C& cart, std::size_t files) {
    Cartridge c;
    c.path = path;
    c.manifest = sq::data::parse_json(cart.manifest().json).value;
    c.files = files;
    return c;
}

/// `path` as a cartridge, or why sq.cart refused it.
inline std::expected<Cartridge, std::string> open_cartridge(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        auto folder = sq::cart::Folder::open(path);
        if (!folder) return std::unexpected(folder.error().message);
        return cartridge_of(path, *folder, folder->entries().size());
    }
    auto archive = sq::cart::Archive::open(path);
    if (!archive) return std::unexpected(archive.error().message);
    auto entries = archive->entries();
    if (!entries) return std::unexpected(entries.error().message);
    return cartridge_of(path, *archive, entries->size());
}

/// Whether `path` looks like a cartridge: a folder with a manifest, or a .sq file.
inline bool looks_like_cartridge(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) return std::filesystem::exists(path / "SQ-INF" / "manifest.json", ec);
    return path.extension() == ".sq" && std::filesystem::is_regular_file(path, ec);
}

/// Every cartridge on the search path, in order, and the ones refused.
inline void scan(const std::vector<std::filesystem::path>& roots, std::vector<Cartridge>& found,
                 JsonValue::Array& problems) {
    std::error_code ec;
    for (const auto& root : roots) {
        std::vector<std::filesystem::path> places{root};
        for (const char* sub : {"templates", "kits", "packages", "assets"}) {
            if (std::filesystem::is_directory(root / sub, ec)) places.push_back(root / sub);
        }
        for (const auto& place : places) {
            std::vector<std::filesystem::path> here;
            for (const auto& e : std::filesystem::directory_iterator(place, ec)) {
                if (looks_like_cartridge(e.path())) here.push_back(e.path());
            }
            std::ranges::sort(here);
            for (const auto& p : here) {
                auto c = open_cartridge(p);
                if (c) {
                    found.push_back(std::move(*c));
                } else {
                    problems.emplace_back(
                        JsonValue::Object{{"path", JsonValue(p.string())}, {"message", JsonValue(c.error())}});
                }
            }
        }
    }
}

inline std::string text_at(const JsonValue& v, std::string_view key) {
    const JsonValue* x = v.find(key);
    const std::string* s = x != nullptr ? x->string_if() : nullptr;
    return s != nullptr ? *s : std::string();
}

inline JsonValue summary(const Cartridge& c) {
    return JsonValue(JsonValue::Object{
        {"id", JsonValue(text_at(c.manifest, "id"))},
        {"version", JsonValue(text_at(c.manifest, "version"))},
        {"kind", JsonValue(text_at(c.manifest, "kind"))},
        {"title", JsonValue(text_at(c.manifest, "title"))},
        {"path", JsonValue(c.path.string())},
    });
}

inline JsonValue path_array(const std::vector<std::filesystem::path>& roots) {
    JsonValue::Array a;
    for (const auto& r : roots) a.emplace_back(r.string());
    return JsonValue(std::move(a));
}

}  // namespace detail

/// The cartridges on the search path, of `kind` when one is given
/// (template, kit, package, asset...), and those sq.cart refused.
inline Answer resources(const std::optional<std::string>& path, const std::optional<std::string>& kind) {
    const auto roots = resource_path(path);
    std::vector<detail::Cartridge> found;
    JsonValue::Array problems;
    detail::scan(roots, found, problems);
    JsonValue::Array list;
    for (const auto& c : found) {
        if (kind && detail::text_at(c.manifest, "kind") != *kind) continue;
        list.push_back(detail::summary(c));
    }
    return JsonValue(JsonValue::Object{{"search", detail::path_array(roots)},
                                       {"resources", JsonValue(std::move(list))},
                                       {"problems", JsonValue(std::move(problems))}});
}

/// One cartridge, by id (or id@version): what it is, what it needs, its
/// manifest whole. The first on the search path when several match.
inline Answer show(const std::optional<std::string>& path, std::string_view wanted) {
    const auto at = wanted.find('@');
    const std::string_view id = wanted.substr(0, at);
    const std::optional<std::string_view> version =
        at == std::string_view::npos ? std::nullopt : std::optional(wanted.substr(at + 1));
    const auto roots = resource_path(path);
    std::vector<detail::Cartridge> found;
    JsonValue::Array problems;
    detail::scan(roots, found, problems);
    for (const auto& c : found) {
        if (detail::text_at(c.manifest, "id") != id) continue;
        if (version && detail::text_at(c.manifest, "version") != *version) continue;
        JsonValue out = detail::summary(c);
        JsonValue::Object& o = *out.object_if();
        o.emplace("description", JsonValue(detail::text_at(c.manifest, "description")));
        for (const char* key : {"requires", "requires_features"}) {
            const JsonValue* r = c.manifest.find(key);
            o.emplace(key, r != nullptr ? *r : JsonValue(nullptr));
        }
        o.emplace("files", JsonValue(static_cast<std::int64_t>(c.files)));
        o.emplace("manifest", c.manifest);
        return out;
    }
    return std::unexpected(Failure{"resource.not_found", std::format("no resource {} on the search path", wanted),
                                   JsonValue(JsonValue::Object{{"search", detail::path_array(roots)}})});
}

}  // namespace square
