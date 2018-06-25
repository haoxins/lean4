/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include <algorithm>
#include <string>
#include <limits>
#include "util/rc.h"
#include "kernel/expr.h"

namespace lean {
/**
inductive reducibility_hints
| opaque  : reducibility_hints
| abbrev  : reducibility_hints
| regular : nat → reducibility_hints

Reducibility hints are used in the convertibility checker (aka is_def_eq predicate),
whenever checking a constraint such as

           (f ...) =?= (g ...)

where f and g are definitions, and the checker has to decide which one will be unfolded.
If f (g) is Opaque,             then g (f) is unfolded if it is also not marked as Opaque.
Else if f (g) is Abbreviation,  then f (g) is unfolded if g (f) is also not marked as Abbreviation.
Else if f and g are Regular,    then we unfold the one with the biggest definitional height.
Otherwise unfold both.

The definitional height is by default computed by the kernel. It only takes into account
other Regular definitions used in a definition.

Remark: the hint only affects performance. */
enum class reducibility_hints_kind { Opaque, Abbreviation, Regular };
class reducibility_hints : public object_ref {
    explicit reducibility_hints(object * r):object_ref(r) {}
public:
    static reducibility_hints mk_opaque() { return reducibility_hints(box(static_cast<unsigned>(reducibility_hints_kind::Opaque))); }
    static reducibility_hints mk_abbreviation() { return reducibility_hints(box(static_cast<unsigned>(reducibility_hints_kind::Abbreviation))); }
    static reducibility_hints mk_regular(unsigned h) {
        object * r = alloc_cnstr(static_cast<unsigned>(reducibility_hints_kind::Regular), 0, sizeof(unsigned));
        cnstr_set_scalar<unsigned>(r, 0, h);
        return reducibility_hints(r);
    }
    reducibility_hints_kind kind() const { return static_cast<reducibility_hints_kind>(obj_tag(raw())); }
    bool is_regular() const { return kind() == reducibility_hints_kind::Regular; }
    unsigned get_height() const { return is_regular() ? cnstr_scalar<unsigned>(raw(), 0) : 0; }
    void serialize(serializer & s) const { s.write_object(raw()); }
    static reducibility_hints deserialize(deserializer & d) { return reducibility_hints(d.read_object()); }
};

inline serializer & operator<<(serializer & s, reducibility_hints const & l) { l.serialize(s); return s; }
inline reducibility_hints read_reducibility_hints(deserializer & d) { return reducibility_hints::deserialize(d); }
inline deserializer & operator>>(deserializer & d, reducibility_hints & l) { l = read_reducibility_hints(d); return d; }

/** Given h1 and h2 the hints for definitions f1 and f2, then
    result is
    <  0 If f1 should be unfolded
    == 0 If f1 and f2 should be unfolded
    >  0 If f2 should be unfolded */
int compare(reducibility_hints const & h1, reducibility_hints const & h2);

/** \brief Environment definitions, theorems, axioms and variable declarations. */
class declaration {
    struct cell {
        MK_LEAN_RC();
        name               m_name;
        level_param_names  m_params;
        expr               m_type;
        bool               m_theorem;
        optional<expr>     m_value;        // if none, then declaration is actually a postulate
        reducibility_hints m_hints;
        /* Definitions are non-meta by default. We use this feature to define tactical-definitions. */
        bool               m_meta;
        void dealloc() { delete this; }

        cell(name const & n, level_param_names const & params, expr const & t, bool is_axiom, bool meta):
            m_rc(1), m_name(n), m_params(params), m_type(t), m_theorem(is_axiom),
            m_hints(reducibility_hints::mk_opaque()), m_meta(meta) {}
        cell(name const & n, level_param_names const & params, expr const & t, expr const & v,
             reducibility_hints const & h, bool meta):
            m_rc(1), m_name(n), m_params(params), m_type(t), m_theorem(false),
            m_value(v), m_hints(h), m_meta(meta) {}
        cell(name const & n, level_param_names const & params, expr const & t, expr const & v):
            m_rc(1), m_name(n), m_params(params), m_type(t), m_theorem(true),
            m_value(v), m_hints(reducibility_hints::mk_opaque()), m_meta(false) {}
    };
    cell * m_ptr;
    explicit declaration(cell * ptr);
    friend struct cell;
public:
    /**
       \brief The default constructor creates a reference to a "dummy"
       declaration.  The actual "dummy" declaration is not relevant, and
       no procedure should rely on the kind of declaration used.

       We have a default constructor because some collections only work
       with types that have a default constructor.
    */
    declaration();
    declaration(declaration const & s);
    declaration(declaration && s);
    ~declaration();

    friend void swap(declaration & a, declaration & b) { std::swap(a.m_ptr, b.m_ptr); }

    declaration & operator=(declaration const & s);
    declaration & operator=(declaration && s);

    friend bool is_eqp(declaration const & d1, declaration const & d2) { return d1.m_ptr == d2.m_ptr; }

    bool is_definition() const;
    bool is_axiom() const;
    bool is_theorem() const;
    bool is_constant_assumption() const;

    bool is_meta() const;

    name const & get_name() const;
    level_param_names const & get_univ_params() const;
    unsigned get_num_univ_params() const;
    expr const & get_type() const;
    expr const & get_value() const;

    reducibility_hints const & get_hints() const;

    friend declaration mk_definition(name const & n, level_param_names const & params, expr const & t, expr const & v,
                                     reducibility_hints const & hints, bool meta);
    friend declaration mk_definition(environment const & env, name const & n, level_param_names const & params, expr const & t,
                                     expr const & v, bool meta);
    friend declaration mk_theorem(name const &, level_param_names const &, expr const &, expr const &);
    friend declaration mk_axiom(name const & n, level_param_names const & params, expr const & t);
    friend declaration mk_constant_assumption(name const & n, level_param_names const & params, expr const & t, bool meta);
};

inline optional<declaration> none_declaration() { return optional<declaration>(); }
inline optional<declaration> some_declaration(declaration const & o) { return optional<declaration>(o); }
inline optional<declaration> some_declaration(declaration && o) { return optional<declaration>(std::forward<declaration>(o)); }

declaration mk_definition(name const & n, level_param_names const & params, expr const & t, expr const & v,
                          reducibility_hints const & hints, bool meta = false);
declaration mk_definition(environment const & env, name const & n, level_param_names const & params, expr const & t, expr const & v,
                          bool meta = false);
declaration mk_theorem(name const & n, level_param_names const & params, expr const & t, expr const & v);
declaration mk_theorem(name const & n, level_param_names const & params, expr const & t, expr const & v);
declaration mk_axiom(name const & n, level_param_names const & params, expr const & t);
declaration mk_constant_assumption(name const & n, level_param_names const & params, expr const & t, bool meta = false);

/** \brief Return true iff \c e depends on meta-declarations */
bool use_meta(environment const & env, expr const & e);

/** \brief Similar to mk_definition but infer the value of meta flag.
    That is, set it to true if \c t or \c v contains a meta declaration. */
declaration mk_definition_inferring_meta(environment const & env, name const & n, level_param_names const & params,
                                         expr const & t, expr const & v, reducibility_hints const & hints);
declaration mk_definition_inferring_meta(environment const & env, name const & n, level_param_names const & params,
                                         expr const & t, expr const & v);
/** \brief Similar to mk_constant_assumption but infer the value of meta flag.
    That is, set it to true if \c t or \c v contains a meta declaration. */
declaration mk_constant_assumption_inferring_meta(environment const & env, name const & n,
                                                  level_param_names const & params, expr const & t);

void initialize_declaration();
void finalize_declaration();
}
