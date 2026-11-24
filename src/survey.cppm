// SPDX-License-Identifier: MIT
//
// square.survey: the workspaces under a folder, and what one workspace's roles
// hold (docs/spec.md §3; sqIDE's Workspaces and Home screens, and its code
// editor's file tree, ask these).
//
//   find DIR     every workspace under DIR: exploded (a folder holding
//                SQ-INF/manifest.json of kind workspace) or compressed (a .sq
//                whose manifest is of kind workspace); newest first
//   roles        each of the workspace's roles: its path, whether it is
//                there, and the files under it, each with its class from the
//                provenance table (a path it does not list is the
//                developer's: user)
//
// Read only, as phase 1 is: nothing here writes.

export module square.survey;

import std;
import sq.data;
import sq.cart;
import square.workspace;

export namespace square {

/// How far below DIR `find` looks: a workspace folder deeper than this is
/// not found (and a whole home is not walked).
inline constexpr int find_depth = 4;

/// How many files `roles` lists under one role, at most (`truncated` says so).
inline constexpr std::size_t role_files = 2000;

namespace detail {

/// A folder find does not look in: hidden ones (.git, .cache), and what
/// builds make (build/), which never hold a workspace of their own.
inline bool skipped(const std::filesystem::path& dir) {
    const std::string name = dir.filename().string();
    return name.empty() || name.front() == '.' || name == "build" || name == "node_modules";
}

/// Seconds since 1970 of `path`'s last change, or 0.
inline std::int64_t modified_seconds(const std::filesystem::path& path) {
    std::error_code ec;
    const auto when = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    // libc++ has no clock_cast: the two clocks' offset, now, is near enough
    // for ordering cards and saying "an hour ago".
    const auto system = std::chrono::system_clock::now() +
                        std::chrono::duration_cast<std::chrono::system_clock::duration>(
                            when - std::filesystem::file_time_type::clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
}

/// What a card shows: one workspace, found.
inline JsonValue card(const std::filesystem::path& path, std::string_view form, const JsonValue& manifest,
                      std::int64_t modified) {
    const JsonValue* square = square_section(manifest);
    const JsonValue* made = square != nullptr ? square->find("made_from") : nullptr;
    return JsonValue(JsonValue::Object{
        {"path", JsonValue(path.string())},
        {"form", JsonValue(form)},
        {"id", JsonValue(text_of(manifest.find("id")))},
        {"title", JsonValue(text_of(manifest.find("title")))},
        {"version", JsonValue(text_of(manifest.find("version")))},
        {"template", JsonValue(made != nullptr ? text_of(made->find("template")) : std::string())},
        {"modified", JsonValue(modified)},
    });
}

}  // namespace detail

/// Every workspace under `dir`, exploded or compressed, newest first; the
/// .sq files that could not be read, with why.
inline Answer find(const std::filesystem::path& dir) {
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::weakly_canonical(dir, ec);
    if (ec || !std::filesystem::is_directory(root, ec)) {
        return std::unexpected(Failure{"find.not_a_folder", std::format("{} is not a folder", dir.string()),
                                       JsonValue(JsonValue::Object{{"path", JsonValue(dir.string())}})});
    }
    struct Found {
        std::int64_t modified;
        JsonValue card;
    };
    std::vector<Found> found;
    JsonValue::Array problems;
    const auto look_at = [&](const std::filesystem::path& dir_here) {
        if (const auto manifest = detail::workspace_manifest(dir_here)) {
            const std::int64_t when = detail::modified_seconds(dir_here / "SQ-INF" / "manifest.json");
            found.push_back({when, detail::card(dir_here, "exploded", sq::data::parse_json(manifest->json).value, when)});
            return true;
        }
        return false;
    };
    if (look_at(root)) {
        // DIR is a workspace itself: it, and nothing inside it.
    } else {
        auto it = std::filesystem::recursive_directory_iterator(
            root, std::filesystem::directory_options::skip_permission_denied, ec);
        for (const auto end = std::filesystem::recursive_directory_iterator(); !ec && it != end; it.increment(ec)) {
            const std::filesystem::directory_entry& entry = *it;
            std::error_code type_ec;
            if (entry.is_symlink(type_ec)) {
                if (entry.is_directory(type_ec)) it.disable_recursion_pending();
                continue;  // followed nowhere: a link loop, or another tree
            }
            if (entry.is_directory(type_ec)) {
                if (detail::skipped(entry.path()) || it.depth() + 1 >= find_depth || look_at(entry.path())) {
                    it.disable_recursion_pending();  // a workspace's inside is the workspace's
                }
                continue;
            }
            if (entry.path().extension() != ".sq" || !entry.is_regular_file(type_ec)) continue;
            auto archive = sq::cart::Archive::open(entry.path());
            if (!archive) {
                problems.emplace_back(JsonValue::Object{{"path", JsonValue(entry.path().string())},
                                                        {"message", JsonValue(archive.error().message)}});
                continue;
            }
            if (archive->manifest().kind != "workspace") continue;  // a template, a kit, a package: not a workspace
            const std::int64_t when = detail::modified_seconds(entry.path());
            found.push_back({when, detail::card(entry.path(), "compressed",
                                                sq::data::parse_json(archive->manifest().json).value, when)});
        }
    }
    std::ranges::stable_sort(found, [](const Found& a, const Found& b) { return a.modified > b.modified; });
    JsonValue::Array cards;
    for (Found& f : found) cards.push_back(std::move(f.card));
    return JsonValue(JsonValue::Object{{"folder", JsonValue(root.string())},
                                       {"workspaces", JsonValue(std::move(cards))},
                                       {"problems", JsonValue(std::move(problems))}});
}

/// Each of the workspace's roles: its path, whether it is there (a folder or
/// a file), and the files under it with their class, sorted by path.
inline Answer roles(const Workspace& ws) {
    const JsonValue* square = detail::square_section(ws.manifest);
    const JsonValue* named = square != nullptr ? square->find("roles") : nullptr;
    JsonValue::Object out;
    if (named == nullptr || named->object_if() == nullptr) {
        return JsonValue(JsonValue::Object{{"workspace", JsonValue(ws.root.string())}, {"roles", JsonValue(std::move(out))}});
    }
    const auto class_of = [&](const std::string& rel) {
        const JsonValue* entry = ws.files.find(rel);
        return entry != nullptr ? detail::text_of(entry->find("class")) : std::string("user");
    };
    for (const auto& [role, path_value] : *named->object_if()) {
        const std::string rel = detail::text_of(&path_value);
        const std::filesystem::path at = ws.root / rel;
        std::error_code ec;
        const bool is_dir = std::filesystem::is_directory(at, ec);
        const bool is_file = !is_dir && std::filesystem::is_regular_file(at, ec);
        std::vector<std::string> paths;
        bool truncated = false;
        if (is_file) {
            paths.push_back(std::filesystem::path(rel).generic_string());
        } else if (is_dir) {
            auto it = std::filesystem::recursive_directory_iterator(
                at, std::filesystem::directory_options::skip_permission_denied, ec);
            for (const auto end = std::filesystem::recursive_directory_iterator(); !ec && it != end; it.increment(ec)) {
                std::error_code type_ec;
                if (it->is_directory(type_ec) && !it->is_symlink(type_ec)) {
                    if (detail::skipped(it->path())) it.disable_recursion_pending();
                    continue;
                }
                if (paths.size() == role_files) {
                    truncated = true;
                    break;
                }
                paths.push_back(it->path().lexically_relative(ws.root).generic_string());
            }
            std::ranges::sort(paths);
        }
        JsonValue::Array files;
        for (const std::string& p : paths) {
            files.emplace_back(JsonValue::Object{{"path", JsonValue(p)}, {"class", JsonValue(class_of(p))}});
        }
        out.emplace(role, JsonValue::Object{
                              {"path", JsonValue(rel)},
                              {"exists", JsonValue(is_dir || is_file)},
                              {"form", JsonValue(is_dir ? "folder" : is_file ? "file" : "missing")},
                              {"files", JsonValue(std::move(files))},
                              {"truncated", JsonValue(truncated)},
                          });
    }
    return JsonValue(JsonValue::Object{{"workspace", JsonValue(ws.root.string())}, {"roles", JsonValue(std::move(out))}});
}

}  // namespace square
