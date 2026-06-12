module bina.codegen;

import std;
import bina.lexer.token;

namespace Codegen {
namespace {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

std::string joinName(const std::vector<std::string>& parts) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "::";
        result += parts[i];
    }
    return result;
}

std::vector<std::string> appendName(std::vector<std::string> parts,
                                    const std::string& name) {
    parts.push_back(name);
    return parts;
}

std::vector<std::string> splitQualifiedName(const std::string& name) {
    std::vector<std::string> parts;
    std::size_t start = 0;

    while (start < name.size()) {
        const std::size_t sep = name.find("::", start);
        if (sep == std::string::npos) {
            parts.push_back(name.substr(start));
            break;
        }
        parts.push_back(name.substr(start, sep - start));
        start = sep + 2;
    }

    return parts;
}

[[noreturn]] void internalError(const std::string& message) {
    throw std::runtime_error("codegen internal error: " + message);
}

int digitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

struct ParsedIntLiteral {
    std::string digits;
    int base;
    std::string suffix;
};

ParsedIntLiteral parseIntLiteralForCodegen(const std::string& value) {
    int base = 10;
    std::size_t pos = 0;

    if (value.size() >= 2 && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        pos = 2;
    } else if (value.size() >= 2 && value[0] == '0' &&
               (value[1] == 'b' || value[1] == 'B')) {
        base = 2;
        pos = 2;
    }

    const std::size_t digits_begin = pos;
    while (pos < value.size()) {
        const int d = digitValue(value[pos]);
        if (d < 0 || d >= base) break;
        ++pos;
    }

    return ParsedIntLiteral{
        .digits = value.substr(digits_begin, pos - digits_begin),
        .base = base,
        .suffix = value.substr(pos),
    };
}

std::string trimLeadingZeros(std::string value) {
    const std::size_t first = value.find_first_not_of('0');
    if (first == std::string::npos) return "0";
    return value.substr(first);
}

std::string multiplyDecimalString(std::string value, int multiplier) {
    value = trimLeadingZeros(std::move(value));

    int carry = 0;
    std::string result;
    result.reserve(value.size() + 1);
    for (auto it = value.rbegin(); it != value.rend(); ++it) {
        const int product = (*it - '0') * multiplier + carry;
        result.push_back(static_cast<char>('0' + (product % 10)));
        carry = product / 10;
    }
    while (carry > 0) {
        result.push_back(static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }
    std::reverse(result.begin(), result.end());
    return trimLeadingZeros(std::move(result));
}

std::string addToDecimalString(std::string value, int addend) {
    value = trimLeadingZeros(std::move(value));

    int carry = addend;
    for (auto it = value.rbegin(); it != value.rend() && carry > 0; ++it) {
        const int sum = (*it - '0') + carry;
        *it = static_cast<char>('0' + (sum % 10));
        carry = sum / 10;
    }
    while (carry > 0) {
        value.insert(value.begin(), static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }
    return trimLeadingZeros(std::move(value));
}

std::string literalDigitsToDecimal(std::string_view digits, int base) {
    std::string result = "0";
    for (char c : digits) {
        result = multiplyDecimalString(std::move(result), base);
        result = addToDecimalString(std::move(result), digitValue(c));
    }
    return trimLeadingZeros(std::move(result));
}

std::string intLiteralToDecimal(const std::string& value) {
    const ParsedIntLiteral parsed = parseIntLiteralForCodegen(value);
    return literalDigitsToDecimal(parsed.digits, parsed.base);
}

std::string stripFloatSuffix(std::string value) {
    if (value.ends_with("f32") || value.ends_with("f64")) {
        value.resize(value.size() - 3);
    }
    return value;
}

std::string floatConstantText(const std::string& literal,
                              const Semantic::Type& type) {
    std::string value = stripFloatSuffix(literal);
    if (value == "inf") return "0x7FF0000000000000";
    if (value == "nan" || value == "NaN") return "0x7FF8000000000000";

    char* end = nullptr;
    double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0') {
        internalError("invalid float literal reached codegen: " + literal);
    }

    if (type.kind == Semantic::TypeKind::FLOAT32) {
        parsed = static_cast<double>(static_cast<float>(parsed));
    }

    const std::uint64_t bits = std::bit_cast<std::uint64_t>(parsed);
    std::ostringstream hex;
    hex << "0x" << std::uppercase << std::hex << std::setw(16)
        << std::setfill('0') << bits;
    return hex.str();
}

std::string doubleHex(double value) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    std::ostringstream hex;
    hex << "0x" << std::uppercase << std::hex << std::setw(16)
        << std::setfill('0') << bits;
    return hex.str();
}

std::pair<std::string, std::string> integerBounds(Semantic::TypeKind kind) {
    using TK = Semantic::TypeKind;
    switch (kind) {
        case TK::INT8:
            return {"-128", "127"};
        case TK::INT16:
            return {"-32768", "32767"};
        case TK::INT32:
            return {"-2147483648", "2147483647"};
        case TK::INT64:
            return {"-9223372036854775808", "9223372036854775807"};
        case TK::UINT8:
            return {"0", "255"};
        case TK::UINT16:
            return {"0", "65535"};
        case TK::UINT32:
            return {"0", "4294967295"};
        case TK::UINT64:
            return {"0", "18446744073709551615"};
        default:
            internalError("integer bounds requested for non-integer type");
    }
}

std::string escapeLlvmBytes(std::string_view bytes) {
    std::ostringstream out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char b : bytes) {
        if (b == '\\' || b == '"' || b < 0x20 || b >= 0x7F) {
            out << '\\' << hex[b >> 4] << hex[b & 0x0F];
        } else {
            out << static_cast<char>(b);
        }
    }
    return out.str();
}

std::string escapeLlvmSourceString(std::string_view text) {
    std::ostringstream out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char b : text) {
        if (b == '\\' || b == '"' || b < 0x20 || b >= 0x7F) {
            out << "\\";
            out << hex[b >> 4] << hex[b & 0x0F];
        } else {
            out << static_cast<char>(b);
        }
    }
    return out.str();
}

}  // namespace

Codegen::Codegen(const Semantic::TypedProgram& typed,
                 std::string source_filename)
    : m_typed(typed),
      m_source_filename(std::move(source_filename)),
      m_current_return_type(Semantic::makePrimitive(Semantic::TypeKind::VOID)) {
}

std::expected<std::string, std::vector<std::string>> Codegen::emit() {
    try {
        emitModuleHeader();
        emitRuntimeDecls();
        emitStructTypes();
        emitFunctions();
        return m_header.str() + "\n" + m_body.str();
    } catch (const std::exception& e) {
        return std::unexpected(std::vector<std::string>{e.what()});
    }
}

void Codegen::emitModuleHeader() {
    m_header << "; ModuleID = '" << escapeLlvmSourceString(m_source_filename)
             << "'\n";
    m_header << "source_filename = \""
             << escapeLlvmSourceString(m_source_filename) << "\"\n";
    m_header << "target triple = \"x86_64-unknown-linux-gnu\"\n\n";
}

void Codegen::emitRuntimeDecls() {
    m_header << "declare void @bina_print_i64(i64)\n";
    m_header << "declare void @bina_print_u64(i64)\n";
    m_header << "declare void @bina_print_f64(double)\n";
    m_header << "declare void @bina_print_bool(i1)\n";
    m_header << "declare void @bina_print_char(i32)\n";
    m_header << "declare void @bina_print_str(ptr, i64)\n";
    m_header << "declare { ptr, i64 } @bina_input()\n";
    m_header << "declare void @bina_panic(ptr, i64, i64)\n";
    m_header << "declare void @bina_assert(i1, i64)\n";
    m_header << "declare void @bina_index_oob(i64, i64, i64)\n";
    m_header << "declare void @bina_div_zero(i64)\n";
    m_header << "declare void @bina_int_overflow(i64)\n";
    m_header << "declare i32 @bina_char_from(i64, i64)\n";
    m_header << "declare { ptr, i64 } @bina_to_string_i64(i64)\n";
    m_header << "declare { ptr, i64 } @bina_to_string_u64(i64)\n";
    m_header << "declare { ptr, i64 } @bina_to_string_f64(double)\n";
    m_header << "declare i64 @bina_parse_i64({ ptr, i64 }, i64)\n";
    m_header << "declare i64 @bina_parse_u64({ ptr, i64 }, i64)\n";
    m_header << "declare double @bina_parse_f64({ ptr, i64 }, i64)\n";
    m_header << "declare { ptr, i64 } @bina_str_concat({ ptr, i64 }, { ptr, "
                "i64 })\n";
    m_header << "declare i1 @bina_str_eq({ ptr, i64 }, { ptr, i64 })\n";
    m_header << "declare void @bina_exit(i64)\n\n";

    for (const std::string& ty : {"i8", "i16", "i32", "i64"}) {
        for (const std::string& op : {"add", "sub", "mul"}) {
            m_header << "declare { " << ty << ", i1 } @llvm.s" << op
                     << ".with.overflow." << ty << "(" << ty << ", " << ty
                     << ")\n";
            m_header << "declare { " << ty << ", i1 } @llvm.u" << op
                     << ".with.overflow." << ty << "(" << ty << ", " << ty
                     << ")\n";
        }
    }
    m_header << '\n';
}

void Codegen::emitStructTypes() {
    std::function<void(const std::vector<Parser::Decl>&,
                       std::vector<std::string>)>
        walk = [&](const std::vector<Parser::Decl>& declarations,
                   std::vector<std::string> ns) {
            for (const auto& decl : declarations) {
                std::visit(
                    overloaded{
                        [&](const std::unique_ptr<Parser::StructDecl>& st) {
                            const std::string qname =
                                joinName(appendName(ns, st->name));
                            if (!m_emitted_structs.insert(qname).second) {
                                return;
                            }

                            const Semantic::StructSymbol* sym =
                                findStructByQualifiedName(qname);
                            if (sym == nullptr) {
                                internalError("missing struct symbol " + qname);
                            }

                            m_header << "%struct." << mangleStruct(qname)
                                     << " = type { ";
                            for (std::size_t i = 0; i < sym->fields.size();
                                 ++i) {
                                if (i > 0) m_header << ", ";
                                m_header
                                    << llvmStoredType(sym->fields[i].type);
                            }
                            m_header << " }\n";
                        },
                        [&](const std::unique_ptr<Parser::NamespaceDecl>& n) {
                            walk(n->declarations, appendName(ns, n->name));
                        },
                        [&](const auto&) {},
                    },
                    decl.node);
            }
        };

    walk(m_typed.program->declarations, {});
    m_header << '\n';
}

