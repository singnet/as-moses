/** table.h ---
 *
 * Copyright (C) 2010 OpenCog Foundation
 * Copyright (C) 2012 Poulin Holdings LLC
 * Copyright (C) 2014 Aidyia Limited
 *
 * Author: Nil Geisweiller <ngeiswei@gmail.com>
 * Additions and tweaks, Linas Vepstas <linasvepstas@gmail.com>
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


#ifndef _OPENCOG_TABLE_H
#define _OPENCOG_TABLE_H

#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm/adjacent_find.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/operators.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <opencog/util/algorithm.h>
#include <opencog/util/Counter.h>
#include <opencog/util/dorepeat.h>
#include <opencog/util/exceptions.h>
#include <opencog/util/KLD.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/value/LinkValue.h>

#include <opencog/combo/type_checker/type_tree.h>
#include <opencog/combo/interpreter/eval.h>
#include <opencog/combo/interpreter/interpreter.h>
#include <opencog/combo/combo/vertex.h>
#include <opencog/combo/combo/common_def.h>

#define COEF_SAMPLE_COUNT 20.0 // involved in the formula that counts
// the number of trials needed to check
// a formula

#define TARGET_DISCRETIZED_BINS_NUM 5  // discretize contin type target
// into # bins

namespace opencog
{
namespace combo
{

std::vector<contin_t> discretize_contin_feature(contin_t min, contin_t max);

// builtin is not used to represent builtins but to trick vertex to
// represent discretized contin values
builtin get_discrete_bin(std::vector<contin_t> disc_intvs, contin_t val);

/**
 * Get indices (aka positions or offsets) of a list of labels given a
 * header. The labels can be sequenced in any order, it will always
 * return the order consistent with the header.
 */
std::vector<unsigned> get_indices(const std::vector<std::string> &labels,
                                  const std::vector<std::string> &header);

///////////////////
// Generic table //
///////////////////

///////////////////////////////////
// Collection of useful visitors //
///////////////////////////////////
//
// The below is sort of weird, and may seeem wrong, and thus requires
// some explanation. The visitors here are implemented in such a way
// that if all features are the same (i.e. all elements in a row are
// the same), then the visitor gets used.  This seems awfully backwards,
// as normally, it is all of the samples in a column of the table that
// all have exactly the same type, and not the rows!
//
// Notation: rows correspond to samples, columns are features.
//
// Anyway: this somewhat oddball visitor implementation was done to
// save system RAM, because vector<variant<T1, ..., Tn>> takes a lot
// more RAM than variant<vector<T1>, ..., vector<Tn>>.  In that sense,
// this implementation "works". Unfortunately, its flawed, if any one
// of the columns has a different type than the others.  Someday, the
// design here should be changed, so that the space-savings is still
// realized, while also allowing different types for different columns.
// XXX FIXME TODO: change the implementation, per the above note.

typedef std::vector<builtin> builtin_seq;
typedef std::vector<contin_t> contin_seq;
typedef std::vector<std::string> string_seq;

// Push back to a multi_type_seq
template<typename T /* type being pushed */>
struct push_back_visitor : public boost::static_visitor<>
{
	push_back_visitor(const T &value) : _value(value)
	{}

	void operator()(std::vector<T> &seq) const
	{
		seq.push_back(_value);
	}

	void operator()(vertex_seq &seq) const
	{
		seq.push_back(_value);
	}

	template<typename Seq>
	void operator()(Seq &seq) const
	{
		std::stringstream ss;
		ss << "You can't push_back " << _value << " in container ";
		ostream_container(ss, seq);
		OC_ASSERT(false, ss.str());
	}

	const T &_value;
};

struct pop_back_visitor : public boost::static_visitor<>
{
	template<typename Seq>
	void operator()(Seq &seq) const
	{
		seq.pop_back();
	}
};

/**
 * Replace the value of multi_type_seq at pos by its initial value
 */
struct init_at_visitor : public boost::static_visitor<>
{
	init_at_visitor(size_t pos) : _pos(pos)
	{}

	template<typename Seq>
	void operator()(Seq &seq) const
	{
		typedef typename Seq::value_type vt;
		seq[_pos] = vt();
	}

	size_t _pos;
};

template<typename T>
struct get_at_visitor : public boost::static_visitor<T>
{
	get_at_visitor(size_t pos) : _pos(pos)
	{}

	T operator()(const std::vector<T> &seq) const
	{
		return seq[_pos];
	}

	T operator()(const vertex_seq &seq) const
	{
		return boost::get<T>(seq[_pos]);
	}

	T operator()(const combo_tree_seq &seq) const
	{
		return boost::get<T>(*seq[_pos].begin());
	}

	template<typename Seq>
	T operator()(const Seq &seq) const
	{
		OC_ASSERT(false, "Impossible operation");
		return T();
	}

	size_t _pos;
};

template<>
struct get_at_visitor<vertex> : public boost::static_visitor<vertex>
{
	get_at_visitor(size_t pos) : _pos(pos)
	{}

	vertex operator()(const combo_tree_seq &seq) const
	{
		return *seq[_pos].begin();
	}

	template<typename Seq>
	vertex operator()(const Seq &seq) const
	{
		return seq[_pos];
	}

	size_t _pos;
};

template<>
struct get_at_visitor<combo_tree> : public boost::static_visitor<combo_tree>
{
	get_at_visitor(size_t pos) : _pos(pos)
	{}

	template<typename Seq>
	combo_tree operator()(const Seq &seq) const
	{
		return seq[_pos];
	}

	size_t _pos;
};

struct erase_at_visitor : public boost::static_visitor<>
{
	erase_at_visitor(size_t pos) : _pos(pos)
	{}

	template<typename Seq>
	void operator()(Seq &seq) const
	{
		seq.erase(seq.begin() + _pos);
	}

	size_t _pos;
};

template<typename T>
struct insert_at_visitor : public boost::static_visitor<>
{
	// if pos is negative then it inserts at the end
	insert_at_visitor(int pos, const T v) : _pos(pos), _v(v)
	{}

	void operator()(std::vector<T> &seq) const
	{
		seq.insert(_pos >= 0 ? seq.begin() + _pos : seq.end(), _v);
	}

