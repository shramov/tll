<%def name='setup_options(parser)'><%
    parser.add_argument('--namespace', dest='namespace', type=str, default=None,
                        help='C++ namespace name for generated source')
%></%def>\
<%!
from tll.templates import cpp
%>\
<%
if options.namespace is None:
    options.namespace = scheme.options.get('cpp-namespace', '')
%>\
#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>
% if options.namespace:

namespace ${options.namespace} {
% endif

static constexpr std::string_view scheme_string = R"(${scheme.dump('yamls+gz')})";
<%
def field2scalar(f):
    t = cpp.numeric(f.type)
    if t is not None:
        if f.sub_type == f.Sub.Bits:
            return f.type_bits.name
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
        return f"{f.type_msg.name}::binder_type<Buf>", "builder"
    elif f.type == f.Union:
        return f"{f.type_union.name}<Buf>", "builder"
    elif f.type == f.Bytes:
        if f.sub_type == f.Sub.ByteString:
            return f"tll::scheme::ByteString<{f.size}>", "bytestring"
        return f"tll::scheme::Bytes<{f.size}>", "bytes"
    elif f.type == f.Array:
        raise RuntimeError("Array not supported")
    elif f.type == f.Pointer:
        if f.sub_type == f.Sub.ByteString:
            return f"tll::scheme::binder::String<Buf, {cpp.offset_ptr_version(f)}>", "string"
        t, m = field2type(f.type_ptr)
        return f"""tll::scheme::binder::List<Buf, {t}, {cpp.offset_ptr_version(f)}>""", "builder"
    raise ValueError(f"Unknown type for field {f.name}: {f.type}")

def field2code(msg, f):
    t, m = field2type(f)
    pmap, suffix = "", ""
    if msg.pmap and f.optional:
        pmap = f"_pmap_set({f.index}); "
        suffix = f"""
bool has_{f.name}() const {{ return _pmap_get({f.index}); }}"""
    if m == 'scalar':
        return cpp.indent("\t\t",
f"""using type_{f.name} = {t};
type_{f.name} get_{f.name}() const {{ return this->template _get_scalar<type_{f.name}>(offset_{f.name}); }}
void set_{f.name}(type_{f.name} v) {{ {pmap}return this->template _set_scalar<type_{f.name}>(offset_{f.name}, v); }}""" + suffix)
    elif m == 'bytes':
        return cpp.indent("\t\t",
f"""const {t} & get_{f.name}() const {{ return this->template _get_bytes<{f.size}>(offset_{f.name}); }}
void set_{f.name}(const {t} &v) const {{ {pmap}return this->template _set_bytes<{f.size}>(offset_{f.name}, {{v.data(), v.size()}}); }}
void set_{f.name}(std::string_view v) {{ {pmap}return this->template _set_bytestring<{f.size}>(offset_{f.name}, v); }}""" + suffix)
    elif m == 'bytestring':
        return cpp.indent("\t\t",
f"""std::string_view get_{f.name}() const {{ return this->template _get_bytestring<{f.size}>(offset_{f.name}); }}
void set_{f.name}(std::string_view v) {{ {pmap}return this->template _set_bytestring<{f.size}>(offset_{f.name}, v); }}""" + suffix)
    elif m == 'string':
        return cpp.indent("\t\t",
f"""std::string_view get_{f.name}() const {{ return this->template _get_string<{cpp.offset_ptr_version(f)}>(offset_{f.name}); }}
void set_{f.name}(std::string_view v) {{ {pmap}return this->template _set_string<{cpp.offset_ptr_version(f)}>(offset_{f.name}, v); }}""" + suffix)
    elif m == 'builder':
        return cpp.indent("\t\t",
f"""using type_{f.name} = {t};
using const_type_{f.name} = {t.replace("<Buf", "<const Buf")};
const_type_{f.name} get_{f.name}() const {{ return this->template _get_binder<const_type_{f.name}>(offset_{f.name}); }}
type_{f.name} get_{f.name}() {{ return this->template _get_binder<type_{f.name}>(offset_{f.name}); }}""" + suffix)

def copy_available(msg):
    for f in msg.fields:
        _, tp = field2type(f)
        if tp in ('scalar', 'string', 'bytestring', 'bytes'):
	    pass
        elif f.type == f.Message:
            if not copy_available(f.type_msg):
                return False
        else:
            return False
    return True

def field2copy(f):
    _, tp = field2type(f)
    if tp in ('scalar', 'string', 'bytestring'):
        return f"set_{f.name}(rhs.get_{f.name}());"
    else:
        return f"get_{f.name}().copy(rhs.get_{f.name}());"

%>\
<%def name='union2decl_inner(u)' filter='cpp.indent_filter'><%call expr='union2decl(u)'></%call></%def>\
<%def name='union2decl(u)'>\

template <typename Buf>
struct ${u.name}: public tll::scheme::binder::Union<Buf, ${cpp.numeric(u.type_ptr.type)}>
{
	using union_index_type = ${cpp.numeric(u.type_ptr.type)};
	using tll::scheme::binder::Union<Buf, union_index_type>::Union;
% for f in u.fields:

<% (t, m) = field2type(f) %>\
	static constexpr union_index_type index_${f.name} = ${f.union_index};
	using type_${f.name} = ${t};
% if m == "string" or m == "bytestring":
	std::optional<std::string_view> get_${f.name}() const { if (this->union_type() != index_${f.name}) return std::nullopt; return unchecked_${f.name}(); }
% else:
	std::optional<${t}> get_${f.name}() const { if (this->union_type() != index_${f.name}) return std::nullopt; return unchecked_${f.name}(); }
% endif
% if m == "builder":
	${t} unchecked_${f.name}() { return this->template _get_binder<${t}>(${f.offset}); }
	${t} unchecked_${f.name}() const { return this->template _get_binder<${t}>(${f.offset}); }
	${t} set_${f.name}() { this->_set_type(index_${f.name}); return this->template _get_binder<${t}>(${f.offset}); }
% elif m == 'bytes':
	${t} unchecked_${f.name}() const { return this->template _get_bytes<${f.size}>(${f.offset}); }
	void set_${f.name}(const ${t} &v) const { this->_set_type(index_${f.name}); return this->template _set_bytes<${f.size}>(${f.offset}, {v.data(), v.size()}); }
	void set_${f.name}(std::string_view v) { this->_set_type(index_${f.name}); return this->template _set_bytestring<${f.size}>(${f.offset}, v); }
% elif m == 'bytestring':
	std::string_view unchecked_${f.name}() const { return this->template _get_bytestring<${f.size}>(${f.offset}); }
	void set_${f.name}(std::string_view v) { this->_set_type(index_${f.name}); this->template _set_bytestring<${f.size}>(${f.offset}, v); }
% elif m == 'string':
	std::string_view unchecked_${f.name}() const { return this->template _get_string<${cpp.offset_ptr_version(f)}>(${f.offset}); }
	void set_${f.name}(std::string_view v) { this->_set_type(index_${f.name}); this->template _set_string<${cpp.offset_ptr_version(f)}>(${f.offset}, v); }
% else:
	${t} unchecked_${f.name}() const { return this->template _get_scalar<${t}>(${f.offset}); }
	void set_${f.name}(const ${t} &v) { this->_set_type(index_${f.name}); this->template _set_scalar<${t}>(${f.offset}, v); }
% endif
% endfor
};
</%def>\
<%def name='enum2dump(prefix, e)'>\

template <>
struct tll::conv::dump<${prefix}${e.name}> : public to_string_from_string_buf<${prefix}${e.name}>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const ${prefix}${e.name} &v, Buf &buf)
	{
		switch (v) {
% for v in sorted(e.keys()):
		case ${prefix}${e.name}::${v}: return "${v}";
% endfor
		default: break;
		}
		return tll::conv::to_string_buf<${cpp.numeric(e.type)}, Buf>((${cpp.numeric(e.type)}) v, buf);
	}
};
</%def>\
% for e in scheme.enums.values():

