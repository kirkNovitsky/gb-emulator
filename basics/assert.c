#include <assert.h>

int main(int argc, char* argv[]) {

  // Ignored, because condition is `true`
  assert((2 + 2) == 4);

  // Triggers, because condition is `false`
  assert((2 + 2) == 5);

  return 0;
}