	template<typename Seq>
	void operator()(Seq &seq) const
	{
		std::stringstream ss;
		ss << "You can't insert " << _v << " at " << _pos << " in container ";
		ostream_container(ss, seq);
		OC_ASSERT(false, ss.str());
	}

	int _pos;
	const T &_v;
};

struct size_visitor : public boost::static_visitor<size_t>
{
	template<typename Seq>
	size_t operator()(const Seq &seq)
	{
		return seq.size();
	}
};

struct empty_visitor : public boost::static_visitor<bool>
{
	template<typename Seq>
	bool operator()(const Seq &seq)
	{
		return seq.empty();
	}
};

/**
 * Allows to compare vertex_vec with vectors of different types.
 */
struct equal_visitor : public boost::static_visitor<bool>
{
#define __FALSE_EQ__(seql_t, seqr_t)                          \
    bool operator()(const seql_t& l, const seqr_t& r) const { \
        return false;                                         \
    }

	__FALSE_EQ__(builtin_seq, contin_seq);

	__FALSE_EQ__(builtin_seq, string_seq);

	__FALSE_EQ__(builtin_seq, combo_tree_seq);

	__FALSE_EQ__(contin_seq, builtin_seq);

	__FALSE_EQ__(contin_seq, string_seq);

	__FALSE_EQ__(contin_seq, combo_tree_seq);

	__FALSE_EQ__(string_seq, builtin_seq);

	__FALSE_EQ__(string_seq, contin_seq);

	__FALSE_EQ__(string_seq, combo_tree_seq);

	__FALSE_EQ__(combo_tree_seq, builtin_seq);

	__FALSE_EQ__(combo_tree_seq, contin_seq);

	__FALSE_EQ__(combo_tree_seq, string_seq);

	__FALSE_EQ__(combo_tree_seq, vertex_seq);

	__FALSE_EQ__(vertex_seq, combo_tree_seq);
#undef __FALSE_EQ__

	template<typename SeqL, typename SeqR>
	bool operator()(const SeqL &l, const SeqR &r) const
	{
		return boost::equal(l, r);
	}
};

// function specifically for output table
std::string table_fmt_vertex_to_str(const vertex &v);

std::string table_fmt_builtin_to_str(const builtin &b);

struct to_strings_visitor : public boost::static_visitor<string_seq>
{
	string_seq operator()(const string_seq &seq)
	{
		return seq;
	}

	string_seq operator()(const vertex_seq &seq)
	{
		string_seq res;
		boost::transform(seq, back_inserter(res), table_fmt_vertex_to_str);
		return res;
	}

	string_seq operator()(const builtin_seq &seq)
	{
		string_seq res;
		boost::transform(seq, back_inserter(res), table_fmt_builtin_to_str);
		return res;
	}

	template<typename Seq>
	string_seq operator()(const Seq &seq)
	{
		string_seq res;
		boost::transform(seq, back_inserter(res),
		                 [](const typename Seq::value_type &v) {
			                 std::stringstream ss;
			                 ss << v;
			                 return ss.str();
		                 });
		return res;
	}
};

struct get_type_tree_at_visitor : public boost::static_visitor<type_tree>
{
	get_type_tree_at_visitor(size_t pos) : _pos(pos)
	{}

	template<typename Seq>
	type_tree operator()(const Seq &seq)
	{
		return get_type_tree(seq[_pos]);
	}

	size_t _pos;
};

/**
 * Interpreter visitor.
 * Chooses the interpreter type, based on the type of vertexes appearing
 * in the combo tree.
 */
struct interpreter_visitor : public boost::static_visitor<vertex>
{
	interpreter_visitor(const combo_tree &tr) : _it(tr.begin())
	{
		// If any of the vertexes in the tree are contin-type,
		// then the mixed interpreter will have to be used.
		mixed = false;
		combo_tree::iterator mit = tr.begin();
		combo_tree::iterator mend = tr.end();
		for (; mit != mend; ++mit) {
			mixed = is_contin_expr(*mit);
			if (mixed) break;
			mixed = (id::greater_than_zero == *mit);
			if (mixed) break;
		}
	}

	interpreter_visitor(const combo_tree::iterator &it) : _it(it)
	{
		// Cannot check the entire tree with this ctor.
		mixed = is_contin_expr(*_it);
		if (not mixed) mixed = (id::greater_than_zero == *_it);
	}

	vertex operator()(const std::vector<builtin> &inputs)
	{
		if (mixed) return mixed_interpreter(inputs)(_it);
		return boolean_interpreter(inputs)(_it);
	}

	vertex operator()(const std::vector<contin_t> &inputs)
	{
		// Can't use contin, since the output might be non-contin,
		// e.g. a boolean, or an enum.
		// return contin_interpreter(inputs)(_it);
		return mixed_interpreter(inputs)(_it);
	}

	vertex operator()(const std::vector<vertex> &inputs)
	{
		return mixed_interpreter(inputs)(_it);
	}

	vertex operator()(const string_seq &inputs)
	{
		OC_ASSERT(false, "Not implemented");
		return vertex();
	}

	vertex operator()(const std::vector<combo_tree> &inputs)
	{
		OC_ASSERT(false, "Not implemented");
		return vertex();
	}

	combo_tree::iterator _it;
	bool mixed;
};

/**
 * multi_type_seq is a variant of sequences of primitive combo types,
 * vertex and tree. That way the Table, ITable or CTable can store
 * primitive inputs without the boost.variant overhead for each
 * entry. combo_tree_seq is also present to store combo_trees as
 * inputs instead of vertex (or primitive types) to have inputs of
 * lists, functions or any structured combo object.
 */
