/*
 * asmoses/opencog/atomese/atom_types/NewLink.cc
 *
 * Copyright (C) 2019 Bitseat Tadesse.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the
 * exceptions at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <opencog/atoms/base/ClassServer.h>
#include <opencog/atoms/execution/EvaluationLink.h>

#include "NewLink.h"

using namespace opencog;

NewLink::NewLink(const HandleSeq &oset, Type t)
        : FunctionLink(oset, t) {
    // Type must be as expected
    if (not nameserver().isA(t, NEW_LINK)) {
        const std::string &tname = nameserver().getTypeName(t);
        throw SyntaxException(TRACE_INFO,
                              "Expecting a NewLink, got %s", tname.c_str());
    }

    if (2 != oset.size())
        throw SyntaxException(TRACE_INFO,
                              "NewLink expects two arguments.");

}

ValuePtr NewLink::execute(AtomSpace *scratch, bool silent) {
    Type t;

    std::string linktype = _outgoing.at(0)->get_name();
    if (linktype == "AndLink") {
        t = AND_LINK;
    }
    if (linktype == "OrLink") {
        t = OR_LINK;
    }
    Handle handle = createLink(_outgoing.at(1)->getOutgoingSet(), t);
    linktype = _outgoing.at(0)->get_name();
    return handle;
}

DEFINE_LINK_FACTORY(NewLink, NEW_LINK)
