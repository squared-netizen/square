// SPDX-License-Identifier: MIT
//
// square.cartridge: a workspace into one .sq file, and one out again
// (docs/spec.md §3), through the Drive (sq.drive: sqdrive's pack and
// explode), never through sqcart directly. square adds what is a
// workspace's alone: only a workspace is packed or exploded, what its build
// makes is left out, and an exploded one lands in a folder of its own.
//
//   pack       the workspace into a new .sq: the folder's files as sqcart's
//              exploded form has them (SQ-INF/entries.json, names in NFC, links
//              refused, dot folders left out: .git/, .squared/), less build/
//              and the build role's folder; no file is ever held whole
//   explode    a .sq of kind workspace written out as a folder, every byte
//              verified, into DIR/<name> (the last part of its id)

export module square.cartridge;

import std;
import sq.data;
import sq.cart;
import sq.drive;
import square.workspace;

export namespace square {

/// The workspace packed into `out` (empty: beside the workspace, as
/// ID-VERSION.sq). Refuses a file that is there already.
inline Answer pack(const Workspace& ws, std::optional<std::filesystem::path> out) {
    const std::string id = detail::text_of(ws.manifest.find("id"));
    const std::string version = detail::text_of(ws.manifest.find("version"));
    const std::filesystem::path target =
        out ? *out : ws.root.parent_path() / std::format("{}-{}.sq", id, version.empty() ? "0" : version);
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::symlink_status(target, ec))) {
        return std::unexpected(Failure{"pack.exists", std::format("{} is there already", target.string()),
                                       JsonValue(JsonValue::Object{{"path", JsonValue(target.string())}})});
    }
    // What the build makes: build/, and the build role's folder if it names another.
    std::vector<std::string> leave_out{"build"};
    const JsonValue* square = detail::square_section(ws.manifest);
    const JsonValue* roles = square != nullptr ? square->find("roles") : nullptr;
    if (const std::string build = detail::text_of(roles != nullptr ? roles->find("build") : nullptr);
        !build.empty() && build != "build" && std::filesystem::is_directory(ws.root / build, ec)) {
        leave_out.push_back(build);
    }
    auto commit = sq::drive::pack(ws.root, target, {.leave_out = leave_out});
    if (!commit) {
        return std::unexpected(Failure{"pack.failed", commit.error().message,
                                       JsonValue(JsonValue::Object{
                                           {"entry", JsonValue(commit.error().entry.value_or(std::string()))}})});
    }
    JsonValue::Array left;
    for (const std::string& l : leave_out) left.emplace_back(l);
    return JsonValue(JsonValue::Object{
        {"workspace", JsonValue(ws.root.string())},
        {"cartridge", JsonValue(target.string())},
        {"left_out", JsonValue(std::move(left))},
        {"bytes", JsonValue(static_cast<std::int64_t>(std::filesystem::file_size(target, ec)))},
    });
}

/// A .sq of kind workspace exploded into `into`/<name>, <name> the last part
/// of its id (dev.me.demo: demo). Refuses a cartridge that is not a
/// workspace, and a folder that is there already.
inline Answer explode(const std::filesystem::path& cartridge, const std::filesystem::path& into) {
    auto archive = sq::cart::Archive::open(cartridge);
    if (!archive) {
        return std::unexpected(Failure{"explode.unreadable", archive.error().message,
                                       JsonValue(JsonValue::Object{{"path", JsonValue(cartridge.string())}})});
    }
    if (archive->manifest().kind != "workspace") {
        return std::unexpected(Failure{"explode.not_a_workspace",
                                       std::format("{} is a {}, not a workspace", cartridge.string(), archive->manifest().kind),
                                       JsonValue(JsonValue::Object{{"kind", JsonValue(archive->manifest().kind)}})});
    }
    const std::string id = archive->manifest().id;
    const std::size_t dot = id.rfind('.');
    const std::string name = dot == std::string::npos ? id : id.substr(dot + 1);
    const std::filesystem::path target = into / name;
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::symlink_status(target, ec))) {
        return std::unexpected(Failure{"explode.exists", std::format("{} is there already", target.string()),
                                       JsonValue(JsonValue::Object{{"path", JsonValue(target.string())}})});
    }
    auto files = sq::drive::explode(cartridge, target);
    if (!files) {
        return std::unexpected(Failure{"explode.failed", files.error().message,
                                       JsonValue(JsonValue::Object{
                                           {"entry", JsonValue(files.error().entry.value_or(std::string()))}})});
    }
    return JsonValue(JsonValue::Object{
        {"cartridge", JsonValue(cartridge.string())},
        {"workspace", JsonValue(std::filesystem::weakly_canonical(target, ec).string())},
        {"files", JsonValue(static_cast<std::int64_t>(*files))},
    });
}

}  // namespace square