struct multi_type_seq : public boost::less_than_comparable<multi_type_seq>,
                        public boost::equality_comparable<multi_type_seq>
{
	typedef boost::variant<builtin_seq,
			contin_seq,
			string_seq,
			vertex_seq,
			combo_tree_seq> multi_type_variant;

	multi_type_seq()
	{
		// logger().debug("sizeof(builtin) = %u", sizeof(builtin));
		// logger().debug("sizeof(vertex) = %u", sizeof(vertex));
	}

	template<typename T>
	multi_type_seq(const std::initializer_list<T> &il)
			: _variant(std::vector<T>(il))
	{}

	template<typename T>
	multi_type_seq(const T &v) : _variant(v)
	{}

	template<typename T>
	void push_back(const T &e)
	{
		boost::apply_visitor(push_back_visitor<T>(e), _variant);
	}

	void pop_back()
	{
		pop_back_visitor popbv;
		boost::apply_visitor(popbv, _variant);
	}

	bool operator<(const multi_type_seq &r) const
	{
		return get_variant() < r.get_variant();
	}

	bool operator==(const multi_type_seq &r) const
	{
		equal_visitor ev;
		return boost::apply_visitor(ev, get_variant(), r.get_variant());
		// return get_variant() == r.get_variant();
	}

	size_t size() const
	{
		size_visitor sv;
		return boost::apply_visitor(sv, _variant);
	}

	bool empty() const
	{
		empty_visitor ev;
		return boost::apply_visitor(ev, _variant);
	}

	void erase_at(size_t pos)
	{
		boost::apply_visitor(erase_at_visitor(pos), _variant);
	}

	void init_at(size_t pos)
	{
		boost::apply_visitor(init_at_visitor(pos), _variant);
	}

	template<typename T>
	T get_at(size_t pos) const
	{
		return boost::apply_visitor(get_at_visitor<T>(pos), _variant);
	}

	template<typename T>
	void insert_at(int pos, const T &v)
	{
		boost::apply_visitor(insert_at_visitor<T>(pos, v), _variant);
	}

	std::vector<std::string> to_strings() const
	{
		to_strings_visitor tsv;
		return boost::apply_visitor(tsv, _variant);
	}

	multi_type_variant &get_variant()
	{ return _variant; }

	const multi_type_variant &get_variant() const
	{ return _variant; }

	// variant helpers
	template<typename T>
	std::vector<T> &get_seq()
	{
		return boost::get<std::vector<T>>(_variant);
	}

	template<typename T>
	const std::vector<T> &get_seq() const
	{
		return boost::get<std::vector<T>>(_variant);
	}

protected:
	// I set it as mutable because the FUCKING boost::variant
	// apply_visitor doesn't allow to deal with const variants. For
	// the same reason I cannot define multi_type_seq as an inherited
	// class from multi_type_variant (boost::variant kinda sucks!).
	mutable multi_type_variant _variant;
};

// Filter a multi_type_seq
template<typename F>
struct seq_filtered_visitor : public boost::static_visitor<multi_type_seq>
{
	seq_filtered_visitor(const F &filter) : _filter(filter)
	{}

	template<typename Seq>
	multi_type_seq operator()(const Seq &seq)
	{
		return seq_filtered(seq, _filter);
	}

	const F &_filter;
};

static const std::string default_timestamp_label("timestamp");

/**
 * Table containing timestamps.
 * There is only one column: a single timestamp for each row.
 */
struct TTable : public std::vector<boost::gregorian::date>
{
	typedef std::vector<boost::gregorian::date> super;
public:
	typedef boost::gregorian::date value_type;

	TTable(const std::string &tl = default_timestamp_label);

	TTable(const super &tt, const std::string &tl = default_timestamp_label);

	void set_label(const std::string &);

	const std::string &get_label() const;

	static TTable::value_type from_string(const std::string &timestamp_str);

	static std::string to_string(const TTable::value_type &timestamp);

protected:
	std::string label;
};

struct TimedValue :
		public boost::less_than_comparable<TimedValue>,
		public boost::equality_comparable<TimedValue>
{
	TimedValue(const vertex v,
	           const TTable::value_type t = TTable::value_type())
			: value(v), timestamp(t)
	{}

	vertex value;
	TTable::value_type timestamp;

	bool operator<(const TimedValue &r) const
	{
		return (value < r.value) || (timestamp < r.timestamp);
	}

	bool operator==(const TimedValue &r) const
	{
		return (value == r.value) && (timestamp == r.timestamp);
	}

};

// How to count timed inputs. The type is double, so that this will
// work with weighted features.
typedef double count_t;

struct TimedCounter : public Counter<TimedValue, count_t>
{
	// Overload get(const TimedValue&) to work with a vertex v, in
	// that case it returns the sum of that counter across all
	// timestamps with vertex equal to v.
	count_t get(const vertex &v) const;

	// Return a counter without timestamps
	Counter<vertex, count_t> untimedCounter() const;

	// Overload mode() so that it returns the most frequent
	// vertex over all timestamps.
	vertex mode() const;
};

/**
 * Like CTable (defined below) but the keys are timestamps.
 *
 * Inputs are currently not supported.
 *
 * For the moment it is a mere typedef, but it will probably have to
 * be turned into a class eventually.
 */
typedef std::map<TTable::value_type, Counter<vertex, count_t>> CTableTime;

