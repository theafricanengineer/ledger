/*
 * Copyright (c) 2003-2009, John Wiegley.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of New Artisans LLC nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @defgroup math Core numerical objects
 */

/**
 * @file   amount.h
 * @author John Wiegley
 *
 * @ingroup math
 *
 * @brief Basic type for handling commoditized math: amount_t
 *
 * An amount is the most basic numerical type in Ledger, and relies on
 * commodity.h to represent commoditized amounts, which allows Ledger to
 * handle mathematical expressions involving disparate commodities.
 *
 * Amounts can be of virtually infinite size and precision.  When
 * division or multiplication is performed, the precision is
 * automatically expanded to include as many extra digits as necessary
 * to avoid losing information.
 */
#ifndef _AMOUNT_H
#define _AMOUNT_H

#include "utils.h"

namespace ledger {

class commodity_t;
class annotation_t;
class commodity_pool_t;

DECLARE_EXCEPTION(amount_error, std::runtime_error);

/**
 * @brief Encapsulate infinite-precision commoditized amounts
 *
 * Used to represent commoditized infinite-precision numbers, and
 * uncommoditized, plain numbers.  In the commoditized case, commodities
 * keep track of how they are used, and are always displayed back to the
 * user after the same fashion.  For uncommoditized numbers, no display
 * truncation is ever done.  In both cases, internal precision is always
 * kept to an excessive degree.
 */
class amount_t
  : public ordered_field_operators<amount_t,
	   ordered_field_operators<amount_t, double,
	   ordered_field_operators<amount_t, unsigned long,
	   ordered_field_operators<amount_t, long> > > >
{
public:
  /** Ready the amount subsystem for use.
      @note Normally called by session_t::initialize(). */
  static void initialize();
  /** Shutdown the amount subsystem and free all resources.
      @note Normally called by session_t::shutdown(). */
  static void shutdown();

  /** The amount's decimal precision. */
  typedef uint_least16_t precision_t;

  /** Number of places of precision by which values are extended to
      avoid losing precision during division and multiplication. */
  static const std::size_t extend_by_digits = 6U;

  /** Indicates which commodity pool should be used. */
  static commodity_pool_t * current_pool;

  /** If scalable commodities are automatically converted to their most
      reduced form for printing.

      For example, Ledger supports time values specified in seconds,
      hours or minutes.  Internally, such amounts are always kept as
      quantities of seconds.  However, when streaming the amount, Ledger
      converts it to its "least representation", which is \c 5.2h in the
      second case.  If \c keep_base is \c true this amount is displayed
      as \c 18720s. */
  static bool keep_base;

  /** If the lot price is considered whenever working with commoditized
      values.

      Let's say a user adds two values of the following form:
      @code
      10 AAPL + 10 AAPL {$20}
      @endcode

      This expression adds ten shares of Apple stock with another ten
      shares that were purchased for \c $20 a share.  If \c keep_price
      is false, the result of this expression is an amount equal to
      <tt>20 AAPL</tt>.  If \c keep_price is \c true the expression
      yields an exception for adding amounts with different commodities.
      In that case, a \link balance_t \endlink object must be used to
      store the combined sum. */
  static bool keep_price;

  /** If the lot date is considered whenever working with commoditized
      values. @see keep_date */
  static bool keep_date;
  /** If the lot note is considered whenever working with commoditized
      values. @see keep_tag */
  static bool keep_tag;

  /** If amounts should be streamed using to_fullstring() rather than
      to_string(), so that complete precision is always displayed no
      matter what the precision of an individual commodity may be. */
  static bool stream_fullstrings;

  static uint_fast32_t sizeof_bigint_t();

protected:
  void _copy(const amount_t& amt);
  void _dup();
  void _resize(precision_t prec);
  void _clear();
  void _release();

  struct bigint_t;

  bigint_t *	quantity;
  commodity_t *	commodity_;

public:
  /** @name Constructors
      @{ */

  /** Creates a value for which is_null() is true, and which has no
      value or commodity.  If used in a value expression it evaluates to
      zero, and its commodity equals \c commodity_t::null_commodity. */
  amount_t() : quantity(NULL), commodity_(NULL) {
    TRACE_CTOR(amount_t, "");
  }

  /** Convert a double to an amount.  As much precision as possible is
      decoded from the binary floating point number. */
  amount_t(const double val);

  /** Convert an unsigned long to an amount.  It's precision is zero. */
  amount_t(const unsigned long val);

  /** Convert a long to an amount.  It's precision is zero, and the sign
      is preserved. */
  amount_t(const long val);

  /** Parse a string as an (optionally commoditized) amount.  If no
      commodity is present, the resulting commodity is \c
      commodity_t::null_commodity.  The number may be of infinite
      precision. */
  explicit amount_t(const string& val) : quantity(NULL) {
    TRACE_CTOR(amount_t, "const string&");
    parse(val);
  }
  /** Parse a pointer to a C string as an (optionally commoditized)
      amount.  If no commodity is present, the resulting commodity is \c
      commodity_t::null_commodity.  The number may be of infinite
      precision. */
  explicit amount_t(const char * val) : quantity(NULL) {
    TRACE_CTOR(amount_t, "const char *");
    assert(val);
    parse(val);
  }

  /*@}*/

  /** Create an amount whose display precision is never truncated, even
      if the amount uses a commodity (which normally causes "round on
      streaming" to occur).  This function is mostly used by debugging
      code and unit tests.  This is the proper way to specify \c
      $100.005, where display of the extra digit precision is required.
      If a regular constructor were used, the amount would stream as \c
      $100.01, even though its internal value equals \c $100.005. */
  static amount_t exact(const string& value);

  /** Release the reference count held for the underlying \c
      amount_t::bigint_t object. */
  ~amount_t() {
    TRACE_DTOR(amount_t);
    if (quantity)
      _release();
  }

  /** @name Assignment and copy
      @{*/

  /** Copy an amount object.  Copies are very efficient, using a
      copy-on-write model.  Until the copy is changed, it refers to the
      same memory used by the original via reference counting.  The \c
      amount_t::bigint_t class in amount.cc maintains the reference. */
  amount_t(const amount_t& amt) : quantity(NULL) {
    TRACE_CTOR(amount_t, "copy");
    if (amt.quantity)
      _copy(amt);
    else
      commodity_ = NULL;
  }
  /** Copy an amount object, applying the given commodity annotation
      details afterward.  This is equivalent to doing a normal copy
      (@see amount_t(const amount_t&)) and then calling
      amount_t::annotate(). */
  amount_t(const amount_t& amt, const annotation_t& details) : quantity(NULL) {
    TRACE_CTOR(amount_t, "const amount_t&, const annotation_t&");
    assert(amt.quantity);
    _copy(amt);
    annotate(details);
  }
  /** Assign an amount object.  This is like copying if the amount was
      null beforehand, otherwise the previous value's reference is must
      be freed. */
  amount_t& operator=(const amount_t& amt);

  amount_t& operator=(const double val) {
    return *this = amount_t(val);
  }
  amount_t& operator=(const unsigned long val) {
    return *this = amount_t(val);
  }
  amount_t& operator=(const long val) {
    return *this = amount_t(val);
  }

  /* Assign a string to an amount.  This causes the contents of the
     string to be parsed, look for a commoditized or uncommoditized
     amount specifier. */
  amount_t& operator=(const string& str) {
    return *this = amount_t(str);
  }
  amount_t& operator=(const char * str) {
    assert(str);
    return *this = amount_t(str);
  }

  /*@}*/

  /** @name Comparison
      @{ */

  /** Compare two amounts, returning a number less than zero if \p amt
      is greater, exactly zero if they are equal, and greater than zero
      if \p amt is less.  This method is used to implement all of the
      other comparison methods.*/
  int compare(const amount_t& amt) const;

  /** Test two amounts for equality.  First the commodity pointers are
      quickly tested, then the multi-precision values themselves must be
      compared. */
  bool operator==(const amount_t& amt) const;

  template <typename T>
  bool operator==(const T& val) const {
    return compare(val) == 0;
  }
  template <typename T>
  bool operator<(const T& amt) const {
    return compare(amt) < 0;
  }
  template <typename T>
  bool operator>(const T& amt) const {
    return compare(amt) > 0;
  }

  /*@}*/

  /** @name Binary arithmetic
   */
  /*@{*/

  amount_t& operator+=(const amount_t& amt);
  amount_t& operator-=(const amount_t& amt);
  amount_t& operator*=(const amount_t& amt);

  /** Divide two amounts while extending the precision to preserve the
      accuracy of the result.  For example, if \c 10 is divided by \c 3,
      the result ends up having a precision of \link
      amount_t::extend_by_digits \endlink place to avoid losing internal
      resolution. */
  amount_t& operator/=(const amount_t& amt);

  /*@}*/

  /** @name Unary arithmetic
      @{ */

  /** Return an amount's internal precision.  To find the precision it
      should be displayed at -- assuming it was not created using
      amount_t::exact() -- use the following expression instead:
      @code
      amount.commodity().precision()
      @endcode */
  precision_t precision() const;

  /** Returns the negated value of an amount.
      @see operator-()
  */
  amount_t negate() const {
    amount_t temp(*this);
    temp.in_place_negate();
    return temp;
  }
  amount_t& in_place_negate();

  amount_t operator-() const {
    return negate();
  }

  /** Returns the absolute value of an amount.  Equivalent to:
      @code
      (x < * 0) ? - x : x
      @endcode
  */
  amount_t abs() const {
    if (sign() < 0)
      return negate();
    return *this;
  }

  /** An amount's internal value to the given precision, or to the
      commodity's current display precision if no precision value is
      given.  This method changes the internal value of the amount, if
      it's internal precision was greater than the rounding precision.
  */
  amount_t round() const {
    amount_t temp(*this);
    temp.in_place_round();
    return temp;
  }
  amount_t& in_place_round();

  amount_t round(precision_t prec) const {
    amount_t temp(*this);
    temp.in_place_round(prec);
    return temp;
  }
  amount_t& in_place_round(precision_t prec);

  /** Yields an amount whose display precision is never truncated, even
      though its commodity normally displays only rounded values.
  */
  amount_t unround() const;

  /** reduces a value to its most basic commodity form, for amounts that
      utilize "scaling commodities".  For example, an amount of \c 1h
      after reduction will be \c 3600s.
  */
  amount_t reduce() const {
    amount_t temp(*this);
    temp.in_place_reduce();
    return temp;
  }
  amount_t& in_place_reduce();

  /** unreduce(), if used with a "scaling commodity", yields the most
      compact form greater than one.  That is, \c 3599s will unreduce to
      \c 59.98m, while \c 3601 unreduces to \c 1h.
  */
  amount_t unreduce() const {
    amount_t temp(*this);
    temp.in_place_unreduce();
    return temp;
  }
  amount_t& in_place_unreduce();

  /** Returns the historical value for an amount -- the default moment
      returns the most recently known price -- based on the price history
      for the given commodity (or determined automatically, if none is
      provided).  For example, if the amount were <tt>10 AAPL</tt>, and
      on Apr 10, 2000 each share of \c AAPL was worth \c $10, then
      calling value() for that moment in time would yield the amount \c
      $100.00.
  */
  optional<amount_t>
  value(const optional<datetime_t>&   moment	  = none,
	const optional<commodity_t&>& in_terms_of = none) const;

  /*@}*/

  /** @name Truth tests
   */
  /*@{*/

  /** Truth tests.  An amount may be truth test in several ways:

      sign() returns an integer less than, greater than, or equal to
      zero depending on whether the amount is negative, zero, or
      greater than zero.  Note that this function tests the actual
      value of the amount -- using its internal precision -- and not
      the display value.  To test its display value, use:
      `round().sign()'.

      is_nonzero(), or operator bool, returns true if an amount's
      display value is not zero.

      is_zero() returns true if an amount's display value is zero.
      Thus, $0.0001 is considered zero if the current display precision
      for dollars is two decimal places.

      is_realzero() returns true if an amount's actual value is zero.
      Thus, $0.0001 is never considered realzero.

      is_null() returns true if an amount has no value and no
      commodity.  This only occurs if an uninitialized amount has never
      been assigned a value.
  */
  int sign() const;

  operator bool() const {
    return is_nonzero();
  }
  bool is_nonzero() const {
    return ! is_zero();
  }

  bool is_zero() const;
  bool is_realzero() const {
    return sign() == 0;
  }

  bool is_null() const {
    if (! quantity) {
      assert(! commodity_);
      return true;
    }
    return false;
  }

  /*@}*/

  /** @name Conversion
   */
  /*@{*/

  /** Conversion methods.  An amount may be converted to the same types
      it can be constructed from -- with the exception of unsigned
      long.  Implicit conversions are not allowed in C++ (though they
      are in Python), rather the following conversion methods must be
      called explicitly:

      to_double([bool]) returns an amount as a double.  If the optional
      boolean argument is true (the default), an exception is thrown if
      the conversion would lose information.

      to_long([bool]) returns an amount as a long integer.  If the
      optional boolean argument is true (the default), an exception is
      thrown if the conversion would lose information.

      fits_in_double() returns true if to_double() would not lose
      precision.

      fits_in_long() returns true if to_long() would not lose
      precision.

      to_string() returns an amount'ss "display value" as a string --
      after rounding the value according to the commodity's default
      precision.  It is equivalent to: `round().to_fullstring()'.

      to_fullstring() returns an amount's "internal value" as a string,
      without any rounding.

      quantity_string() returns an amount's "display value", but
      without any commodity.  Note that this is different from
      `number().to_string()', because in that case the commodity has
      been stripped and the full, internal precision of the amount
      would be displayed.
  */
  double to_double(bool no_check = false) const;
  long   to_long(bool no_check = false) const;
  string to_string() const;
  string to_fullstring() const;
  string quantity_string() const;

  bool   fits_in_double() const;
  bool   fits_in_long() const;

  /*@}*/

  /** @name Commodity methods
   */
  /*@{*/

  /** The following methods relate to an
      amount's commodity:

      commodity() returns an amount's commodity.  If the amount has no
      commodity, the value returned is `current_pool->null_commodity'.

      has_commodity() returns true if the amount has a commodity.

      set_commodity(commodity_t) sets an amount's commodity to the
      given value.  Note that this merely sets the current amount to
      that commodity, it does not "observe" the amount for possible
      changes in the maximum display precision of the commodity, the
      way that `parse' does.

      clear_commodity() sets an amount's commodity to null, such that
      has_commodity() afterwards returns false.

      number() returns a commodity-less version of an amount.  This is
      useful for accessing just the numeric portion of an amount.
  */
  commodity_t& commodity() const;

  bool has_commodity() const;
  void set_commodity(commodity_t& comm) {
    if (! quantity)
      *this = 0L;
    commodity_ = &comm;
  }
  void clear_commodity() {
    commodity_ = NULL;
  }

  amount_t number() const {
    if (! has_commodity())
      return *this;

    amount_t temp(*this);
    temp.clear_commodity();
    return temp;
  }

  /*@}*/

  /** @name Commodity annotations
   */
  /*@{*/

  /** An amount's commodity may be
      annotated with special details, such as the price it was
      purchased for, when it was acquired, or an arbitrary note,
      identifying perhaps the lot number of an item.

      annotate_commodity(amount_t price, [datetime_t date, string tag])
      sets the annotations for the current amount's commodity.  Only
      the price argument is required, although it can be passed as
      `none' if no price is desired.

      commodity_annotated() returns true if an amount's commodity has
      any annotation details associated with it.

      annotation_details() returns all of the details of an annotated
      commodity's annotations.  The structure returns will evaluate as
      boolean false if there are no details.

      strip_annotations([keep_price, keep_date, keep_tag]) returns an
      amount whose commodity's annotations have been stripped.  The
      three `keep_' arguments determine which annotation detailed are
      kept, meaning that the default is to follow whatever
      amount_t::keep_price, amount_t::keep_date and amount_t::keep_tag
      have been set to (which all default to false).
  */
  void          annotate(const annotation_t& details);
  bool          is_annotated() const;

  annotation_t& annotation();
  const annotation_t& annotation() const {
    return const_cast<amount_t&>(*this).annotation();
  }

  amount_t      strip_annotations(const bool _keep_price = keep_price,
				  const bool _keep_date  = keep_date,
				  const bool _keep_tag   = keep_tag) const;

  /*@}*/

  /** @name Parsing
   */
  /*@{*/

  /** The `flags' argument of both parsing may be one or more of the
      following:

      PARSE_NO_MIGRATE means to not pay attention to the way an
      amount is used.  Ordinarily, if an amount were $100.001, for
      example, it would cause the default display precision for $ to be
      "widened" to three decimal places.  If PARSE_NO_MIGRATE is
      used, the commodity's default display precision is not changed.

      PARSE_NO_REDUCE means not to call in_place_reduce() on the
      resulting amount after it is parsed.

      These parsing methods observe the amounts they parse (unless
      PARSE_NO_MIGRATE is true), and set the display details of
      the corresponding commodity accordingly.  This way, amounts do
      not require commodities to be pre-defined in any way, but merely
      displays them back to the user in the same fashion as it saw them
      used.

      There is also a static convenience method called
      `parse_conversion' which can be used to define a relationship
      between scaling commodity values.  For example, Ledger uses it to
      define the relationships among various time values:

      @code
      amount_t::parse_conversion("1.0m", "60s"); // a minute is 60 seconds
      amount_t::parse_conversion("1.0h", "60m"); // an hour is 60 minutes
      @endcode
  */
  enum parse_flags_enum_t {
    PARSE_DEFAULT    = 0x00,
    PARSE_NO_MIGRATE = 0x01,
    PARSE_NO_REDUCE  = 0x02,
    PARSE_SOFT_FAIL  = 0x04
  };

  typedef basic_flags_t<parse_flags_enum_t, uint_least8_t> parse_flags_t;

  /** The method parse() is used to parse an amount from an input stream
      or a string.  A global operator>>() is also defined which simply
      calls parse on the input stream.  The parse() method has two forms:

      parse(istream, flags_t) parses an amount from the given input
      stream.

      parse(string, flags_t) parses an amount from the given string.

      parse(string, flags_t) also parses an amount from a string.
  */
  bool parse(std::istream& in,
	     const parse_flags_t& flags = PARSE_DEFAULT);
  bool parse(const string& str,
	     const parse_flags_t& flags = PARSE_DEFAULT) {
    std::istringstream stream(str);
    bool result = parse(stream, flags);
    assert(stream.eof());
    return result;
  }

  static void parse_conversion(const string& larger_str,
			       const string& smaller_str);

  /*@}*/

  /** @name Printing
   */
  /*@{*/

  /** An amount may be output to a stream using the
      `print' method.  There is also a global operator<< defined which
      simply calls print for an amount on the given stream.  There is
      one form of the print method, which takes one required argument
      and two arguments with default values:

      print(ostream, bool omit_commodity = false, bool full_precision =
      false) prints an amounts to the given output stream, using its
      commodity's default display characteristics.  If `omit_commodity'
      is true, the commodity will not be displayed, only the amount
      (although the commodity's display precision is still used).  If
      `full_precision' is true, the full internal precision of the
      amount is displayed, regardless of its commodity's display
      precision.
  */
  void print(std::ostream& out, bool omit_commodity = false,
	     bool full_precision = false) const;

  /*@}*/

  /** @name Serialization
   */
  /*@{*/

  /** An amount may be deserialized from an input stream or a character
      pointer, and it may be serialized to an output stream.  The methods
      used are:

      read(istream) reads an amount from the given input stream.  It
      must have been put there using `write(ostream)'.  The required
      flow of logic is:
      amount_t::current_pool->write(out)
      amount.write(out)	// write out all amounts
      amount_t::current_pool->read(in)
      amount.read(in)

      read(char *&) reads an amount from data which has been read from
      an input stream into a buffer.  It advances the pointer passed in
      to the end of the deserialized amount.

      write(ostream, [bool]) writes an amount to an output stream in a
      compact binary format.  If the second parameter is true,
      quantities with multiple reference counts will be written in an
      optimized fashion.  NOTE: This form of usage is valid only for
      the binary journal writer, it should not be used otherwise, as it
      has strict requirements for reading that only the binary reader
      knows about.
  */
  void read(std::istream& in);
  void read(const char *& data,
	    char **	  pool	    = NULL,
	    char **	  pool_next = NULL);
  void write(std::ostream& out, std::size_t index = 0) const;

  /*@}*/

  /** @name XML Serialization
   */
  /*@{*/

  void read_xml(std::istream& in);
  void write_xml(std::ostream& out, const int depth = 0) const;

  /*@}*/

  /** @name Debugging
   */
  /*@{*/

  /** There are two methods defined to help with debugging:

      dump(ostream) dumps an amount to an output stream.  There is
      little different from print(), it simply surrounds the display
      value with a marker, for example "AMOUNT($1.00)".  This code is
      used by other dumping code elsewhere in Ledger.

      valid() returns true if an amount is valid.  This ensures that if
      an amount has a commodity, it has a valid value pointer, for
      example, even if that pointer simply points to a zero value.
  */
  void dump(std::ostream& out) const {
    out << "AMOUNT(";
    print(out);
    out << ")";
  }

  bool valid() const;

  /*@}*/
};

extern amount_t * one;

inline amount_t amount_t::exact(const string& value) {
  amount_t temp;
  temp.parse(value, PARSE_NO_MIGRATE);
  return temp;
}

inline string amount_t::to_string() const {
  std::ostringstream bufstream;
  print(bufstream);
  return bufstream.str();
}

inline string amount_t::to_fullstring() const {
  std::ostringstream bufstream;
  print(bufstream, false, true);
  return bufstream.str();
}

inline string amount_t::quantity_string() const {
  std::ostringstream bufstream;
  print(bufstream, true);
  return bufstream.str();
}

inline std::ostream& operator<<(std::ostream& out, const amount_t& amt) {
  amt.print(out, false, amount_t::stream_fullstrings);
  return out;
}
inline std::istream& operator>>(std::istream& in, amount_t& amt) {
  amt.parse(in);
  return in;
}

} // namespace ledger

#include "commodity.h"

namespace ledger {

inline bool amount_t::operator==(const amount_t& amt) const {
  if (commodity() != amt.commodity())
    return false;
  return compare(amt) == 0;
}

inline commodity_t& amount_t::commodity() const {
  return has_commodity() ? *commodity_ : *current_pool->null_commodity;
}

inline bool amount_t::has_commodity() const {
  return commodity_ && commodity_ != commodity_->parent().null_commodity;
}

} // namespace ledger

#endif // _AMOUNT_H