void Codegen::emitFunctions() {
    std::function<void(const std::vector<Parser::Decl>&,
                       std::vector<std::string>)>
        walk = [&](const std::vector<Parser::Decl>& declarations,
                   std::vector<std::string> ns) {
            for (const auto& decl : declarations) {
                std::visit(
                    overloaded{
                        [&](const std::unique_ptr<Parser::FunctionDecl>& fn) {
                            const Semantic::FunctionSignature* sig =
                                findSignatureForFunctionDecl(ns, *fn);
                            if (sig == nullptr) {
                                internalError("missing function signature " +
                                              fn->name);
                            }
                            emitFunction(*fn, *sig);
                        },
                        [&](const std::unique_ptr<Parser::ImplDecl>& impl) {
                            for (const auto& method : impl->methods) {
                                const Semantic::FunctionSignature* sig =
                                    findSignatureForMethodDecl(ns, *impl,
                                                               *method);
                                if (sig == nullptr) {
                                    internalError("missing method signature " +
                                                  impl->struct_name +
                                                  "::" + method->name);
                                }
                                emitFunction(*method, *sig);
                            }
                        },
                        [&](const std::unique_ptr<Parser::NamespaceDecl>& n) {
                            walk(n->declarations, appendName(ns, n->name));
                        },
                        [&](const auto&) {},
                    },
                    decl.node);
            }
        };

    walk(m_typed.program->declarations, {});
}

void Codegen::emitFunction(const Parser::FunctionDecl& fn,
                           const Semantic::FunctionSignature& sig) {
    m_value_id = 0;
    m_label_id = 0;
    m_local_addrs.clear();
    m_env.clear();
    m_loop_stack.clear();
    m_block_terminated = false;
    m_current_return_type = sig.return_type;
    m_alloca_lines.clear();

    const std::string mangled = mangle(sig);
    m_in_main = (mangled == "@main");

    const std::string ret_llvm = m_in_main ? "i32" : llvmType(sig.return_type);

    std::ostringstream module_text = std::move(m_body);
    m_body = std::ostringstream{};
    m_current_label = "entry";

    envPush();
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
        const auto& param = fn.params[i];
        const Semantic::Type& type = sig.param_types[i];
        const std::string addr = emitEntryAlloca(param.name, type);
        storeValue(type, "%arg." + sanitizeIdent(sig.param_names[i]), addr);
        envDeclare(param.name, addr, type, &param);
    }

    genBlock(fn.body);

    if (!m_block_terminated) {
        if (sig.return_type.kind == Semantic::TypeKind::VOID) {
            emitTerminator("ret void");
        } else if (m_in_main) {
            emitTerminator("ret i32 0");
        } else {
            emitTerminator("unreachable");
        }
    }

    envPop();

    const std::string body_text = m_body.str();
    m_body = std::move(module_text);

    m_body << "define " << ret_llvm << " " << mangled << "(";
    for (std::size_t i = 0; i < sig.param_types.size(); ++i) {
        if (i > 0) m_body << ", ";
        m_body << llvmType(sig.param_types[i]) << " %arg."
               << sanitizeIdent(sig.param_names[i]);
    }
    m_body << ") {\n";
    m_body << "entry:\n";
    for (const std::string& line : m_alloca_lines) m_body << line;
    m_body << body_text;
    m_body << "}\n\n";
}

void Codegen::genStmt(const Parser::Stmt& s) {
    if (m_block_terminated) return;

    std::visit(
        overloaded{
            [&](const std::unique_ptr<Parser::LetStmt>& l) {
                genLet(*l, s.loc);
            },
            [&](const std::unique_ptr<Parser::AssignStmt>& a) {
                genAssign(*a, s.loc);
            },
            [&](const std::unique_ptr<Parser::IfStmt>& i) { genIf(*i, s.loc); },
            [&](const std::unique_ptr<Parser::WhileStmt>& w) {
                genWhile(*w, s.loc);
            },
            [&](const std::unique_ptr<Parser::ReturnStmt>& r) {
                genReturn(*r, s.loc);
            },
            [&](const std::unique_ptr<Parser::ExprStmt>& e) {
                genExprStmt(*e);
            },
            [&](const std::unique_ptr<Parser::Block>& b) { genBlock(*b); },
            [&](const Parser::BreakStmt&) { genBreak(); },
            [&](const Parser::ContinueStmt&) { genContinue(); },
            [&](const Parser::NullStmt&) {},
        },
        s.node);
}

void Codegen::genLet(const Parser::LetStmt& l, Parser::NodeLocation) {
    const Semantic::Type type = m_typed.decl_types.at(&l);

    // Инициализатор вычисляется до объявления имени
    Value init = genExpr(l.init);
    init = emitConversion(init, type, false, l.init.loc);

    const std::string addr = emitEntryAlloca(l.name, type);
    envDeclare(l.name, addr, type, &l);
    storeValue(type, init.ssa, addr);
}

void Codegen::genAssign(const Parser::AssignStmt& a, Parser::NodeLocation) {
    const std::string addr = genLvalueAddr(a.target);
    const Semantic::Type target_t = exprType(a.target);
    Value v = genExpr(a.value);
    v = emitConversion(v, target_t, false, a.value.loc);
    storeValue(target_t, v.ssa, addr);
}

void Codegen::genIf(const Parser::IfStmt& i, Parser::NodeLocation) {
    Value cond = genExpr(i.condition);
    const std::string then_label = freshLabel("if.then");
    const std::string else_label = i.else_branch ? freshLabel("if.else") : "";
    const std::string end_label = freshLabel("if.end");

    emitTerminator("br i1 " + cond.ssa + ", label %" + then_label +
                   ", label %" + (i.else_branch ? else_label : end_label));

    startLabel(then_label);
    genBlock(i.then_block);
    if (!m_block_terminated) {
        emitTerminator("br label %" + end_label);
    }

    if (i.else_branch) {
        startLabel(else_label);
        genStmt(**i.else_branch);
        if (!m_block_terminated) {
            emitTerminator("br label %" + end_label);
        }
    }

    startLabel(end_label);
}

void Codegen::genWhile(const Parser::WhileStmt& w, Parser::NodeLocation) {
    const std::string cond_label = freshLabel("while.cond");
    const std::string body_label = freshLabel("while.body");
    const std::string end_label = freshLabel("while.end");

    emitTerminator("br label %" + cond_label);

    startLabel(cond_label);
    Value cond = genExpr(w.condition);
    emitTerminator("br i1 " + cond.ssa + ", label %" + body_label +
                   ", label %" + end_label);

    startLabel(body_label);
    m_loop_stack.push_back({cond_label, end_label});
    genBlock(w.body);
    m_loop_stack.pop_back();
    if (!m_block_terminated) {
        emitTerminator("br label %" + cond_label);
    }

    startLabel(end_label);
}

void Codegen::genReturn(const Parser::ReturnStmt& r, Parser::NodeLocation) {
    if (!r.value) {
        emitTerminator("ret void");
        return;
    }

    Value v = genExpr(*r.value);
    v = emitConversion(v, m_current_return_type, false, r.value->loc);

    if (m_in_main) {
        std::string ret32;
        if (v.ssa == "0") {
            ret32 = "0";
        } else {
            ret32 = freshValue();
            m_body << "  " << ret32 << " = trunc i64 " << v.ssa << " to i32\n";
        }
        emitTerminator("ret i32 " + ret32);
        return;
    }

    emitTerminator("ret " + llvmType(m_current_return_type) + " " + v.ssa);
}

void Codegen::genBreak() {
    if (m_loop_stack.empty())
        internalError("break outside loop reached codegen");
    emitTerminator("br label %" + m_loop_stack.back().second);
}

void Codegen::genContinue() {
    if (m_loop_stack.empty()) {
        internalError("continue outside loop reached codegen");
    }
    emitTerminator("br label %" + m_loop_stack.back().first);
}

void Codegen::genBlock(const Parser::Block& b) {
    envPush();
    for (const auto& stmt : b.statements) {
        genStmt(stmt);
        if (m_block_terminated) break;
    }
    envPop();
}

void Codegen::genExprStmt(const Parser::ExprStmt& e) {
    (void)genExpr(e.expression);
}