/// CTable is a "compressed" table.  Compression is done by removing
/// duplicated inputs, and the output column is replaced by a counter
/// of the duplicated outputs.  That is, the output column is of the
/// form {(v1,t1):c1, {(v2,t2):c2, ...} where c1 is the number of
/// times value v1 was seen in the output at timestamp t1, c2 the
/// number of times v2 was observed at timestamps t2, etc. If the data
/// are not timestamped, then it's only {v1:c1, {v2:c2, ...}. In
/// practice the same structured is used with empty timestamp.
///
/// For weighted rows, the counts c1, c2,... need not be integers; they
/// will hold the sum of the (floating point) weights for the rows.
///
/// For example, if one has the following table (there is no timestamp
/// in this example):
///
///   output,input1,input2
///   1,1,0
///   0,1,1
///   1,1,0
///   0,1,0
///
/// Then the compressed table is
///
///   output,input1,input2
///   {0:1,1:2},1,0
///   {0:1},1,1
///
/// Most scoring functions work on CTable, as it avoids re-evaluating a
/// combo program on duplicated inputs.
//
class CTable : public std::map<multi_type_seq, TimedCounter>
	// , public boost::equality_comparable<CTable>
{
public:
	typedef multi_type_seq key_type;
	typedef TimedCounter mapped_type;
	typedef TimedCounter counter_t;
	typedef std::map<key_type, TimedCounter> super;
	typedef typename super::value_type value_type;
	typedef std::vector<std::string> string_seq;

	// Definition is delayed until after Table, as it uses Table.
	template<typename Func>
	CTable(const Func &func, arity_t arity, int nsamples = -1);

	CTable(const std::string &_olabel = "output");

	CTable(const string_seq &labs, const type_tree &tt);

	CTable(const std::string &_olabel, const string_seq &_ilabels,
	       const type_tree &tt);

	arity_t get_arity() const
	{ return ilabels.size(); }

	vertex_seq get_input_col_data(int offset) const;

	/// Return the total number of observations.
	/// This will equal to the size of the corresponding uncompressed
	/// when all row weights are equal to 1.0; otherwise, this is the
	/// sum of all the row weights.
	count_t uncompressed_size() const;

	/// Create a new table from this one, which contains only those
	/// columns specified by the filter.  The filter is assumed to be
	/// either a set or a vector (or an iterable, in general) that
	/// holds the index numbers of the columns to be kept.
	///
	/// Note that the filtered CTable can typically be further
	/// compressed, and so the compressed size will be smaller. This
	/// can be exploted to obtain performance gains.
	template<typename F>
	CTable filtered(const F &filter) const
	{
		typedef type_tree::iterator pre_it;
		typedef type_tree::sibling_iterator sib_it;

		// Filter the type signature tree
		// copy head
		type_tree fsig;
		pre_it head_src = tsig.begin();
		OC_ASSERT(*head_src == id::lambda_type);
		OC_ASSERT((int) tsig.number_of_children(head_src) == get_arity() + 1);
		pre_it head_dst = fsig.set_head(*head_src);
		// copy filtered input types
		sib_it sib_src = head_src.begin();
		arity_t a_pre = 0;
		for (arity_t a : filter) {
			std::advance(sib_src, a - a_pre);
			a_pre = a;
			fsig.replace(fsig.append_child(head_dst), sib_src);
		}

		// copy output type
		fsig.replace(fsig.append_child(head_dst), head_src.last_child());

		// Filter the labels
		CTable res(olabel, seq_filtered(ilabels, filter), fsig);

		// Filter the content
		seq_filtered_visitor <F> sfv(filter);
		auto asfv = boost::apply_visitor(sfv);
		for (const CTable::value_type v : *this)
			res[asfv(v.first.get_variant())] += v.second;

		// return the filtered CTable
		return res;
	}

	template<typename F>
	multi_type_seq filtered_preserve_idxs(const F &filter,
	                                      const multi_type_seq &seq) const
	{
		multi_type_seq res;
		auto it = filter.cbegin();
		for (unsigned i = 0; i < seq.size(); ++i) {
			if (it != filter.cend() && (typename F::value_type) i == *it) {
				// XXX TODO WARNING ERROR: builtin hardcoded shit!!!
				res.push_back(seq.get_at<builtin>(i));
				++it;
			} else {
				// XXX TODO WARNING ERROR: builtin hardcoded shit!!!
				res.push_back(id::null_vertex);
			}
		}
		return res;
	}

	/**
	 * Create a new table from this one, with all column values not in
	 * the filtered set being replaced by id::null_vertex.  This is
	 * similar to the filtered() method above, except that the total
	 * number of columns remains unchanged.  The table signature is also
	 * left unchanged.
	 *
	 * The filter should be an iterable (set or vector) containing
	 * column index numbers. The specified columns are kept; all others
	 * are blanked.
	 *
	 *  Note that the filtered CTable can typically be further
	 *  compressed, and so the compressed size will be smaller. This
	 *  can be exploted to obtain performance gains.
	 */
	template<typename F>
	CTable filtered_preserve_idxs(const F &filter) const
	{
		// Set new CTable
		CTable res(olabel, ilabels, tsig);

		// Filter the rows (replace filtered out values by id::null_vertex)
		for (const CTable::value_type v : *this)
			res[filtered_preserve_idxs(filter, v.first)] += v.second;

		// return the filtered CTable
		return res;
	}

	/**
	 * Remove the rows of the ctable. It treats rows as if they were
	 * uncompressed. The indexes follow the order set by the input
	 * rows, and then the output values, so for instance if a
	 * compressed row has N 0s and M 1s output values, it will treat
	 * that as N+M rows, where the rows ending by 0s precedes the ones
	 * ending by 1s (since 0 < 1).
	 */
	void remove_rows(const std::set<unsigned> &idxs);

	/**
	 * Similar to above but remove rows matching a set of dates.
	 */
	void remove_rows_at_times(const std::set<TTable::value_type> &timestamps);

	/**
	* Remove rows timestamped timestamp.
	 */
	void remove_rows_at_time(const TTable::value_type &timestamp);

	/**
	 * Get the set of timestamps in the data (if any)
	 */
	std::set<TTable::value_type> get_timestamps() const;

	// return the output label + list of input labels
	void set_labels(const string_seq &labels);

	string_seq get_labels() const;

	const std::string &get_output_label() const;

	const string_seq &get_input_labels() const;

	void set_signature(const type_tree &tt);

	const type_tree &get_signature() const;

	type_node get_output_type() const;

	CTableTime ordered_by_time() const;

	// Balance the ctable, so that, in case the output type is
	// discrete, all class counts are equal, but the uncompressed size
	// is till the same.
	void balance();

	// hmmm, it doesn't compile, I give up
	// bool operator==(const CTable& r) const {
	//     return super::operator==(static_cast<super>(r))
	//         && get_labels() == r.get_labels()
	//         && get_signature() == r.get_signature();
	// }
protected:
	type_tree tsig;                   // table signature
	std::string olabel;               // output label
	string_seq ilabels;               // list of input labels
};


/**
 * Input table of vertexes.
 * Rows represent data samples.
 * Columns represent input variables.
 * Optionally holds a list of column labels (input variable names)
 *
 * Each entry in the vector is a row.
 */
class OTable;

class ITable : public std::vector<multi_type_seq>
{
public:
	typedef std::vector<multi_type_seq> super;
	typedef super::value_type value_type;
	typedef std::vector<std::string> string_seq;
	typedef type_node_seq type_seq;

	ITable();

	ITable(const type_seq &ts, const string_seq &il = string_seq());

