#include "T503.h"

int main(int argc, char *argv[]) {

	struct t503_context ctx;
	if (t503_init(&ctx)) return -1;
	t503_loop(&ctx);
	t503_exit(&ctx);

	return 0;

}