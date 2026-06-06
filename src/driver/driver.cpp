module bina.driver;

import std;

namespace Driver {
namespace {

std::string shellQuote(std::string_view text) {
    std::string result = "'";
    for (char c : text) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

std::string tempPath(std::string_view stem, std::string_view ext) {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto pid =
        static_cast<unsigned long long>(std::hash<std::thread::id>{}(
            std::this_thread::get_id()));
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "bina_" + std::string(stem) + "_" + std::to_string(now) + "_" +
            std::to_string(pid) + std::string(ext);
    return path.string();
}

bool commandExists(const std::string& name) {
    const std::string cmd =
        "command -v " + shellQuote(name) + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::string findLlc() {
    if (const char* env = std::getenv("BINA_LLC")) return env;
    if (commandExists("llc-22")) return "llc-22";
    if (commandExists("llc")) return "llc";
    return "llc";
}

std::string findCc() {
    if (const char* env = std::getenv("BINA_CC")) return env;
    if (commandExists("cc")) return "cc";
    if (commandExists("clang")) return "clang";
    return "cc";
}

int runCommand(const std::string& cmd) { return std::system(cmd.c_str()); }

}  // namespace

std::string findRuntimeObject(const char* argv0) {
    std::error_code ec;
    const auto self = std::filesystem::weakly_canonical(argv0, ec);
    if (!ec) {
        const auto candidate = self.parent_path() / "bina_runtime.o";
        if (std::filesystem::exists(candidate)) return candidate.string();
    }

    if (const char* env = std::getenv("BINA_RUNTIME")) return env;
    return "bina_runtime.o";
}

std::expected<void, std::string> buildExecutable(const BuildOptions& opts) {
    const std::string stem =
        std::filesystem::path(opts.source_filename).stem().string();
    const std::string ll_path = tempPath(stem, ".ll");
    const std::string obj_path = tempPath(stem, ".o");

    {
        std::ofstream out(ll_path);
        if (!out) return std::unexpected("cannot write " + ll_path);
        out << opts.ll_text;
    }

    const std::string llc = findLlc();
    const std::string llc_cmd =
        llc + " -relocation-model=pic -filetype=obj " + shellQuote(ll_path) +
        " -o " + shellQuote(obj_path);
    if (runCommand(llc_cmd) != 0) {
        return std::unexpected("llc failed: " + llc_cmd);
    }

    if (opts.no_link) {
        std::string obj_out = opts.output_path;
        if (!obj_out.ends_with(".o")) obj_out += ".o";

        std::error_code ec;
        std::filesystem::copy_file(
            obj_path, obj_out,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) return std::unexpected("cannot write " + obj_out + ": " +
                                      ec.message());

        if (!opts.keep_intermediates) std::filesystem::remove(ll_path, ec);
        std::filesystem::remove(obj_path, ec);
        return {};
    }

    if (!std::filesystem::exists(opts.runtime_obj)) {
        return std::unexpected("runtime object not found: " + opts.runtime_obj);
    }

    const std::string cc = findCc();
    const std::string link_cmd = cc + " " + shellQuote(obj_path) + " " +
                                 shellQuote(opts.runtime_obj) + " -o " +
                                 shellQuote(opts.output_path);
    if (runCommand(link_cmd) != 0) {
        return std::unexpected("link failed: " + link_cmd);
    }

    if (!opts.keep_intermediates) {
        std::error_code ec;
        std::filesystem::remove(ll_path, ec);
        std::filesystem::remove(obj_path, ec);
    }

    return {};
}

}  // namespace Driver