	ITable(const super &mat, const string_seq &il = string_seq());

	ITable(const OTable &);
	/**
	 * generate an input table according to the signature tt.
	 *
	 * @param tt signature of the table to generate.
	 * @param nsamples sample size, if negative then the sample
			  size is automatically determined.
	 * @param min_contin minimum contin value.
	 * @param max_contin maximum contin value.
	 *
	 * It only works for contin-boolean signatures
	 */
	// min_contin and max_contin are used in case tt has contin inputs
	ITable(const type_tree &tt, int nsamples = -1,
	       contin_t min_contin = -1.0, contin_t max_contin = 1.0);

	arity_t get_arity() const
	{
		return super::front().size();
	}

	bool operator==(const ITable &rhs) const;

	// set input labels
	void set_labels(const string_seq &);

	const string_seq &get_labels() const;

	void set_types(const type_seq &);

	const type_seq &get_types() const;

	type_node get_type(const std::string &) const;

	/**
	 * Insert a column 'col', named 'clab', after position 'off'.
	 * If off is negative, then insert after the last column.
	 *
	 * TODO: we really should use iterators here, not column numbers.
	 *
	 * TODO: should be generalized for multi_type_seq rather than
	 * vertex_seq
	 *
	 * WARNING: this function is automatically converting the ITable's
	 * rows into vertex_seq (this is also a hack till it handles
	 * multi_type_seq).
	 */
	void insert_col(const std::string &clab,
	                const vertex_seq &col,
	                int off = -1);

	/**
	 * Delete the named feature from the input table.
	 * If the feature is the empty string, then column zero is deleted.
	 * The returned value is the name of the column.
	 */
	std::string delete_column(const std::string &feature);

	void delete_columns(const string_seq &ignore_features);

	/**
	 * Get the column, given its offset or label
	 */
	vertex_seq get_column_data(const std::string &name) const;

	vertex_seq get_column_data(int offset) const;

	/// Return a copy of the input table filtered according to a given
	/// container of arity_t. Each value of that container corresponds
	/// to the column index of the ITable (starting from 0).
	template<typename F>
	ITable filtered(const F &filter) const
	{
		ITable res;

		// filter labels
		res.set_labels(seq_filtered(get_labels(), filter));

		// filter types
		res.set_types(seq_filtered(get_types(), filter));

		// filter content
		seq_filtered_visitor <F> sfv(filter);
		auto asf = boost::apply_visitor(sfv);
		for (const value_type &row : *this)
			res.push_back(asf(row.get_variant()));

		return res;
	}

	int get_column_offset(const std::string &col_name) const;

protected:
	mutable type_seq types;    // list of types of the columns
	mutable string_seq labels; // list of input labels

private:
	string_seq get_default_labels() const;

	/**
	 * this function take an arity in input and returns in output the
	 * number of samples that would be appropriate to check the semantics
	 * of its associated tree.
	 *
	 * Note : could take the two trees to checking and according to their
	 * arity structure, whatever, find an appropriate number.
	 */
	unsigned sample_count(arity_t contin_arity)
	{
		if (contin_arity == 0)
			return 1;
		else return COEF_SAMPLE_COUNT * log(contin_arity + M_E);
	}

};

static const std::string default_output_label("output");

/**
 * Output table of vertexes.
 * Rows represent dependent data samples.
 * There is only one column: a single output value for each row.
 * Optionally holds a column label (output variable names)
 */
class OTable : public vertex_seq
{
	typedef vertex_seq super;
public:
	typedef vertex value_type;

	OTable(const std::string &ol = default_output_label);

	OTable(const super &ot, const std::string &ol = default_output_label);

	/// Construct the OTable by evaluating the combo tree @tr for each
	/// row in the input ITable.
	OTable(const combo_tree &tr, const ITable &itable,
	       const std::string &ol = default_output_label);

	/// Construct the OTable by evaluating the combo tree @tr for each
	/// row in the input CTable.
	OTable(const combo_tree &tr, const CTable &ctable,
	       const std::string &ol = default_output_label);

	template<typename Func>
	OTable(const Func &f, const ITable &it,
	       const std::string &ol = default_output_label)
			: label(ol)
	{
		for (const multi_type_seq &vs : it)
			push_back(f(vs.get_seq<vertex>().begin(),
			            vs.get_seq<vertex>().end()));
	}

	void set_label(const std::string &);

	const std::string &get_label() const;

	void set_type(type_node);

	type_node get_type() const;

	bool operator==(const OTable &rhs) const;

	contin_t abs_distance(const OTable &) const;

	contin_t sum_squared_error(const OTable &) const;

	contin_t mean_squared_error(const OTable &) const;

	contin_t root_mean_square_error(const OTable &) const;

	vertex get_enum_vertex(const std::string &token);

protected:
	std::string label; // output label
	type_node type;    // the type of the column data.
};

/**
 * Typed data table.
 * The table consists of an ITable of inputs (independent variables),
 * an OTable holding the output (the dependent variable), and a type
 * tree identifiying the types of the inputs and outputs.
 */
struct Table : public boost::equality_comparable<Table>
{
	typedef std::vector<std::string> string_seq;
	typedef vertex value_type;

	Table();

	Table(const OTable &otable_, const ITable &itable_);

	template<typename Func>
	Table(const Func &func, arity_t a, int nsamples = -1) :
			itable(gen_signature(type_node_of<bool>(),
			                     type_node_of<bool>(), a)),
			otable(func, itable), target_pos(0), timestamp_pos(0)
	{}

	Table(const combo_tree &tr, int nsamples = -1,
	      contin_t min_contin = -1.0, contin_t max_contin = 1.0);

	size_t size() const
	{ return itable.size(); }

	arity_t get_arity() const
	{ return itable.get_arity(); }

	// Return the types of the columns in the table.
	// The type is returned as a lambda(input col types) -> output col type.
	// This is computed on the fly each time, instead ov being
	// stored with the object, so that RAM isn't wasted holding this
	// infrequently-needed info.
	type_tree get_signature() const
	{
		type_tree tt(id::lambda_type);
		auto root = tt.begin();
		for (type_node tn : itable.get_types())
			tt.append_child(root, tn);
		tt.append_child(root, otable.get_type());
		return tt;
	}

