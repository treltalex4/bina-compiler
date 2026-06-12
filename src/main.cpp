import std;
import bina.lexer;
import bina.lexer.token;
import bina.parser;
import bina.parser.ast;
import bina.semantic;
import bina.codegen;
import bina.driver;

int main(int argc, char* argv[]) {
    bool dump_tokens = false;
    bool dump_ast = false;
    bool dump_symbols = false;
    bool emit_ir = false;
    bool no_link = false;
    std::string filename;
    std::string output;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-tokens")
            dump_tokens = true;
        else if (arg == "--dump-ast")
            dump_ast = true;
        else if (arg == "--dump-symbols")
            dump_symbols = true;
        else if (arg == "--emit-ir")
            emit_ir = true;
        else if (arg == "--no-link")
            no_link = true;
        else if (arg == "-o" && i + 1 < argc)
            output = argv[++i];
        else
            filename = arg;
    }

    if (filename.empty()) {
        std::cerr << "Usage: bina [--dump-tokens] [--dump-ast] "
                     "[--dump-symbols] [--emit-ir] [--no-link] "
                     "[-o <output>] <source.bina>\n";
        return 1;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << filename << "'\n";
        return 1;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    Lexer::Lexer lexer(source, filename);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        std::cerr << tokens.error() << '\n';
        return 1;
    }

    if (dump_tokens) {
        for (const auto& tok : tokens.value()) {
            std::cout << tok.line << ":" << tok.col << "  "
                      << tokenTypeToString(tok.type);
            if (!tok.lexeme.empty()) std::cout << " \"" << tok.lexeme << "\"";
            std::cout << '\n';
        }
        return 0;
    }

    Parser::Parser parser(tokens.value(), filename);
    auto ast = parser.parse();
    if (!ast) {
        for (const auto& err : ast.error()) std::cerr << err << '\n';
        return 1;
    }

    if (dump_ast) {
        Parser::printAst(*ast, std::cout);
        return 0;
    }

    Semantic::Semantic sema(*ast, filename);
    auto typed = sema.analyze();
    if (!typed) {
        for (const auto& err : typed.error()) std::cerr << err << '\n';
        return 1;
    }

    if (dump_symbols) {
        typed->global_scope->dumpSymbols(std::cout);
        return 0;
    }

    Codegen::Codegen codegen(*typed, filename);
    auto ll = codegen.emit();
    if (!ll) {
        for (const auto& err : ll.error()) std::cerr << err << '\n';
        return 1;
    }

    if (emit_ir) {
        std::cout << *ll;
        return 0;
    }

    if (output.empty()) {
        output = std::filesystem::path(filename).stem().string() + ".out";
    }

    Driver::BuildOptions build_options{
        .ll_text = *ll,
        .source_filename = filename,
        .output_path = output,
        .runtime_obj = Driver::findRuntimeObject(argv[0]),
        .no_link = no_link,
    };

    auto built = Driver::buildExecutable(build_options);
    if (!built) {
        std::cerr << "error: " << built.error() << '\n';
        return 1;
    }

    return 0;
}
