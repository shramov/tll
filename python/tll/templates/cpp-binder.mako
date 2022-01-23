<%def name='setup_options(parser)'><%
    parser.add_argument('--namespace', dest='namespace', type=str,
                        help='C++ namespace name for generated source')
%></%def>\
<%!
from tll.templates import cpp
%>\
#include <tll/scheme/binder.h>
% if options.namespace:

namespace ${options.namespace} {
% endif

static constexpr std::string_view scheme_string = R"(${scheme.dump('yamls+gz')})";
<%
def field2scalar(f):
    t = cpp.numeric(f.type)
    if t is not None:
        if f.sub_type == f.Sub.Bits:
            #return f"tll::scheme::Bits<{t}>"
            return t
        elif f.sub_type == f.Sub.Enum:
            return f.type_enum.name
        elif f.sub_type == f.Sub.FixedPoint:
            return f"tll::scheme::FixedPoint<{t}, {f.fixed_precision}>";
        elif f.sub_type == f.Sub.Duration:
            return f"std::chrono::duration<{t}, {cpp.time_resolution(f)}>";
        elif f.sub_type == f.Sub.TimePoint:
            return f"std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<{t}, {cpp.time_resolution(f)}>>";
        return t
    elif f.type == f.Decimal128:
        return "tll::scheme::Decimal128"
    return None

def field2type(f):
    t = field2scalar(f)
    if t is not None:
	return t, "scalar"
    elif f.type == f.Message:
    	return f"{f.type_msg.name}<Buf>", "builder"
    elif f.type == f.Bytes:
	if f.sub_type == f.Sub.ByteString:
	    return f"tll::scheme::ByteString<{f.size}>", "bytestring"
	return f"tll::scheme::Bytes<{f.size}", "bytes"
    elif f.type == f.Array:
    	raise RuntimeError("Array not supported")
    elif f.type == f.Pointer:
	if f.sub_type == f.Sub.ByteString:
	    return f"tll::scheme::binder::String<Buf, {cpp.offset_ptr_version(f)}>", "string"
	t, m = field2type(f.type_ptr)
	return f"""tll::scheme::binder::List<Buf, {t}, {cpp.offset_ptr_version(f)}>""", "builder"
    raise ValueError(f"Unknown type for field {f.name}: {f.type}")

def field2code(f):
    t, m = field2type(f)
    if m == 'scalar':
        return cpp.indent("\t",
f"""using type_{f.name} = {t};
type_{f.name} get_{f.name}() const {{ return this->template _get_scalar<type_{f.name}>({f.offset}); }}
void set_{f.name}(type_{f.name} v) {{ return this->template _set_scalar<type_{f.name}>({f.offset}, v); }}""")
    elif m == 'bytes':
        return cpp.indent("\t",
f"""const {t} & get_{f.name}() const {{ return this->template _get_bytes<{f.size}>({f.offset}); }}
void set_{f.name}(const {t} &v) const {{ return this->template _set_bytes<{f.size}>({f.offset}, {{v.data(), v.size()}}); }}
void set_{f.name}(std::string_view v) const {{ return this->template _set_bytestring<{f.size}>({f.offset}, v); }}""")
    elif m == 'bytestring':
	return cpp.indent("\t",
f"""std::string_view get_{f.name}() const {{ return this->template _get_bytestring<{f.size}>({f.offset}); }}
void set_{f.name}(std::string_view v) const {{ return this->template _set_bytestring<{f.size}>({f.offset}, v); }}""")
    elif m == 'string':
	return cpp.indent("\t",
f"""std::string_view get_{f.name}() const {{ return this->template _get_string<{cpp.offset_ptr_version(f)}>({f.offset}); }}
void set_{f.name}(std::string_view v) const {{ return this->template _set_string<{cpp.offset_ptr_version(f)}>({f.offset}, v); }}""")
    elif m == 'builder':
        return cpp.indent("\t",
f"""using type_{f.name} = {t};
const type_{f.name} get_{f.name}() const {{ return this->template _get_binder<type_{f.name}>({f.offset}); }}
type_{f.name} get_{f.name}() {{ return this->template _get_binder<type_{f.name}>({f.offset}); }}""")
%>\
% for e in scheme.enums.values():

${cpp.declare_enum(e)}
% endfor
% for msg in scheme.messages:

template <typename Buf>
struct ${msg.name} : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return ${msg.size}; }
	static constexpr std::string_view meta_name() { return "${msg.name}"; }
% if msg.msgid:
	static constexpr int meta_id() { return ${msg.msgid}; }
% endif
% for e in msg.enums.values():

${cpp.indent("\t", cpp.declare_enum(e))}
% endfor
% for f in msg.fields:

${field2code(f)}
% endfor
};
% endfor
% if options.namespace:

} // namespace ${options.namespace}
% endif