	// return a string with the io labels, the output label comes first
	string_seq get_labels() const;

	const std::string &get_target() const
	{ return otable.get_label(); }

	// Useful for filtered (see below), return some column position
	// after a filter has been applied
	template<typename F>
	unsigned update_pos(unsigned pos, const F &f) const
	{
		unsigned filtered_out_count = 0,
				last = 0;
		for (unsigned v : f) {
			if (v < pos)
				filtered_out_count += v - last;
			else {
				filtered_out_count += pos - last;
				break;
			}
			last = v;
		}
		return pos - filtered_out_count;
	}

	// Filter in, according to a container of arity_t. Each value of
	// that container corresponds to the column index of the ITable
	// (starting from 0).
	template<typename F>
	Table filtered(const F &f) const
	{
		Table res;

		// filter input table
		res.itable = itable.filtered(f);

		// set output table
		res.otable = otable;

		// set timestamp table
		res.ttable = ttable;

		// update target_pos
		res.target_pos = update_pos(target_pos, f);

		// update timestamp_pos
		if (!ttable.empty())
			res.timestamp_pos = update_pos(timestamp_pos, f);

		return res;
	}

	/// Return the corresponding compressed table.
	/// The named column, if not empty, will be used to provide weights
	/// for each row, during compression.
	CTable compressed(const std::string= "") const;

	ITable itable;
	OTable otable;
	TTable ttable;

	// Position of the target, useful for writing the table
	unsigned target_pos;

	// Position of the timestamp feature, useful for writing the
	// table. If the timestamp feature is empty then it is irrelevant.
	unsigned timestamp_pos;

	bool operator==(const Table &rhs) const;
};

template<typename Func>
CTable::CTable(const Func &func, arity_t arity, int nsamples)
{
	Table table(func, arity, nsamples);
	*this = table.compressed();
}

/////////////////////
// Subsample table //
/////////////////////

// Randomly remove rows so that the new size is ratio * table size
void subsampleTable(float ratio, Table &table);

void subsampleCTable(float ratio, CTable &ctable);

////////////////////////
// Mutual Information //
////////////////////////

/**
 * Compute the joint entropy H(Y) of an output table. It assumes the data
 * are discretized. (?)
 */
double OTEntropy(const OTable &ot);

/**
 * Compute the mutual information between a set of independent features
 * X_1, ... X_n and a taget feature Y.
 *
 * The target (output) feature Y is provided in the output table OTable,
 * whereas the input features are specified as a set of indexes giving
 * columns in the input table ITable. That is, the columns X1..Xn are
 * specified by the feature set fs.
 *
 * The mutual information
 *
 *   MI(Y; X1, ..., Xn)
 *
 * is computed as
 *
 *   MI(Y;X1, ..., Xn) = H(X1, ..., Xn) + H(Y) - H(X1, ..., Xn, Y)
 *
 * where
 *   H(...) are the joint entropies.
 *
 * @note currently, only works for boolean output columns.
 * to add enum support, cut-n-paste from CTable code below.
 *
 * XXX TODO -- this also should probably support the weight column,
 * since not all rows are important, and the ones that are not
 * important should not contribute to the MI.
 */
template<typename FeatureSet>
double mutualInformation(const ITable &it, const OTable &ot, const FeatureSet &fs)
{
	// XXX TODO to implement enum support, cut-n-paste from CTable
	// mutual info code, below.
	type_node otype = ot.get_type();
	OC_ASSERT(id::boolean_type == otype, "Only boolean types supported");

	// declare useful visitors
	seq_filtered_visitor <FeatureSet> sfv(fs);
	auto asf = boost::apply_visitor(sfv);

	// Let X1, ..., Xn be the input columns on the table, and
	// Y be the output column.  We need to compute the joint entropies
	// H(Y, X1, ..., Xn) and H(X1, ..., Xn)
	// To do this, we need to count how often the vertex sequence
	// (X1, ..., Xn) occurs. This count is kept in "ic". Likewise, the
	// "ioc" counter counts how often the vertex_seq (Y, X1, ..., Xn)
	// occurs.
	typedef Counter<multi_type_seq, count_t> VSCounter;
	VSCounter ic, // for H(X1, ..., Xn)
			ioc; // for H(Y, X1, ..., Xn)
	ITable::const_iterator i_it = it.begin();
	OTable::const_iterator o_it = ot.begin();

	for (; i_it != it.end(); ++i_it, ++o_it) {
		multi_type_seq ic_vec = asf(i_it->get_variant());
		ic[ic_vec] += 1.0;
		multi_type_seq ioc_vec(ic_vec);
		ioc_vec.push_back(get_builtin(*o_it));
		ioc[ioc_vec] += 1.0;
	}

	// Compute the probability distributions
	std::vector<double> ip(ic.size()), iop(ioc.size());
	double total = it.size();
	auto div_total = [&](count_t c) { return c / total; };
	transform(ic | map_values, ip.begin(), div_total);
	transform(ioc | map_values, iop.begin(), div_total);

	// Compute the joint entropies
	return entropy(ip) + OTEntropy(ot) - entropy(iop);
}

// Like the above, but taking a table in argument instead of
// input and output tables
template<typename FeatureSet>
double mutualInformation(const Table &table, const FeatureSet &fs)
{
	return mutualInformation(table.itable, table.otable, fs);
}

/**
 * Like above, but uses a compressed table instead of input and output
 * table.  Currently supports only boolean and enum outputs.  For contin
 * outputs, consider using KL instead (although, to be technically
 * correct, we really should use Fisher information. @todo this).
 */
