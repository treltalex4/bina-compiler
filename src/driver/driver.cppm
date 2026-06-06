export module bina.driver;

import std;

export namespace Driver {

struct BuildOptions {
    std::string ll_text;
    std::string source_filename;
    std::string output_path;
    std::string runtime_obj;
    bool keep_intermediates = false;
    bool no_link = false;
};

std::expected<void, std::string> buildExecutable(const BuildOptions& opts);
std::string findRuntimeObject(const char* argv0);

}  // namespace Driver
