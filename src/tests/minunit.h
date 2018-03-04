// *
// *	JTN002 - MinUnit -- a minimal unit testing framework for C.
// *	License (from source): "You may use the code in this tech note for any purpose, with the understanding that it comes with NO WARRANTY."
// *
// *	Sourced from: http://www.jera.com/techinfo/jtns/jtn002.html
// *	Modified to use a global buffer to contain the failed test's function name, assertion expression, and line number.
// *
#ifndef MINUNIT_H_
#define MINUNIT_H_

#define mu_assert(func, test) do { if (!(test)) { snprintf(G_TEST_FAILED_BUF, 1024, "%s(): Line %d: assertion '%s' failed", func, __LINE__, #test); return 1; } } while (0)
#define mu_run_test(test) do { int result = test(); tests_run++; if (result) return result; } while (0)
char G_TEST_FAILED_BUF[1024];
extern int tests_run;

#endif // !MINUNIT_H_