template<typename FeatureSet>
double mutualInformation(const CTable &ctable, const FeatureSet &fs)
{
	// declare useful visitors
	seq_filtered_visitor <FeatureSet> sfv(fs);
	auto asf = boost::apply_visitor(sfv);
	type_node otype = ctable.get_output_type();

	const type_tree &tsig = ctable.get_signature();
	bool all_discrete_inputs = true;
	for (const type_tree &in_tt : get_signature_inputs(tsig)) {
		type_node tn = get_type_node(in_tt);
		if (tn != id::boolean_type and tn != id::enum_type) {
			all_discrete_inputs = false;
			break;
		}
	}


	/////////////////////
	// discrete inputs //
	/////////////////////
	if (all_discrete_inputs
	    and (id::enum_type == otype
	         or id::boolean_type == otype
	         or id::contin_type == otype)) {
		// Let X1, ..., Xn be the input columns on the table (as given by fs),
		// and Y be the output column.  We need to compute the joint entropies
		// H(Y, X1, ..., Xn) and H(X1, ..., Xn)
		// To do this, we need to count how often the vertex sequence
		// (X1, ..., Xn) occurs. This count is kept in "ic". Likewise, the
		// "ioc" counter counts how often the vertex_seq (Y, X1, ..., Xn)
		// occurs.
		typedef Counter<CTable::key_type, count_t> VSCounter;
		VSCounter ic;  // for H(X1, ..., Xn)
		VSCounter ioc; // for H(Y, X1, ..., Xn)
		double total = 0.0;

		std::vector<contin_t> disc_intvs;

		if (id::contin_type == otype) {
			contin_t min = 100000.0;
			contin_t max = 0.0;

			for (const auto &row : ctable) {
				for (const auto &val_pair : row.second) {
					const vertex &v = val_pair.first.value; // key of map
					if (get_contin(v) < min)
						min = get_contin(v);

					if (get_contin(v) > max)
						max = get_contin(v);
				}
			}
			disc_intvs = discretize_contin_feature(min, max);
		}

		// Count the total number of times an enum appears in the table
		Counter<vertex, count_t> ycount;

		for (const auto &row : ctable) {
			// Create the filtered row.
			CTable::key_type vec = asf(row.first.get_variant());

			// update ic (input counter)
			count_t row_total = row.second.total_count();
			ic[vec] += row_total;

			// for each enum type counted in the row,
			for (const auto &val_pair : row.second) {
				const vertex &v = val_pair.first.value; // counter key value

				count_t count = row.second.get(v);

				builtin b;

				// update ioc == "input output counter"
				switch (otype) {
					case id::enum_type:
						vec.push_back(get_enum_type(v));
						ycount[v] += count;
						break;
					case id::boolean_type:
						vec.push_back(get_builtin(v));
						ycount[v] += count;
						break;
					case id::contin_type:
						b = get_discrete_bin(disc_intvs, get_contin(v));
						vec.push_back(b);
						ycount[b] += count;
						break;
					default: OC_ASSERT(false, "case not implemented");
				}
				ioc[vec] += count;
				vec.pop_back();
			}

			// update total numer of data points
			total += row_total;
		}

		// Compute the probability distributions; viz divide count by total.
		// "c" == count, "p" == probability
		std::vector<double> yprob(ycount.size()), ip(ic.size()), iop(ioc.size());
		auto div_total = [&](count_t c) { return c / total; };
		transform(ycount | map_values, yprob.begin(), div_total);
		transform(ic | map_values, ip.begin(), div_total);
		transform(ioc | map_values, iop.begin(), div_total);

		// Compute the entropies
		return entropy(ip) + entropy(yprob) - entropy(iop);
	}

		/////////////////////
		// continuous case //
		/////////////////////
	else if (id::contin_type == otype) {
		if (1 < fs.size()) {
			OC_ASSERT(0, "Contin MI currently supports only 1 feature.");
		}
		std::multimap<contin_t, contin_t> sorted_list;
		for (const auto &row : ctable) {
			CTable::key_type vec = asf(row.first.get_variant());
			contin_t x = vec.get_at<contin_t>(0);

			// for each contin counted in the row,
			for (const auto &val_pair : row.second) {
				const auto &v = val_pair.first.value; // counter key value
				contin_t y = get_contin(v); // typecast

				unsigned flt_count = val_pair.second;
				dorepeat(flt_count) {
					auto pr = std::make_pair(x, y);
					sorted_list.insert(pr);
				}
			}
		}

		// XXX TODO, it would be easier if KLD took a sorted list
		// as the argument.
		std::vector<contin_t> p, q;
		for (auto pr : sorted_list) {
			p.push_back(pr.first);
			q.push_back(pr.second);
		}

		// KLD is negative; we want the IC to be postive.
		// XXX review this, is this really correct?  At any rate,
		// feature selection utterly fails with negative IC.
		// Also a problem, this is returning values greater than 1.0;
		// I thought that IC was supposed to max out at 1.0 !?
		contin_t ic = -KLD(p, q);
		// XXX TODO remove this print, for better performance.
		unsigned idx = *(fs.begin());
		logger().debug() << "Contin MI for feat=" << idx << " ic=" << ic;
		return ic;
	}

		//////////////////////////////////
		// Other non implemented cases //
		//////////////////////////////////
	else {
		std::stringstream ss;
		ss << "Type " << otype << " is not supported for mutual information";
		OC_ASSERT(0, ss.str());
		return 0.0;
	}
}

/**
 * Compute the mutual information between 2 sets of a ctable (only
 * discrete types are supported.
 */
