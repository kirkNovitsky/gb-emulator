#include <stdio.h>

#define WIDTH 10
#define HEIGHT 5

unsigned int pixels[WIDTH * HEIGHT];

int main(int argc, char* argv[]) {

  // Store the array index for each pixel in its "value"
  for(unsigned int i = 0; i < WIDTH * HEIGHT; i++) {
    pixels[i] = i;
  }

  // Print some image info
  printf("\n  Size: %d x %d\n\n", WIDTH, HEIGHT);

  // Draw image
  unsigned int i = 0;
  for(unsigned int y = 0; y < HEIGHT; y++) {
    printf("\n");
    for(unsigned int x = 0; x < WIDTH; x++) {
      printf(" %3d", pixels[i]);
      i++;
    }
    printf("\n\n");
  }

  return 0;
}