Codegen::Value Codegen::genExpr(const Parser::Expr& e) {
    const Semantic::Type result_t = exprType(e);
    return std::visit(
        overloaded{
            [&](const Parser::IntLiteral& lit) {
                return genIntLit(lit, result_t);
            },
            [&](const Parser::FloatLiteral& lit) {
                return genFloatLit(lit, result_t);
            },
            [&](const Parser::CharLiteral& lit) { return genCharLit(lit); },
            [&](const Parser::StringLiteral& lit) { return genStringLit(lit); },
            [&](const Parser::BoolLiteral& lit) { return genBoolLit(lit); },
            [&](const Parser::Identifier& id) {
                return genIdentifier(id, result_t);
            },
            [&](const std::unique_ptr<Parser::BinaryExpr>& b) {
                return genBinary(*b, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::UnaryExpr>& u) {
                return genUnary(*u, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::CallExpr>& c) {
                return genCall(*c, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::MethodCall>& c) {
                return genMethodCall(*c, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::IndexExpr>& ix) {
                return genIndex(*ix, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::FieldAccess>& fa) {
                return genFieldAccess(*fa, result_t);
            },
            [&](const std::unique_ptr<Parser::CastExpr>& c) {
                return genCast(*c, result_t, e.loc);
            },
            [&](const std::unique_ptr<Parser::ArrayLiteral>& al) {
                return genArrayLit(e, *al, e.loc);
            },
            [&](const std::unique_ptr<Parser::StructLiteral>& sl) {
                return genStructLit(*sl, result_t);
            },
        },
        e.node);
}

Codegen::Value Codegen::genIntLit(const Parser::IntLiteral& l,
                                  const Semantic::Type& t) {
    const std::string value = intLiteralToDecimal(l.value);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = add " << llvmType(t) << " 0, " << value
           << "\n";
    return {ssa, t};
}

Codegen::Value Codegen::genFloatLit(const Parser::FloatLiteral& l,
                                    const Semantic::Type& t) {
    const std::string value = floatConstantText(l.value, t);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = fadd " << llvmType(t) << " 0.0, " << value
           << "\n";
    return {ssa, t};
}

Codegen::Value Codegen::genCharLit(const Parser::CharLiteral& l) {
    const Semantic::Type t = Semantic::makePrimitive(Semantic::TypeKind::CHAR);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = add i32 0, " << l.value << "\n";
    return {ssa, t};
}

Codegen::Value Codegen::genStringLit(const Parser::StringLiteral& l) {
    const std::string const_name = emitStringConstant(l.value);
    const std::size_t len = l.value.size();

    const std::string ptr = freshValue();
    const std::string with_ptr = freshValue();
    const std::string str = freshValue();
    m_body << "  " << ptr << " = getelementptr [" << len << " x i8], ptr "
           << const_name << ", i64 0, i64 0\n";
    m_body << "  " << with_ptr << " = insertvalue { ptr, i64 } undef, ptr "
           << ptr << ", 0\n";
    m_body << "  " << str << " = insertvalue { ptr, i64 } " << with_ptr
           << ", i64 " << len << ", 1\n";

    return {str, Semantic::makePrimitive(Semantic::TypeKind::STRING)};
}

Codegen::Value Codegen::genBoolLit(const Parser::BoolLiteral& l) {
    const Semantic::Type t = Semantic::makePrimitive(Semantic::TypeKind::BOOL);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = xor i1 " << (l.value ? "true" : "false")
           << ", false\n";
    return {ssa, t};
}

Codegen::Value Codegen::genIdentifier(const Parser::Identifier& id,
                                      const Semantic::Type& t) {
    if (id.parts.size() != 1) {
        internalError("qualified identifier used as value reached codegen");
    }

    const LocalSlot* slot = envLookup(id.parts[0]);
    if (slot == nullptr) {
        internalError("unknown local variable '" + id.parts[0] + "'");
    }

    return loadValueAtAddr(t, slot->addr);
}

Codegen::Value Codegen::genBinary(const Parser::BinaryExpr& b,
                                  const Semantic::Type& result_t,
                                  Parser::NodeLocation loc) {
    using TK = Semantic::TypeKind;
    using TT = TokenType;

    if (b.op == TT::AND_AND) return genShortCircuit(b, true);
    if (b.op == TT::OR_OR) return genShortCircuit(b, false);

    Value lhs = genExpr(b.left);
    Value rhs = genExpr(b.right);

    if (b.op == TT::PLUS && lhs.type.kind == TK::STRING) {
        const std::string ssa = freshValue();
        m_body << "  " << ssa
               << " = call { ptr, i64 } @bina_str_concat({ ptr, i64 } "
               << lhs.ssa << ", { ptr, i64 } " << rhs.ssa << ")\n";
        return {ssa, Semantic::makePrimitive(TK::STRING)};
    }

    if ((b.op == TT::EQUAL || b.op == TT::NOT_EQUAL) &&
        lhs.type.kind == TK::STRING) {
        const std::string eq = freshValue();
        m_body << "  " << eq << " = call i1 @bina_str_eq({ ptr, i64 } "
               << lhs.ssa << ", { ptr, i64 } " << rhs.ssa << ")\n";
        if (b.op == TT::NOT_EQUAL) {
            const std::string neq = freshValue();
            m_body << "  " << neq << " = xor i1 " << eq << ", true\n";
            return {neq, Semantic::makePrimitive(TK::BOOL)};
        }
        return {eq, Semantic::makePrimitive(TK::BOOL)};
    }

    if ((b.op == TT::EQUAL || b.op == TT::NOT_EQUAL) &&
        (lhs.type.kind == TK::ARRAY || lhs.type.kind == TK::STRUCT)) {
        const std::string lhs_addr = materializeAddr(lhs);
        const std::string rhs_addr = materializeAddr(rhs);
        Value eq = genCompareEq(lhs.type, lhs_addr, rhs_addr);
        if (b.op == TT::NOT_EQUAL) {
            const std::string neq = freshValue();
            m_body << "  " << neq << " = xor i1 " << eq.ssa << ", true\n";
            return {neq, Semantic::makePrimitive(TK::BOOL)};
        }
        return eq;
    }

    if (b.op == TT::PLUS || b.op == TT::MINUS || b.op == TT::STAR ||
        b.op == TT::SLASH || b.op == TT::PERCENT) {
        const Semantic::Type op_t = result_t;
        const std::string l = convertNumeric(lhs, op_t);
        const std::string r = convertNumeric(rhs, op_t);
        const std::string ty = llvmType(op_t);

        if (b.op == TT::PLUS) {
            if (Semantic::isFloat(op_t))
                return makeBinOp("fadd", ty, l, r, op_t);
            return emitCheckedAddSubMul("add", op_t, l, r, loc);
        }
        if (b.op == TT::MINUS) {
            if (Semantic::isFloat(op_t))
                return makeBinOp("fsub", ty, l, r, op_t);
            return emitCheckedAddSubMul("sub", op_t, l, r, loc);
        }
        if (b.op == TT::STAR) {
            if (Semantic::isFloat(op_t))
                return makeBinOp("fmul", ty, l, r, op_t);
            return emitCheckedAddSubMul("mul", op_t, l, r, loc);
        }
        if (b.op == TT::SLASH) {
            if (Semantic::isFloat(op_t))
                return makeBinOp("fdiv", ty, l, r, op_t);
            emitDivByZeroCheck(r, ty, loc);
            if (isSigned(op_t.kind))
                emitSignedDivOverflowCheck(l, r, op_t, loc);
            return makeBinOp(isSigned(op_t.kind) ? "sdiv" : "udiv", ty, l, r,
                             op_t);
        }
        if (b.op == TT::PERCENT) {
            emitDivByZeroCheck(r, ty, loc);
            if (isSigned(op_t.kind))
                emitSignedDivOverflowCheck(l, r, op_t, loc);
            return makeBinOp(isSigned(op_t.kind) ? "srem" : "urem", ty, l, r,
                             op_t);
        }
    }

    auto commonNumeric = [&]() {
        auto common = Semantic::getCommonType(lhs.type, rhs.type);
        if (!common) internalError("missing common numeric type");
        return *common;
    };

    auto emitIntCmp = [&](const Semantic::Type& op_t, const std::string& l,
                          const std::string& r, std::string_view signed_op,
                          std::string_view unsigned_op) -> Value {
        const std::string pred = isSigned(op_t.kind) ? std::string(signed_op)
                                                     : std::string(unsigned_op);
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = icmp " << pred << " " << llvmType(op_t)
               << " " << l << ", " << r << "\n";
        return {ssa, Semantic::makePrimitive(TK::BOOL)};
    };

    auto emitFloatCmp = [&](const Semantic::Type& op_t, const std::string& l,
                            const std::string& r,
                            std::string_view pred) -> Value {
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = fcmp " << pred << " " << llvmType(op_t)
               << " " << l << ", " << r << "\n";
        return {ssa, Semantic::makePrimitive(TK::BOOL)};
    };

    if (b.op == TT::EQUAL || b.op == TT::NOT_EQUAL) {
        Value eq;
        if (lhs.type.kind == TK::BOOL || lhs.type.kind == TK::CHAR) {
            const std::string ssa = freshValue();
            m_body << "  " << ssa << " = icmp eq " << llvmType(lhs.type) << " "
                   << lhs.ssa << ", " << rhs.ssa << "\n";
            eq = {ssa, Semantic::makePrimitive(TK::BOOL)};
        } else {
            const Semantic::Type op_t = commonNumeric();
            const std::string l = convertNumeric(lhs, op_t);
            const std::string r = convertNumeric(rhs, op_t);
            eq = Semantic::isFloat(op_t) ? emitFloatCmp(op_t, l, r, "oeq")
                                         : emitIntCmp(op_t, l, r, "eq", "eq");
        }

        if (b.op == TT::NOT_EQUAL) {
            const std::string neq = freshValue();
            m_body << "  " << neq << " = xor i1 " << eq.ssa << ", true\n";
            return {neq, Semantic::makePrimitive(TK::BOOL)};
        }
        return eq;
    }

    if (b.op == TT::LESS || b.op == TT::LESS_EQUAL || b.op == TT::GREATER ||
        b.op == TT::GREATER_EQUAL) {
        const Semantic::Type op_t = commonNumeric();
        const std::string l = convertNumeric(lhs, op_t);
        const std::string r = convertNumeric(rhs, op_t);

        if (b.op == TT::LESS) {
            return Semantic::isFloat(op_t)
                       ? emitFloatCmp(op_t, l, r, "olt")
                       : emitIntCmp(op_t, l, r, "slt", "ult");
        }
        if (b.op == TT::LESS_EQUAL) {
            return Semantic::isFloat(op_t)
                       ? emitFloatCmp(op_t, l, r, "ole")
                       : emitIntCmp(op_t, l, r, "sle", "ule");
        }
        if (b.op == TT::GREATER) {
            return Semantic::isFloat(op_t)
                       ? emitFloatCmp(op_t, l, r, "ogt")
                       : emitIntCmp(op_t, l, r, "sgt", "ugt");
        }
        if (b.op == TT::GREATER_EQUAL) {
            return Semantic::isFloat(op_t)
                       ? emitFloatCmp(op_t, l, r, "oge")
                       : emitIntCmp(op_t, l, r, "sge", "uge");
        }
    }

    internalError("unsupported binary expression reached codegen");
}

Codegen::Value Codegen::genUnary(const Parser::UnaryExpr& u,
                                 const Semantic::Type& result_t,
                                 Parser::NodeLocation loc) {
    if (u.op == TokenType::MINUS) {
        if (Semantic::isInteger(result_t) && isSigned(result_t.kind)) {
            if (const auto* literal =
                    std::get_if<Parser::IntLiteral>(&u.operand.node)) {
                std::string value = intLiteralToDecimal(literal->value);
                if (value != "0") value = "-" + value;

                const std::string ssa = freshValue();
                m_body << "  " << ssa << " = add " << llvmType(result_t)
                       << " 0, " << value << "\n";
                return {ssa, result_t};
            }
        }

        Value v = genExpr(u.operand);
        if (Semantic::isFloat(result_t)) {
            const std::string ssa = freshValue();
            m_body << "  " << ssa << " = fneg " << llvmType(result_t) << " "
                   << v.ssa << "\n";
            return {ssa, result_t};
        }
        if (Semantic::isInteger(result_t)) {
            return emitCheckedAddSubMul("sub", result_t, "0", v.ssa, loc);
        }
    }

    if (u.op == TokenType::NOT) {
        Value v = genExpr(u.operand);
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = xor i1 " << v.ssa << ", true\n";
        return {ssa, Semantic::makePrimitive(Semantic::TypeKind::BOOL)};
    }

    internalError("unsupported unary expression reached codegen");
}

Codegen::Value Codegen::genCall(const Parser::CallExpr& c,
                                const Semantic::Type& result_t,
                                Parser::NodeLocation loc) {
    (void)result_t;
    if (c.name.parts.size() == 1) {
        const std::string& name = c.name.parts[0];
        if (name == "print") return genBuiltinPrint(c);
        if (name == "input") return genBuiltinInput();
        if (name == "len") return genBuiltinLen(c);
        if (name == "assert") return genBuiltinAssert(c, loc);
        if (name == "code") return genBuiltinCode(c);
        if (name == "char_from") return genBuiltinCharFrom(c, loc);
        if (name == "exit") return genBuiltinExit(c);
        if (name == "panic") return genBuiltinPanic(c, loc);
    }

    const Semantic::FunctionSignature* sig = callTarget(&c);
    std::vector<Value> args;
    args.reserve(c.args.size());
    for (const auto& arg : c.args) args.push_back(genExpr(arg));

    std::vector<Value> converted;
    converted.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        converted.push_back(
            emitConversion(args[i], sig->param_types[i], false, loc));
    }

    const bool ret_void = sig->return_type.kind == Semantic::TypeKind::VOID;
    const std::string result = ret_void ? "" : freshValue();

    m_body << "  ";
    if (!ret_void) m_body << result << " = ";
    m_body << "call " << (ret_void ? "void" : llvmType(sig->return_type)) << " "
           << mangle(*sig) << "(";
    for (std::size_t i = 0; i < converted.size(); ++i) {
        if (i > 0) m_body << ", ";
        m_body << llvmType(sig->param_types[i]) << " " << converted[i].ssa;
    }
    m_body << ")\n";

    if (ret_void)
        return {"", Semantic::makePrimitive(Semantic::TypeKind::VOID)};
    return {result, sig->return_type};
}

Codegen::Value Codegen::genMethodCall(const Parser::MethodCall& c,
                                      const Semantic::Type&,
                                      Parser::NodeLocation loc) {
    const Semantic::FunctionSignature* sig = callTarget(&c);

    std::vector<Value> args;
    args.reserve(c.args.size() + 1);
    args.push_back(genExpr(c.object));
    for (const auto& arg : c.args) args.push_back(genExpr(arg));

    std::vector<Value> converted;
    converted.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        converted.push_back(
            emitConversion(args[i], sig->param_types[i], false, loc));
    }

    const bool ret_void = sig->return_type.kind == Semantic::TypeKind::VOID;
    const std::string result = ret_void ? "" : freshValue();

    m_body << "  ";
    if (!ret_void) m_body << result << " = ";
    m_body << "call " << (ret_void ? "void" : llvmType(sig->return_type)) << " "
           << mangle(*sig) << "(";
    for (std::size_t i = 0; i < converted.size(); ++i) {
        if (i > 0) m_body << ", ";
        m_body << llvmType(sig->param_types[i]) << " " << converted[i].ssa;
    }
    m_body << ")\n";

    if (ret_void)
        return {"", Semantic::makePrimitive(Semantic::TypeKind::VOID)};
    return {result, sig->return_type};
}

Codegen::Value Codegen::genIndex(const Parser::IndexExpr& ix,
                                 const Semantic::Type& result_t,
                                 Parser::NodeLocation loc) {
    const Semantic::Type arr_t = exprType(ix.object);
    const auto& array = std::get<Semantic::ArrayTypeInfo>(arr_t.data);

    const std::string arr_addr = isLvalueExpr(ix.object)
                                     ? genLvalueAddr(ix.object)
                                     : materializeAddr(genExpr(ix.object));

    Value idx = genExpr(ix.index);
    const std::string i64_idx =
        convertNumeric(idx, Semantic::makePrimitive(Semantic::TypeKind::INT64));
    emitBoundsCheck(i64_idx, array.size, loc);

    const std::string elem_ptr = freshValue();
    m_body << "  " << elem_ptr << " = getelementptr " << llvmType(arr_t)
           << ", ptr " << arr_addr << ", i64 0, i64 " << i64_idx << "\n";
    return loadValueAtAddr(result_t, elem_ptr);
}

Codegen::Value Codegen::genFieldAccess(const Parser::FieldAccess& fa,
                                       const Semantic::Type& result_t) {
    const Semantic::Type obj_t = exprType(fa.object);
    const std::string& qname =
        std::get<Semantic::StructTypeInfo>(obj_t.data).struct_name;
    const Semantic::StructSymbol* structure = findStructByQualifiedName(qname);
    if (structure == nullptr) internalError("missing struct " + qname);
    const int idx = structFieldIndex(*structure, fa.field);

    const std::string obj_addr = isLvalueExpr(fa.object)
                                     ? genLvalueAddr(fa.object)
                                     : materializeAddr(genExpr(fa.object));
    const std::string field_ptr = freshValue();
    m_body << "  " << field_ptr << " = getelementptr %struct."
           << mangleStruct(qname) << ", ptr " << obj_addr << ", i32 0, i32 "
           << idx << "\n";
    return loadValueAtAddr(result_t, field_ptr);
}

Codegen::Value Codegen::genCast(const Parser::CastExpr& c,
                                const Semantic::Type& result_t,
                                Parser::NodeLocation loc) {
    return emitConversion(genExpr(c.value), result_t, true, loc);
}

Codegen::Value Codegen::genArrayLit(const Parser::Expr& wrap,
                                    const Parser::ArrayLiteral& al,
                                    Parser::NodeLocation) {
    const Semantic::Type array_t = exprType(wrap);
    const auto& info = std::get<Semantic::ArrayTypeInfo>(array_t.data);
    const Semantic::Type& elem_t = *info.element_type;

    const std::string addr = emitEntryAlloca("arr.lit", array_t);

    for (std::size_t i = 0; i < al.elements.size(); ++i) {
        Value v = genExpr(al.elements[i]);
        v = emitConversion(v, elem_t, false, al.elements[i].loc);
        const std::string ptr = freshValue();
        m_body << "  " << ptr << " = getelementptr " << llvmType(array_t)
               << ", ptr " << addr << ", i64 0, i64 " << i << "\n";
        storeValue(elem_t, v.ssa, ptr);
    }

    return loadValueAtAddr(array_t, addr);
}

Codegen::Value Codegen::genStructLit(const Parser::StructLiteral& sl,
                                     const Semantic::Type& result_t) {
    (void)sl;
    const auto& info = std::get<Semantic::StructTypeInfo>(result_t.data);
    const Semantic::StructSymbol* structure =
        findStructByQualifiedName(info.struct_name);
    if (structure == nullptr) {
        internalError("missing struct " + info.struct_name);
    }

    const std::string st = "%struct." + mangleStruct(info.struct_name);
    const std::string addr = emitEntryAlloca("struct.lit", result_t);

    for (const auto& field : sl.fields) {
        const int idx = structFieldIndex(*structure, field.name);
        const Semantic::Type& field_t =
            structure->fields[static_cast<std::size_t>(idx)].type;
        Value v = genExpr(field.value);
        v = emitConversion(v, field_t, false, field.loc);

        const std::string ptr = freshValue();
        m_body << "  " << ptr << " = getelementptr " << st << ", ptr " << addr
               << ", i32 0, i32 " << idx << "\n";
        storeValue(field_t, v.ssa, ptr);
    }

    return loadValueAtAddr(result_t, addr);
}

Codegen::Value Codegen::genBuiltinPrint(const Parser::CallExpr& c) {
    Value v = genExpr(c.args[0]);
    using TK = Semantic::TypeKind;

    switch (v.type.kind) {
        case TK::INT8:
        case TK::INT16:
        case TK::INT32:
        case TK::INT64: {
            Value w = emitConversion(v, Semantic::makePrimitive(TK::INT64),
                                     false, c.args[0].loc);
            m_body << "  call void @bina_print_i64(i64 " << w.ssa << ")\n";
            break;
        }
        case TK::UINT8:
        case TK::UINT16:
        case TK::UINT32:
        case TK::UINT64: {
            Value w = emitConversion(v, Semantic::makePrimitive(TK::UINT64),
                                     false, c.args[0].loc);
            m_body << "  call void @bina_print_u64(i64 " << w.ssa << ")\n";
            break;
        }
        case TK::FLOAT32:
        case TK::FLOAT64: {
            Value w = emitConversion(v, Semantic::makePrimitive(TK::FLOAT64),
                                     false, c.args[0].loc);
            m_body << "  call void @bina_print_f64(double " << w.ssa << ")\n";
            break;
        }
        case TK::BOOL:
            m_body << "  call void @bina_print_bool(i1 " << v.ssa << ")\n";
            break;
        case TK::CHAR:
            m_body << "  call void @bina_print_char(i32 " << v.ssa << ")\n";
            break;
        case TK::STRING: {
            const std::string ptr = freshValue();
            const std::string len = freshValue();
            m_body << "  " << ptr << " = extractvalue { ptr, i64 } " << v.ssa
                   << ", 0\n";
            m_body << "  " << len << " = extractvalue { ptr, i64 } " << v.ssa
                   << ", 1\n";
            m_body << "  call void @bina_print_str(ptr " << ptr << ", i64 "
                   << len << ")\n";
            break;
        }
        case TK::ARRAY:
            emitPrintArray(v.type, materializeAddr(v));
            break;
        case TK::STRUCT:
            emitPrintStruct(v.type, materializeAddr(v));
            break;
        default:
            internalError("print unsupported type");
    }

    return {"", Semantic::makePrimitive(TK::VOID)};
}

Codegen::Value Codegen::genBuiltinInput() {
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = call { ptr, i64 } @bina_input()\n";
    return {ssa, Semantic::makePrimitive(Semantic::TypeKind::STRING)};
}

Codegen::Value Codegen::genBuiltinLen(const Parser::CallExpr& c) {
    Value v = genExpr(c.args[0]);
    if (v.type.kind == Semantic::TypeKind::STRING) {
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = extractvalue { ptr, i64 } " << v.ssa
               << ", 1\n";
        return {ssa, Semantic::makePrimitive(Semantic::TypeKind::INT64)};
    }

    if (v.type.kind == Semantic::TypeKind::ARRAY) {
        const auto& array = std::get<Semantic::ArrayTypeInfo>(v.type.data);
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = add i64 0, " << array.size << "\n";
        return {ssa, Semantic::makePrimitive(Semantic::TypeKind::INT64)};
    }

    internalError("len unsupported type");
}

Codegen::Value Codegen::genBuiltinAssert(const Parser::CallExpr& c,
                                         Parser::NodeLocation loc) {
    Value v = genExpr(c.args[0]);
    m_body << "  call void @bina_assert(i1 " << v.ssa << ", i64 " << loc.line
           << ")\n";
    return {"", Semantic::makePrimitive(Semantic::TypeKind::VOID)};
}

Codegen::Value Codegen::genBuiltinCode(const Parser::CallExpr& c) {
    Value v = genExpr(c.args[0]);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = zext i32 " << v.ssa << " to i64\n";
    return {ssa, Semantic::makePrimitive(Semantic::TypeKind::INT64)};
}

Codegen::Value Codegen::genBuiltinCharFrom(const Parser::CallExpr& c,
                                           Parser::NodeLocation loc) {
    Value v = genExpr(c.args[0]);
    Value w = emitConversion(
        v, Semantic::makePrimitive(Semantic::TypeKind::INT64), false, loc);
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = call i32 @bina_char_from(i64 " << w.ssa
           << ", i64 " << loc.line << ")\n";
    return {ssa, Semantic::makePrimitive(Semantic::TypeKind::CHAR)};
}

Codegen::Value Codegen::genBuiltinExit(const Parser::CallExpr& c) {
    Value v = genExpr(c.args[0]);
    Value w =
        emitConversion(v, Semantic::makePrimitive(Semantic::TypeKind::INT64),
                       false, c.args[0].loc);
    m_body << "  call void @bina_exit(i64 " << w.ssa << ")\n";
    emitTerminator("unreachable");
    return {"", Semantic::makePrimitive(Semantic::TypeKind::VOID)};
}

Codegen::Value Codegen::genBuiltinPanic(const Parser::CallExpr& c,
                                        Parser::NodeLocation loc) {
    Value v = genExpr(c.args[0]);
    const std::string ptr = freshValue();
    const std::string len = freshValue();
    m_body << "  " << ptr << " = extractvalue { ptr, i64 } " << v.ssa
           << ", 0\n";
    m_body << "  " << len << " = extractvalue { ptr, i64 } " << v.ssa
           << ", 1\n";
    m_body << "  call void @bina_panic(ptr " << ptr << ", i64 " << len
           << ", i64 " << loc.line << ")\n";
    emitTerminator("unreachable");
    return {"", Semantic::makePrimitive(Semantic::TypeKind::VOID)};
}

std::string Codegen::genLvalueAddr(const Parser::Expr& e) {
    return std::visit(
        overloaded{
            [&](const Parser::Identifier& id) -> std::string {
                if (id.parts.size() != 1) {
                    internalError(
                        "qualified identifier lvalue reached codegen");
                }
                const LocalSlot* slot = envLookup(id.parts[0]);
                if (slot == nullptr) {
                    internalError("unknown lvalue '" + id.parts[0] + "'");
                }
                return slot->addr;
            },
            [&](const std::unique_ptr<Parser::IndexExpr>& ix) -> std::string {
                const std::string arr_addr = genLvalueAddr(ix->object);
                const Semantic::Type arr_t = exprType(ix->object);
                const auto& array =
                    std::get<Semantic::ArrayTypeInfo>(arr_t.data);
                Value idx = genExpr(ix->index);
                const std::string i64_idx = convertNumeric(
                    idx, Semantic::makePrimitive(Semantic::TypeKind::INT64));
                emitBoundsCheck(i64_idx, array.size, ix->index.loc);
                const std::string ptr = freshValue();
                m_body << "  " << ptr << " = getelementptr " << llvmType(arr_t)
                       << ", ptr " << arr_addr << ", i64 0, i64 " << i64_idx
                       << "\n";
                return ptr;
            },
            [&](const std::unique_ptr<Parser::FieldAccess>& fa) -> std::string {
                const std::string obj_addr = genLvalueAddr(fa->object);
                const Semantic::Type obj_t = exprType(fa->object);
                const std::string& qname =
                    std::get<Semantic::StructTypeInfo>(obj_t.data).struct_name;
                const Semantic::StructSymbol* structure =
                    findStructByQualifiedName(qname);
                if (structure == nullptr) {
                    internalError("missing struct " + qname);
                }
                const int idx = structFieldIndex(*structure, fa->field);
                const std::string ptr = freshValue();
                m_body << "  " << ptr << " = getelementptr %struct."
                       << mangleStruct(qname) << ", ptr " << obj_addr
                       << ", i32 0, i32 " << idx << "\n";
                return ptr;
            },
            [&](const auto&) -> std::string {
                internalError("non-lvalue expression reached genLvalueAddr");
            },
        },
        e.node);
}

bool Codegen::isLvalueExpr(const Parser::Expr& e) {
    return std::visit(
        overloaded{
            [&](const Parser::Identifier&) { return true; },
            [&](const std::unique_ptr<Parser::IndexExpr>&) { return true; },
            [&](const std::unique_ptr<Parser::FieldAccess>&) { return true; },
            [&](const auto&) { return false; },
        },
        e.node);
}

std::string Codegen::materializeAddr(const Value& v) {
    const std::string addr = emitEntryAlloca("tmp", v.type);
    storeValue(v.type, v.ssa, addr);
    return addr;
}

std::string Codegen::emitEntryAlloca(std::string_view source_name,
                                     const Semantic::Type& t) {
    const std::string addr = freshLocalAddr(source_name);
    m_alloca_lines.push_back("  " + addr + " = alloca " + llvmStoredType(t) +
                             "\n");
    return addr;
}

Codegen::Value Codegen::genCompareEq(const Semantic::Type& t,
                                     const std::string& lhs_ptr,
                                     const std::string& rhs_ptr) {
    using TK = Semantic::TypeKind;

    if (Semantic::isInteger(t) || t.kind == TK::BOOL || t.kind == TK::CHAR) {
        Value lhs = loadValueAtAddr(t, lhs_ptr);
        Value rhs = loadValueAtAddr(t, rhs_ptr);
        const std::string eq = freshValue();
        m_body << "  " << eq << " = icmp eq " << llvmType(t) << " " << lhs.ssa
               << ", " << rhs.ssa << "\n";
        return {eq, Semantic::makePrimitive(TK::BOOL)};
    }

    if (Semantic::isFloat(t)) {
        Value lhs = loadValueAtAddr(t, lhs_ptr);
        Value rhs = loadValueAtAddr(t, rhs_ptr);
        const std::string eq = freshValue();
        m_body << "  " << eq << " = fcmp oeq " << llvmType(t) << " " << lhs.ssa
               << ", " << rhs.ssa << "\n";
        return {eq, Semantic::makePrimitive(TK::BOOL)};
    }

    if (t.kind == TK::STRING) {
        Value lhs = loadValueAtAddr(t, lhs_ptr);
        Value rhs = loadValueAtAddr(t, rhs_ptr);
        const std::string eq = freshValue();
        m_body << "  " << eq << " = call i1 @bina_str_eq({ ptr, i64 } "
               << lhs.ssa << ", { ptr, i64 } " << rhs.ssa << ")\n";
        return {eq, Semantic::makePrimitive(TK::BOOL)};
    }

    if (t.kind == TK::ARRAY) {
        const auto& info = std::get<Semantic::ArrayTypeInfo>(t.data);
        const Semantic::Type& elem_t = *info.element_type;
        std::string acc = freshValue();
        m_body << "  " << acc << " = xor i1 true, false\n";
        for (std::size_t i = 0; i < info.size; ++i) {
            const std::string lp = freshValue();
            const std::string rp = freshValue();
            m_body << "  " << lp << " = getelementptr " << llvmType(t)
                   << ", ptr " << lhs_ptr << ", i64 0, i64 " << i << "\n";
            m_body << "  " << rp << " = getelementptr " << llvmType(t)
                   << ", ptr " << rhs_ptr << ", i64 0, i64 " << i << "\n";
            Value elem_eq = genCompareEq(elem_t, lp, rp);
            const std::string next = freshValue();
            m_body << "  " << next << " = and i1 " << acc << ", " << elem_eq.ssa
                   << "\n";
            acc = next;
        }
        return {acc, Semantic::makePrimitive(TK::BOOL)};
    }

    if (t.kind == TK::STRUCT) {
        const auto& info = std::get<Semantic::StructTypeInfo>(t.data);
        const Semantic::StructSymbol* structure =
            findStructByQualifiedName(info.struct_name);
        if (structure == nullptr) {
            internalError("missing struct " + info.struct_name);
        }

        const std::string st = "%struct." + mangleStruct(info.struct_name);
        std::string acc = freshValue();
        m_body << "  " << acc << " = xor i1 true, false\n";
        for (std::size_t i = 0; i < structure->fields.size(); ++i) {
            const auto& field_t = structure->fields[i].type;
            const std::string lp = freshValue();
            const std::string rp = freshValue();
            m_body << "  " << lp << " = getelementptr " << st << ", ptr "
                   << lhs_ptr << ", i32 0, i32 " << i << "\n";
            m_body << "  " << rp << " = getelementptr " << st << ", ptr "
                   << rhs_ptr << ", i32 0, i32 " << i << "\n";
            Value field_eq = genCompareEq(field_t, lp, rp);
            const std::string next = freshValue();
            m_body << "  " << next << " = and i1 " << acc << ", "
                   << field_eq.ssa << "\n";
            acc = next;
        }
        return {acc, Semantic::makePrimitive(TK::BOOL)};
    }

    internalError("unsupported equality type");
}

std::string Codegen::freshValue() {
    return "%t" + std::to_string(++m_value_id);
}

std::string Codegen::freshLabel(std::string_view prefix) {
    return std::string(prefix) + "." + std::to_string(++m_label_id);
}

std::string Codegen::freshStrConst() {
    return "@.str." + std::to_string(m_str_const_id++);
}

std::string Codegen::freshLocalAddr(std::string_view source_name) {
    return "%addr." + sanitizeIdent(source_name) + "." +
           std::to_string(++m_value_id);
}

std::string Codegen::sanitizeIdent(std::string_view source_name) {
    std::string result;
    for (char c : source_name) {
        unsigned char b = static_cast<unsigned char>(c);
        if (std::isalnum(b) || c == '_') {
            result += c;
        } else {
            result += '_';
        }
    }
    if (result.empty()) result = "tmp";
    return result;
}

std::string Codegen::emitStringConstant(std::string_view bytes) {
    const std::string name = freshStrConst();
    m_header << name << " = private unnamed_addr constant [" << bytes.size()
             << " x i8] c\"" << escapeLlvmBytes(bytes) << "\", align 1\n";
    return name;
}

std::string Codegen::llvmType(const Semantic::Type& t) {
    using TK = Semantic::TypeKind;
    switch (t.kind) {
        case TK::INT8:
        case TK::UINT8:
            return "i8";
        case TK::INT16:
        case TK::UINT16:
            return "i16";
        case TK::INT32:
        case TK::UINT32:
        case TK::CHAR:
            return "i32";
        case TK::INT64:
        case TK::UINT64:
            return "i64";
        case TK::FLOAT32:
            return "float";
        case TK::FLOAT64:
            return "double";
        case TK::BOOL:
            return "i1";
        case TK::STRING:
            return "{ ptr, i64 }";
        case TK::VOID:
            return "void";
        case TK::ARRAY: {
            const auto& array = std::get<Semantic::ArrayTypeInfo>(t.data);
            return "[" + std::to_string(array.size) + " x " +
                   llvmStoredType(*array.element_type) + "]";
        }
        case TK::STRUCT:
            return "%struct." +
                   mangleStruct(
                       std::get<Semantic::StructTypeInfo>(t.data).struct_name);
        case TK::ERROR:
            internalError("error type reached codegen");
    }
}

std::string Codegen::llvmStoredType(const Semantic::Type& t) {
    if (t.kind == Semantic::TypeKind::BOOL) return "i8";
    return llvmType(t);
}

std::string Codegen::mangle(const Semantic::FunctionSignature& sig) {
    if (sig.name == "main" && sig.namespace_qname.empty() &&
        sig.enclosing_struct_qname.empty()) {
        return "@main";
    }

    std::string result = "@bina.";
    if (!sig.enclosing_struct_qname.empty()) {
        result += mangleStruct(sig.enclosing_struct_qname) + ".";
    } else if (!sig.namespace_qname.empty()) {
        result += mangleStruct(sig.namespace_qname) + ".";
    }

    result += sig.name + "$";
    for (std::size_t i = 0; i < sig.param_types.size(); ++i) {
        if (i > 0) result += "_";
        result += mangleType(sig.param_types[i]);
    }
    return result;
}

std::string Codegen::mangleType(const Semantic::Type& t) {
    using TK = Semantic::TypeKind;
    switch (t.kind) {
        case TK::INT8:
            return "i8";
        case TK::INT16:
            return "i16";
        case TK::INT32:
            return "i32";
        case TK::INT64:
            return "i64";
        case TK::UINT8:
            return "u8";
        case TK::UINT16:
            return "u16";
        case TK::UINT32:
            return "u32";
        case TK::UINT64:
            return "u64";
        case TK::FLOAT32:
            return "f32";
        case TK::FLOAT64:
            return "f64";
        case TK::BOOL:
            return "b";
        case TK::CHAR:
            return "c";
        case TK::STRING:
            return "s";
        case TK::VOID:
            return "v";
        case TK::ARRAY: {
            const auto& array = std::get<Semantic::ArrayTypeInfo>(t.data);
            return "a" + std::to_string(array.size) + "." +
                   mangleType(*array.element_type);
        }
        case TK::STRUCT:
            return "S." +
                   mangleStruct(
                       std::get<Semantic::StructTypeInfo>(t.data).struct_name);
        case TK::ERROR:
            internalError("error type reached mangling");
    }
}

std::string Codegen::mangleStruct(const std::string& qualified_name) {
    std::string result = qualified_name;
    std::size_t pos = 0;
    while ((pos = result.find("::", pos)) != std::string::npos) {
        result.replace(pos, 2, ".");
        ++pos;
    }
    return result;
}

void Codegen::emitTerminator(std::string_view instr) {
    m_body << "  " << instr << "\n";
    m_block_terminated = true;
}

void Codegen::startLabel(std::string_view label) {
    m_body << label << ":\n";
    m_current_label = std::string(label);
    m_block_terminated = false;
}

Codegen::Value Codegen::emitConversion(Value v, const Semantic::Type& to,
                                       bool explicit_cast,
                                       Parser::NodeLocation loc) {
    (void)explicit_cast;
    using TK = Semantic::TypeKind;
    const TK from_k = v.type.kind;
    const TK to_k = to.kind;

    if (Semantic::typeEquals(v.type, to)) return v;

    if (Semantic::isInteger(v.type) && Semantic::isInteger(to)) {
        const int from_bits = bitWidth(from_k);
        const int to_bits = bitWidth(to_k);
        if (from_bits == to_bits) return {v.ssa, to};

        const std::string ssa = freshValue();
        const std::string op = to_bits > from_bits
                                   ? (isSigned(from_k) ? "sext" : "zext")
                                   : "trunc";
        m_body << "  " << ssa << " = " << op << " " << llvmType(v.type) << " "
               << v.ssa << " to " << llvmType(to) << "\n";
        return {ssa, to};
    }

    if (Semantic::isInteger(v.type) && Semantic::isFloat(to)) {
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = "
               << (isSigned(from_k) ? "sitofp" : "uitofp") << " "
               << llvmType(v.type) << " " << v.ssa << " to " << llvmType(to)
               << "\n";
        return {ssa, to};
    }

    if (Semantic::isFloat(v.type) && Semantic::isInteger(to)) {
        // fptosi/fptoui вне диапазона целевого типа дают poison, поэтому
        // проверяем диапазон (и NaN) заранее; проверка ведётся в double,
        // где границы всех целевых типов представимы точно.
        std::string dval = v.ssa;
        if (from_k == TK::FLOAT32) {
            dval = freshValue();
            m_body << "  " << dval << " = fpext float " << v.ssa
                   << " to double\n";
        }
        emitFloatToIntRangeCheck(dval, to_k, loc);

        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = " << (isSigned(to_k) ? "fptosi" : "fptoui")
               << " double " << dval << " to " << llvmType(to) << "\n";
        return {ssa, to};
    }

    if (Semantic::isFloat(v.type) && Semantic::isFloat(to)) {
        const int from_bits = bitWidth(from_k);
        const int to_bits = bitWidth(to_k);
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = "
               << (to_bits > from_bits ? "fpext" : "fptrunc") << " "
               << llvmType(v.type) << " " << v.ssa << " to " << llvmType(to)
               << "\n";
        return {ssa, to};
    }

    if (v.type.kind == TK::BOOL && Semantic::isInteger(to)) {
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = zext i1 " << v.ssa << " to "
               << llvmType(to) << "\n";
        return {ssa, to};
    }

    if (Semantic::isInteger(v.type) && to.kind == TK::BOOL) {
        const std::string ssa = freshValue();
        m_body << "  " << ssa << " = icmp ne " << llvmType(v.type) << " "
               << v.ssa << ", 0\n";
        return {ssa, to};
    }

    if (Semantic::isArithmetic(v.type) && to.kind == TK::STRING) {
        if (Semantic::isInteger(v.type)) {
            const TK wide_kind = isSigned(from_k) ? TK::INT64 : TK::UINT64;
            Value wide = emitConversion(v, Semantic::makePrimitive(wide_kind),
                                        false, loc);
            const std::string ssa = freshValue();
            const char* fn = isSigned(from_k) ? "@bina_to_string_i64"
                                              : "@bina_to_string_u64";
            m_body << "  " << ssa << " = call { ptr, i64 } " << fn << "(i64 "
                   << wide.ssa << ")\n";
            return {ssa, to};
        }

        Value wide =
            emitConversion(v, Semantic::makePrimitive(TK::FLOAT64), false, loc);
        const std::string ssa = freshValue();
        m_body << "  " << ssa
               << " = call { ptr, i64 } @bina_to_string_f64(double " << wide.ssa
               << ")\n";
        return {ssa, to};
    }

    if (v.type.kind == TK::STRING && Semantic::isArithmetic(to)) {
        if (Semantic::isInteger(to)) {
            const std::string parsed = freshValue();
            const char* fn =
                isSigned(to_k) ? "@bina_parse_i64" : "@bina_parse_u64";
            m_body << "  " << parsed << " = call i64 " << fn << "({ ptr, i64 } "
                   << v.ssa << ", i64 " << loc.line << ")\n";

            const int to_bits = bitWidth(to_k);
            if (to_bits < 64) emitRangeCheck(parsed, to_k, loc);

            if (to_bits < 64) {
                const std::string ssa = freshValue();
                m_body << "  " << ssa << " = trunc i64 " << parsed << " to "
                       << llvmType(to) << "\n";
                return {ssa, to};
            }
            return {parsed, to};
        }

        const std::string parsed = freshValue();
        m_body << "  " << parsed
               << " = call double @bina_parse_f64({ ptr, i64 } " << v.ssa
               << ", i64 " << loc.line << ")\n";
        if (to.kind == TK::FLOAT32) {
            const std::string ssa = freshValue();
            m_body << "  " << ssa << " = fptrunc double " << parsed
                   << " to float\n";
            return {ssa, to};
        }
        return {parsed, to};
    }

    internalError("unsupported conversion from " +
                  Semantic::typeToString(v.type) + " to " +
                  Semantic::typeToString(to));
}

std::string Codegen::convertNumeric(Value v, const Semantic::Type& to) {
    return emitConversion(v, to, false, {}).ssa;
}

Codegen::Value Codegen::makeBinOp(std::string_view op, std::string_view llvm_ty,
                                  std::string_view lhs, std::string_view rhs,
                                  const Semantic::Type& result_t) {
    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = " << op << " " << llvm_ty << " " << lhs << ", "
           << rhs << "\n";
    return {ssa, result_t};
}

Codegen::Value Codegen::emitCheckedAddSubMul(std::string_view base_op,
                                             const Semantic::Type& t,
                                             const std::string& lhs,
                                             const std::string& rhs,
                                             Parser::NodeLocation loc) {
    const char sign = isSigned(t.kind) ? 's' : 'u';
    const std::string ty = llvmType(t);
    const std::string intrinsic = "@llvm." + std::string(1, sign) +
                                  std::string(base_op) + ".with.overflow." + ty;
    const std::string ret_ty = "{ " + ty + ", i1 }";

    const std::string pair = freshValue();
    m_body << "  " << pair << " = call " << ret_ty << " " << intrinsic << "("
           << ty << " " << lhs << ", " << ty << " " << rhs << ")\n";

    const std::string val = freshValue();
    const std::string ovf = freshValue();
    m_body << "  " << val << " = extractvalue " << ret_ty << " " << pair
           << ", 0\n";
    m_body << "  " << ovf << " = extractvalue " << ret_ty << " " << pair
           << ", 1\n";

    const std::string bad = freshLabel("ovf.bad");
    const std::string ok = freshLabel("ovf.ok");
    emitTerminator("br i1 " + ovf + ", label %" + bad + ", label %" + ok);

    startLabel(bad);
    m_body << "  call void @bina_int_overflow(i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
    return {val, t};
}

void Codegen::emitDivByZeroCheck(const std::string& rhs,
                                 std::string_view llvm_ty,
                                 Parser::NodeLocation loc) {
    const std::string is_zero = freshValue();
    m_body << "  " << is_zero << " = icmp eq " << llvm_ty << " " << rhs
           << ", 0\n";

    const std::string bad = freshLabel("div.zero");
    const std::string ok = freshLabel("div.ok");
    emitTerminator("br i1 " + is_zero + ", label %" + bad + ", label %" + ok);

    startLabel(bad);
    m_body << "  call void @bina_div_zero(i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
}

void Codegen::emitSignedDivOverflowCheck(const std::string& lhs,
                                         const std::string& rhs,
                                         const Semantic::Type& t,
                                         Parser::NodeLocation loc) {
    const auto [min_value, max_value] = integerBounds(t.kind);
    (void)max_value;
    const std::string ty = llvmType(t);

    const std::string is_min = freshValue();
    const std::string is_neg1 = freshValue();
    const std::string bad_cond = freshValue();
    m_body << "  " << is_min << " = icmp eq " << ty << " " << lhs << ", "
           << min_value << "\n";
    m_body << "  " << is_neg1 << " = icmp eq " << ty << " " << rhs << ", -1\n";
    m_body << "  " << bad_cond << " = and i1 " << is_min << ", " << is_neg1
           << "\n";

    const std::string bad = freshLabel("div.ovf");
    const std::string ok = freshLabel("div.ok");
    emitTerminator("br i1 " + bad_cond + ", label %" + bad + ", label %" + ok);

    startLabel(bad);
    m_body << "  call void @bina_int_overflow(i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
}

void Codegen::emitBoundsCheck(const std::string& index, std::size_t len,
                              Parser::NodeLocation loc) {
    const std::string lo = freshValue();
    const std::string hi = freshValue();
    const std::string bad_cond = freshValue();
    m_body << "  " << lo << " = icmp slt i64 " << index << ", 0\n";
    m_body << "  " << hi << " = icmp sge i64 " << index << ", " << len << "\n";
    m_body << "  " << bad_cond << " = or i1 " << lo << ", " << hi << "\n";

    const std::string bad = freshLabel("idx.bad");
    const std::string ok = freshLabel("idx.ok");
    emitTerminator("br i1 " + bad_cond + ", label %" + bad + ", label %" + ok);

    startLabel(bad);
    m_body << "  call void @bina_index_oob(i64 " << index << ", i64 " << len
           << ", i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
}

void Codegen::emitRangeCheck(const std::string& i64_val, Semantic::TypeKind to,
                             Parser::NodeLocation loc) {
    const auto [lo_bound, hi_bound] = integerBounds(to);
    std::string bad_cond;

    if (isSigned(to)) {
        const std::string lo = freshValue();
        const std::string hi = freshValue();
        bad_cond = freshValue();
        m_body << "  " << lo << " = icmp slt i64 " << i64_val << ", "
               << lo_bound << "\n";
        m_body << "  " << hi << " = icmp sgt i64 " << i64_val << ", "
               << hi_bound << "\n";
        m_body << "  " << bad_cond << " = or i1 " << lo << ", " << hi << "\n";
    } else {
        bad_cond = freshValue();
        m_body << "  " << bad_cond << " = icmp ugt i64 " << i64_val << ", "
               << hi_bound << "\n";
    }

    const std::string bad = freshLabel("range.bad");
    const std::string ok = freshLabel("range.ok");
    emitTerminator("br i1 " + bad_cond + ", label %" + bad + ", label %" + ok);

    startLabel(bad);
    m_body << "  call void @bina_int_overflow(i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
}

void Codegen::emitFloatToIntRangeCheck(const std::string& dval,
                                       Semantic::TypeKind to,
                                       Parser::NodeLocation loc) {
    using TK = Semantic::TypeKind;

    double lo = 0.0;
    double hi = 0.0;
    const char* lo_pred = "ogt";
    switch (to) {
        case TK::INT8:
            lo = -129.0;
            hi = 128.0;
            break;
        case TK::INT16:
            lo = -32769.0;
            hi = 32768.0;
            break;
        case TK::INT32:
            lo = -2147483649.0;
            hi = 2147483648.0;
            break;
        case TK::INT64:
            lo = -9223372036854775808.0;
            hi = 9223372036854775808.0;
            lo_pred = "oge";
            break;
        case TK::UINT8:
            lo = -1.0;
            hi = 256.0;
            break;
        case TK::UINT16:
            lo = -1.0;
            hi = 65536.0;
            break;
        case TK::UINT32:
            lo = -1.0;
            hi = 4294967296.0;
            break;
        case TK::UINT64:
            lo = -1.0;
            hi = 18446744073709551616.0;
            break;
        default:
            internalError("float-to-int range check for non-integer type");
    }

    const std::string above = freshValue();
    const std::string below = freshValue();
    const std::string ok_cond = freshValue();
    m_body << "  " << above << " = fcmp " << lo_pred << " double " << dval
           << ", " << doubleHex(lo) << "\n";
    m_body << "  " << below << " = fcmp olt double " << dval << ", "
           << doubleHex(hi) << "\n";
    m_body << "  " << ok_cond << " = and i1 " << above << ", " << below << "\n";

    const std::string bad = freshLabel("fcast.bad");
    const std::string ok = freshLabel("fcast.ok");
    emitTerminator("br i1 " + ok_cond + ", label %" + ok + ", label %" + bad);

    startLabel(bad);
    m_body << "  call void @bina_int_overflow(i64 " << loc.line << ")\n";
    emitTerminator("unreachable");

    startLabel(ok);
}

Codegen::Value Codegen::genShortCircuit(const Parser::BinaryExpr& b,
                                        bool is_and) {
    Value lhs = genExpr(b.left);
    const std::string lhs_block = m_current_label;
    const std::string rhs_label = freshLabel(is_and ? "and.rhs" : "or.rhs");
    const std::string end_label = freshLabel(is_and ? "and.end" : "or.end");

    if (is_and) {
        emitTerminator("br i1 " + lhs.ssa + ", label %" + rhs_label +
                       ", label %" + end_label);
    } else {
        emitTerminator("br i1 " + lhs.ssa + ", label %" + end_label +
                       ", label %" + rhs_label);
    }

    startLabel(rhs_label);
    Value rhs = genExpr(b.right);
    const std::string rhs_block = m_current_label;
    emitTerminator("br label %" + end_label);

    startLabel(end_label);
    const std::string result = freshValue();
    if (is_and) {
        m_body << "  " << result << " = phi i1 [ false, %" << lhs_block
               << " ], [ " << rhs.ssa << ", %" << rhs_block << " ]\n";
    } else {
        m_body << "  " << result << " = phi i1 [ true, %" << lhs_block
               << " ], [ " << rhs.ssa << ", %" << rhs_block << " ]\n";
    }
    return {result, Semantic::makePrimitive(Semantic::TypeKind::BOOL)};
}

void Codegen::emitPrintArray(const Semantic::Type& at,
                             const std::string& addr) {
    const auto& info = std::get<Semantic::ArrayTypeInfo>(at.data);
    const Semantic::Type& elem_t = *info.element_type;
    emitPrintLiteralString("[");
    for (std::size_t i = 0; i < info.size; ++i) {
        if (i > 0) emitPrintLiteralString(", ");
        const std::string ptr = freshValue();
        m_body << "  " << ptr << " = getelementptr " << llvmType(at) << ", ptr "
               << addr << ", i64 0, i64 " << i << "\n";
        emitPrintValueAtAddr(elem_t, ptr);
    }
    emitPrintLiteralString("]");
}

void Codegen::emitPrintStruct(const Semantic::Type& st,
                              const std::string& addr) {
    const auto& info = std::get<Semantic::StructTypeInfo>(st.data);
    const Semantic::StructSymbol* structure =
        findStructByQualifiedName(info.struct_name);
    if (structure == nullptr)
        internalError("missing struct " + info.struct_name);

    const std::size_t sep = info.struct_name.rfind("::");
    const std::string short_name = sep == std::string::npos
                                       ? info.struct_name
                                       : info.struct_name.substr(sep + 2);
    emitPrintLiteralString(short_name + " { ");

    const std::string llvm_st = "%struct." + mangleStruct(info.struct_name);
    for (std::size_t i = 0; i < structure->fields.size(); ++i) {
        if (i > 0) emitPrintLiteralString(", ");
        emitPrintLiteralString(structure->fields[i].name + ": ");
        const std::string ptr = freshValue();
        m_body << "  " << ptr << " = getelementptr " << llvm_st << ", ptr "
               << addr << ", i32 0, i32 " << i << "\n";
        emitPrintValueAtAddr(structure->fields[i].type, ptr);
    }

    emitPrintLiteralString(" }");
}

void Codegen::emitPrintValueAtAddr(const Semantic::Type& t,
                                   const std::string& ptr) {
    if (t.kind == Semantic::TypeKind::ARRAY) {
        emitPrintArray(t, ptr);
        return;
    }
    if (t.kind == Semantic::TypeKind::STRUCT) {
        emitPrintStruct(t, ptr);
        return;
    }

    Value v = loadValueAtAddr(t, ptr);
    emitPrintScalarSsa(t, v.ssa);
}

void Codegen::emitPrintScalarSsa(const Semantic::Type& t,
                                 const std::string& ssa) {
    using TK = Semantic::TypeKind;
    switch (t.kind) {
        case TK::INT8:
        case TK::INT16:
        case TK::INT32:
        case TK::INT64: {
            Value w = emitConversion(
                {ssa, t}, Semantic::makePrimitive(TK::INT64), false, {});
            m_body << "  call void @bina_print_i64(i64 " << w.ssa << ")\n";
            return;
        }
        case TK::UINT8:
        case TK::UINT16:
        case TK::UINT32:
        case TK::UINT64: {
            Value w = emitConversion(
                {ssa, t}, Semantic::makePrimitive(TK::UINT64), false, {});
            m_body << "  call void @bina_print_u64(i64 " << w.ssa << ")\n";
            return;
        }
        case TK::FLOAT32:
        case TK::FLOAT64: {
            Value w = emitConversion(
                {ssa, t}, Semantic::makePrimitive(TK::FLOAT64), false, {});
            m_body << "  call void @bina_print_f64(double " << w.ssa << ")\n";
            return;
        }
        case TK::BOOL:
            m_body << "  call void @bina_print_bool(i1 " << ssa << ")\n";
            return;
        case TK::CHAR:
            m_body << "  call void @bina_print_char(i32 " << ssa << ")\n";
            return;
        case TK::STRING: {
            const std::string ptr = freshValue();
            const std::string len = freshValue();
            m_body << "  " << ptr << " = extractvalue { ptr, i64 } " << ssa
                   << ", 0\n";
            m_body << "  " << len << " = extractvalue { ptr, i64 } " << ssa
                   << ", 1\n";
            m_body << "  call void @bina_print_str(ptr " << ptr << ", i64 "
                   << len << ")\n";
            return;
        }
        default:
            internalError("non-scalar print reached scalar printer");
    }
}

void Codegen::emitPrintLiteralString(std::string_view text) {
    const std::string name = emitStringConstant(text);
    const std::string ptr = freshValue();
    m_body << "  " << ptr << " = getelementptr [" << text.size()
           << " x i8], ptr " << name << ", i64 0, i64 0\n";
    m_body << "  call void @bina_print_str(ptr " << ptr << ", i64 "
           << text.size() << ")\n";
}

void Codegen::storeValue(const Semantic::Type& t, const std::string& value,
                         const std::string& addr) {
    if (t.kind == Semantic::TypeKind::BOOL) {
        const std::string stored = freshValue();
        m_body << "  " << stored << " = zext i1 " << value << " to i8\n";
        m_body << "  store i8 " << stored << ", ptr " << addr << "\n";
        return;
    }

    m_body << "  store " << llvmStoredType(t) << " " << value << ", ptr "
           << addr << "\n";
}

Codegen::Value Codegen::loadValueAtAddr(const Semantic::Type& t,
                                        const std::string& addr) {
    if (t.kind == Semantic::TypeKind::BOOL) {
        const std::string raw = freshValue();
        const std::string ssa = freshValue();
        m_body << "  " << raw << " = load i8, ptr " << addr << "\n";
        m_body << "  " << ssa << " = trunc i8 " << raw << " to i1\n";
        return {ssa, t};
    }

    const std::string ssa = freshValue();
    m_body << "  " << ssa << " = load " << llvmStoredType(t) << ", ptr " << addr
           << "\n";
    return {ssa, t};
}

bool Codegen::isSigned(Semantic::TypeKind k) {
    using TK = Semantic::TypeKind;
    return k == TK::INT8 || k == TK::INT16 || k == TK::INT32 || k == TK::INT64;
}

int Codegen::bitWidth(Semantic::TypeKind k) {
    using TK = Semantic::TypeKind;
    switch (k) {
        case TK::INT8:
        case TK::UINT8:
            return 8;
        case TK::INT16:
        case TK::UINT16:
            return 16;
        case TK::INT32:
        case TK::UINT32:
        case TK::FLOAT32:
        case TK::CHAR:
            return 32;
        case TK::INT64:
        case TK::UINT64:
        case TK::FLOAT64:
            return 64;
        case TK::BOOL:
            return 1;
        default:
            internalError("bit width requested for unsupported type");
    }
}

int Codegen::structFieldIndex(const Semantic::StructSymbol& s,
                              const std::string& field) {
    for (std::size_t i = 0; i < s.fields.size(); ++i) {
        if (s.fields[i].name == field) return static_cast<int>(i);
    }
    internalError("missing field '" + field + "' in struct " +
                  s.qualified_name);
}

const Semantic::StructSymbol* Codegen::findStructByQualifiedName(
    const std::string& qname) {
    auto found = m_typed.global_scope->findByPath(splitQualifiedName(qname));
    if (!found) return nullptr;
    if (const auto* structure =
            std::get_if<const Semantic::StructSymbol*>(&found.value())) {
        return *structure;
    }
    return nullptr;
}

const Semantic::FunctionSignature* Codegen::findSignatureForFunctionDecl(
    const std::vector<std::string>& ns, const Parser::FunctionDecl& fn) {
    const Semantic::Scope* scope = m_typed.global_scope.get();
    if (!ns.empty()) {
        auto found = scope->findByPath(ns);
        if (!found) return nullptr;
        const auto* ns_scope =
            std::get_if<const Semantic::Scope*>(&found.value());
        if (ns_scope == nullptr) return nullptr;
        scope = *ns_scope;
    }

    for (const auto* sig : scope->findFunctions(fn.name)) {
        if (sig->decl == &fn) return sig;
    }
    return nullptr;
}

const Semantic::FunctionSignature* Codegen::findSignatureForMethodDecl(
    const std::vector<std::string>& ns, const Parser::ImplDecl& impl,
    const Parser::FunctionDecl& method) {
    const Semantic::Scope* scope = m_typed.global_scope.get();
    if (!ns.empty()) {
        auto found = scope->findByPath(ns);
        if (!found) return nullptr;
        const auto* ns_scope =
            std::get_if<const Semantic::Scope*>(&found.value());
        if (ns_scope == nullptr) return nullptr;
        scope = *ns_scope;
    }

    const std::string key = impl.struct_name + "::" + method.name;
    for (const auto* sig : scope->findFunctions(key)) {
        if (sig->decl == &method) return sig;
    }
    return nullptr;
}

Semantic::Type Codegen::exprType(const Parser::Expr& e) {
    auto it = m_typed.expr_types.find(&e);
    if (it == m_typed.expr_types.end()) {
        internalError("missing expression type");
    }
    return it->second;
}

const Semantic::FunctionSignature* Codegen::callTarget(const void* call_node) {
    auto it = m_typed.call_targets.find(call_node);
    if (it == m_typed.call_targets.end()) {
        internalError("missing call target");
    }
    return it->second;
}

void Codegen::envPush() { m_env.emplace_back(); }

void Codegen::envPop() {
    if (m_env.empty()) internalError("env underflow");
    m_env.pop_back();
}

void Codegen::envDeclare(const std::string& name, std::string addr,
                         Semantic::Type type, const void* decl_node) {
    if (m_env.empty()) internalError("envDeclare without scope");
    m_local_addrs[decl_node] = addr;
    m_env.back()[name] = LocalSlot{std::move(addr), std::move(type)};
}

const Codegen::LocalSlot* Codegen::envLookup(const std::string& name) const {
    for (auto it = m_env.rbegin(); it != m_env.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

}  // namespace Codegen