template<typename FeatureSet>
double mutualInformationBtwSets(const CTable &ctable,
                                const FeatureSet &fs_l,
                                const FeatureSet &fs_r)
{
	// get union of fs_l and fs_r
	FeatureSet fs_u = set_union(fs_l, fs_r);

	// Check that the arities are within permitted range
	OC_ASSERT(all_of(fs_u.begin(), fs_u.end(),
	                 [&](const typename FeatureSet::value_type &f) {
		                 return f < ctable.get_arity();
	                 }));

	// declare useful visitors
	seq_filtered_visitor <FeatureSet> sfv_u(fs_u), sfv_l(fs_l), sfv_r(fs_r);
	auto asf_u = boost::apply_visitor(sfv_u),
			asf_l = boost::apply_visitor(sfv_l),
			asf_r = boost::apply_visitor(sfv_r);
	type_node otype = ctable.get_output_type();

	///////////////////
	// discrete case //
	///////////////////
	if (id::enum_type == otype or id::boolean_type == otype or id::contin_type) {
		// Let U1, ..., Un the features resulting from the union
		// between fs_l and fs_r.
		//
		// Let L1, ..., Lm the features of fs_l
		//
		// Let R1, ..., Rl the features of fs_r
		//
		// We need to compute the entropies
		//
		//       H(U1, ..., Un)
		//       H(L1, ..., Lm)
		//       H(R1, ..., Rl)
		//
		// Then the mutual information is
		//
		// MI(fs_l, fs_r) = H(L1, ..., Lm) + H(R1, ..., Rl) - H(U1, ..., Un)
		//
		// To do this, we need to count how often those events occurs.
		typedef Counter<CTable::key_type, count_t> VSCounter;
		VSCounter
				uc, // for H(U1, ..., Un)
				lc, // for H(L1, ..., Lm)
				rc; // for H(R1, ..., Rl)
		double total = 0.0;

		for (const auto &row : ctable) {
			// Create the filtered row.
			CTable::key_type vec_u = asf_u(row.first.get_variant()),
					vec_l = asf_l(row.first.get_variant()),
					vec_r = asf_r(row.first.get_variant());
			count_t row_total = row.second.total_count();

			// update uc, lc and rc
			uc[vec_u] += row_total;
			lc[vec_l] += row_total;
			rc[vec_r] += row_total;

			// update total numer of data points
			total += row_total;
		}

		// Compute the probability distributions; viz divide count by total.
		// "c" == count, "p" == probability
		std::vector<double> up(uc.size()), lp(lc.size()), rp(rc.size());
		auto div_total = [&](count_t c) { return c / total; };
		transform(uc | map_values, up.begin(), div_total);
		transform(lc | map_values, lp.begin(), div_total);
		transform(rc | map_values, rp.begin(), div_total);

		// Compute the entropies
		return entropy(lp) + entropy(rp) - entropy(up);
	}

		//////////////////////////////////
		// Other non implemented cases //
		//////////////////////////////////
	else {
		OC_ASSERT(0, "Unsupported type for mutual information");
		return 0.0;
	}
}

/**
 * template to subsample input and output tables, after subsampling
 * the table have size min(nsamples, *table.size())
 */
void subsampleTable(ITable &it, OTable &ot, unsigned nsamples);

/**
 * Like above on Table instead of ITable and OTable
 */
void subsampleTable(Table &table, unsigned nsamples);

/**
 * like above but subsample only the input table
 */
void subsampleTable(ITable &it, unsigned nsamples);

/////////////////
// Truth table //
/////////////////

/**
 * complete truth table, it contains only the outputs, the inputs are
 * assumed to be ordered in the conventional way, for instance if
 * there are 2 inputs, the output is ordered as follows:
 *
 * +-----------------------+--+--+
 * |Output                 |$1|$2|
 * +-----------------------+--+--+
 * |complete_truth_table[0]|F |F |
 * +-----------------------+--+--+
 * |complete_truth_table[1]|T |F |
 * +-----------------------+--+--+
 * |complete_truth_table[2]|F |T |
 * +-----------------------+--+--+
 * |complete_truth_table[3]|T |T |
 * +-----------------------+--+--+
 */
typedef std::vector<bool> bool_seq;
typedef ValueSeq ValuePtrVec;

class complete_truth_table : public bool_seq
{
public:
	typedef bool_seq super;

	complete_truth_table()
	{}

	template<typename It>
	complete_truth_table(It from, It to) : super(from, to)
	{}

	template<typename T>
	complete_truth_table(const tree<T> &tr, arity_t arity)
			: super(pow2(arity)), _arity(arity)
	{
		populate(tr);
	}

	template<typename T>
	complete_truth_table(const tree<T> &tr)
	{
		_arity = arity(tr);
		this->resize(pow2(_arity));
		populate(tr);
	}

	complete_truth_table(const Handle &)
	{
		OC_ASSERT(false, "Truth table from Handle not implemented yet");
	}

	/**
	 * This constructor assumes the program[handle] to have its features named
	 * '$1' to $[arity]. This convention was required in [setup_features] in order
	 * to map features with their respective values.
	 * */
	complete_truth_table(const Handle &handle, arity_t arity)
			: super(pow2(arity)), _arity(arity)
	{
		populate(handle);
	}

	template<typename Func>
	complete_truth_table(const Func &f, arity_t arity)
			: super(pow2(arity)), _arity(arity)
	{
		iterator it = begin();
		for (int i = 0; it != end(); ++i, ++it) {
			bool_seq v(_arity);
			for (arity_t j = 0; j < _arity; ++j)
				v[j] = (i >> j) % 2;  // j'th bit of i
			(*it) = f(v.begin(), v.end());
		}
	}

	/*
	  this operator allows to access quickly to the results of a
	  complete_truth_table. [from, to) points toward a chain of
	  boolean describing the inputs of the function coded into the
	  complete_truth_table and the operator returns the results.
	*/
	template<typename It>
	bool operator()(It from, It to)
	{
		const_iterator it = begin();
		for (int i = 1; from != to; ++from, i = i << 1)
			if (*from)
				it += i;
		return *it;
	}

	size_type hamming_distance(const complete_truth_table &other) const;

	/**
	 * compute the truth table of tr and compare it to self. This
	 * method is optimized so that if there are not equal it can be
	 * detected before calculating the entire table.
	 */
	bool same_complete_truth_table(const combo_tree &tr) const;

protected:
	template<typename T>
	void populate(const tree<T> &tr)
	{
		inputs.resize(_arity);
		iterator it = begin();
		for (int i = 0; it != end(); ++i, ++it) {
			for (int j = 0; j < _arity; ++j)
				inputs[j] = bool_to_builtin((i >> j) % 2);  // j'th bit of i
			*it = builtin_to_bool(boolean_interpreter(inputs)(tr));
		}
	}

	/**
	 * Sets the values of each predicateNode[Variables] of the program in order to
	 * get evaluated by the interpreter.
	 *
	 * @param handle       contains the program to get its variables populated.
	 * @param It from      beginning iterator of the vector containing values of variables.
	 * @param It to        end iterator of vector containing values of variables.
	 */
	void setup_features(const Handle &handle, const std::vector<ValuePtrVec>& features);

	void populate(const Handle &handle);

	void populate_features(std::vector<ValuePtrVec> &features);

	arity_t _arity;
	mutable builtin_seq inputs;
};

}
} // ~namespaces combo opencog

#endif // _OPENCOG_TABLE_H
