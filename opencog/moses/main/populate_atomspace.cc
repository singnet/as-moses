#include <opencog/data/table/table.h>
#include <opencog/data/table/table_io.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/utils/value_key.h>
#include <opencog/atoms/base/Link.h>
#include "populate_atomspace.h"

namespace opencog { namespace moses {

void populate(AtomSpace *as, const ITable &itable) {

	int n_columns = itable.get_types().size();
	for (int i = 0; i < n_columns; i++) {
		id::type_node col_type = itable.get_types().at(i);
		vertex_seq col = itable.get_column_data(i);
		Handle feature = createNode(col_type == id::boolean_type? PREDICATE_NODE : SCHEMA_NODE, itable.get_labels().at(i));
		ValuePtr vtr = vertex_seq_to_value(col, col_type);
		feature->setValue(value_key, vtr);
		as->add_atom(feature);
	}
}

void populate(AtomSpace *as, const CTable &ctable)
{
	const string_seq &labels = ctable.get_input_labels();
	const type_tree_seq &types = get_signature_inputs(ctable.get_signature());
	for (int j = 0; j < labels.size(); ++j) {
		const vertex_seq &col = ctable.get_input_col_data(j);
		type_node col_type = get_type_node(types[j]);
		const Handle &feature = createNode(col_type == id::boolean_type? PREDICATE_NODE : SCHEMA_NODE,labels[j] );
		const ValuePtr &vtr = vertex_seq_to_value(col, get_type_node(types[j]));
		feature->setValue(compressed_value_key, vtr);
		as->add_atom(feature);
	}
}



ValuePtr vertex_seq_to_value(const vertex_seq& col, id::type_node col_type) {

	int n_rows = col.size();
	switch (col_type) {
		case id::boolean_type: {
			ValueSeq col_values = {};
			for (int j = 0; j < n_rows; j++) {
				bool col_data = vertex_to_bool(col.at(j));
				col_values.push_back(ValuePtr(createLink(col_data ? TRUE_LINK : FALSE_LINK)));
			}
			return ValuePtr(new LinkValue(col_values));
		}
		case id::contin_type: {
			std::vector<double> col_values_contin = {};
			for (int j = 0; j < n_rows; j++)
				col_values_contin.push_back(get_contin(col.at(j)));
			return ValuePtr(new FloatValue(col_values_contin));
		}
		case id::enum_type: {
			//TODO enum_type data to be added to Atomspace
			break;
		}
		default: {
			std::stringstream ss;
			ss << col_type;
			throw ComboException(TRACE_INFO,
			                     "vertex_seq_to_value can not handle type_node %s",
			                     ss.str().c_str());
		}
	}

	// Silence compiler warning
	return nullptr;
}

}
}
