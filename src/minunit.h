// *
// *	JTN002 - MinUnit -- a minimal unit testing framework for C.
// *	License (from source): "You may use the code in this tech note for any purpose, with the understanding that it comes with NO WARRANTY."
// *
// *	Sourced from: http://www.jera.com/techinfo/jtns/jtn002.html
// *
#ifndef MINUNIT_H_
#define MINUNIT_H_

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; if (message) return message; } while (0)
extern int tests_run;

#endif // !MINUNIT_H_