/** program_interpreter.h ---
 *
 * Copyright (C) 2018 OpenCog Foundation
 *
 * Authors: Kasim Ebrahim <se.kasim.ebrahim@gmail.com>
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


#ifndef MOSES_INTERPRETER_H
#define MOSES_INTERPRETER_H

#include <opencog/atoms/base/Handle.h>

/**
 * class Interpreter -- create an unfolded program from compressed program
 *
 * Given a program(i:e (Plus (Schema i1) (Schema i2))) that operates on features
 * of problem data[schema or predicate], this class will create unfolded program
 * that operate on observations of the features (i1 and i2).
 * */
namespace opencog
{
namespace atomese
{

class Interpreter
{

	opencog::Handle _problem_data;
	std::vector<double >::size_type _problem_data_size;

public:
	Interpreter(const opencog::Handle &input_table);

/**
 * executes/evaluates a program over a problem data.
 *
 * @param Handle&       handle ref, stores the atomese program
 * @return               return ProtoAtomPtr containing the output
 *                       of the program executed/evaluated on the problem data.
 */
	opencog::ProtoAtomPtr interpret(const opencog::Handle& program);

private:
	ProtoAtomPtr unwrap_node(const Handle& handle);

	ProtoAtomPtr execute(const Type t, const ProtomSeq &params);
};

}
}


#endif //MOSES_INTERPRETER_H