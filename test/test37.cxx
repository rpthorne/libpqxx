#include <functional>

#include "test_helpers.hxx"

using namespace std;
using namespace pqxx;


// Test program for libpqxx.  Verify abort behaviour of RobustTransaction with
// a lazy connection.
//
// The program will attempt to add an entry to a table called "pqxxevents",
// with a key column called "year"--and then abort the change.
namespace
{
// Let's take a boring year that is not going to be in the "pqxxevents" table
const int BoringYear = 1977;

// Count events and specifically events occurring in Boring Year, leaving the
// former count in the result pair's first member, and the latter in second.
pair<int, int> count_events(connection_base &C, string table)
{
  const string CountQuery = "SELECT count(*) FROM " + table;
  row R;
  int all_years, boring_year;

  nontransaction tx{C};
  R = tx.exec1(CountQuery.c_str());
  R.front().to(all_years);

  R = tx.exec1(CountQuery + " WHERE year=" + to_string(BoringYear));
  R.front().to(boring_year);

  return make_pair(all_years, boring_year);
}


struct deliberate_error : exception
{
};


void test_037(transaction_base &)
{
  lazyconnection C;
  {
    nontransaction T(C);
    test::create_pqxxevents(T);
  }

  const string Table = "pqxxevents";

  const pair<int,int> Before = perform(bind(count_events, ref(C), Table));
  PQXX_CHECK_EQUAL(
	Before.second,
	0,
	"Already have event for " + to_string(BoringYear) + ", cannot test.");

  {
    quiet_errorhandler d(C);
    PQXX_CHECK_THROWS(
	perform(
          [&C, &Table](){ 
                robusttransaction<> tx{C};
    		tx.exec0(
			"INSERT INTO " + Table + " VALUES (" +
			to_string(BoringYear) + ", "
			"'yawn')");

    		throw deliberate_error();
	  }),
	deliberate_error,
	"Did not get expected exception from failing transactor.");
  }

  const pair<int,int> After = perform(bind(count_events, ref(C), Table));

  PQXX_CHECK_EQUAL(After.first, Before.first, "Number of events changed.");
  PQXX_CHECK_EQUAL(
	After.second,
	Before.second,
	"Number of events for " + to_string(BoringYear) + " changed.");
}
} // namespace

PQXX_REGISTER_TEST_NODB(test_037)