${cpp.declare_enum(e)}
% endfor
% for u in scheme.unions.values():
<%call expr='union2decl(u)'></%call>
% endfor
% for b in scheme.bits.values():

${cpp.declare_bits(b)}
% endfor
% for msg in scheme.messages:

struct ${msg.name}
{
	static constexpr size_t meta_size() { return ${msg.size}; }
	static constexpr std::string_view meta_name() { return "${msg.name}"; }
% if msg.msgid:
	static constexpr int meta_id() { return ${msg.msgid}; }
% endif
% for f in msg.fields:
	static constexpr size_t offset_${f.name} = ${f.offset};
% endfor
% for e in msg.enums.values():

${cpp.indent("\t", cpp.declare_enum(e))}
% endfor
% for u in msg.unions.values():
<%call expr='union2decl_inner(u)'></%call>
% endfor
% for b in msg.bits.values():

${cpp.indent("\t", cpp.declare_bits(b))}
% endfor

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ${msg.name}::meta_size(); }
		static constexpr auto meta_name() { return ${msg.name}::meta_name(); }
% if msg.msgid:
		static constexpr auto meta_id() { return ${msg.name}::meta_id(); }
% endif
		void view_resize() { this->_view_resize(meta_size()); }
% if msg.pmap:
		bool _pmap_get(int index) const { return tll::scheme::pmap_get(this->view().view(${msg.pmap.offset}).data(), index); }
		void _pmap_set(int index) { tll::scheme::pmap_set(this->view().view(${msg.pmap.offset}).data(), index); }
% endif
% if copy_available(msg):
		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
% for f in msg.fields:
			${field2copy(f)}
% endfor
		}
% endif
% for f in msg.fields:

${field2code(msg, f)}
% endfor
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};
% endfor
% if options.namespace:

} // namespace ${options.namespace}
% endif
% for e in scheme.enums.values():
${enum2dump('::'.join(([options.namespace] if options.namespace else []) + ['']), e)}\
% endfor
% for msg in scheme.messages:
% for e in msg.enums.values():
${enum2dump('::'.join(([options.namespace] if options.namespace else []) + [msg.name, '']), e)}\
% endfor
% endfor
