/*
 * asmoses/opencog/atomese/atom_types/NewLink.h
 *
 * Copyright (C) 2019 Bitseat Tadesse
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ATOMSPACE_NEWLINK_H
#define ATOMSPACE_NEWLINK_H

#include <opencog/atoms/core/FunctionLink.h>
#include <opencog/atoms/core/ScopeLink.h>
#include <opencog/atoms/core/Quotation.h>
#include <opencog/atoms/atom_types/atom_types.h>

namespace opencog {
    class NewLink : public FunctionLink {
    public:
        NewLink(const HandleSeq &, Type= NEW_LINK);

        virtual ValuePtr execute(AtomSpace *, bool);

        static Handle factory(const Handle &);

        void init(void);
    };

    typedef std::shared_ptr<NewLink> NewLinkPtr;

    static inline NewLinkPtr NewLinkCast(const Handle &h) {
        AtomPtr a(h);
        return std::dynamic_pointer_cast<NewLink>(a);
    }

    static inline NewLinkPtr NewLinkCast(AtomPtr a) { return std::dynamic_pointer_cast<NewLink>(a); }

#define createNewLink std::make_shared<NewLink>

/** @}*/
}

#endif //ATOMSPACE_NEWLINK_H
