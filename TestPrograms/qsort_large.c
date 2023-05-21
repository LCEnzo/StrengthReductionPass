#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define UNLIMIT
#define MAXARRAY 60000 /* this number, if too large, will cause a seg. fault!! */

struct my3DVertexStruct {
  int x, y, z;
  double distance;
};

int compare(const void *elem1, const void *elem2)
{
  /* D = [(x1 - x2)^2 + (y1 - y2)^2 + (z1 - z2)^2]^(1/2) */
  /* sort based on distances from the origin... */

  double distance1, distance2;

  distance1 = (*((struct my3DVertexStruct *)elem1)).distance;
  distance2 = (*((struct my3DVertexStruct *)elem2)).distance;

  return (distance1 > distance2) ? 1 : ((distance1 == distance2) ? 0 : -1);
}

int
main(int argc, char *argv[]) {
  struct my3DVertexStruct array[MAXARRAY];
  FILE *input_fp, *output_fp;
  int i, count = 0;
  int x, y, z;
  
  input_fp = fopen("input.dat", "r");
  if (input_fp == NULL) {
    fprintf(stderr, "Error opening input.dat\n");
    exit(-1);
  }

  while ((fscanf(input_fp, "%d", &x) == 1) && (fscanf(input_fp, "%d", &y) == 1) && (fscanf(input_fp, "%d", &z) == 1) && (count < MAXARRAY)) {
    array[count].x = x;
    array[count].y = y;
    array[count].z = z;
    array[count].distance = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
    count++;
  }
  fclose(input_fp);

  printf("\nSorting %d vectors based on distance from the origin.\n\n", count);
  qsort(array, count, sizeof(struct my3DVertexStruct), compare);

  output_fp = fopen("output.dat", "w");
  if (output_fp == NULL) {
    fprintf(stderr, "Error opening output.dat\n");
    exit(-1);
  }

  for (i = 0; i < count; i++) {
    fprintf(output_fp, "%d %d %d\n", array[i].x, array[i].y, array[i].z);
  }
  fclose(output_fp);

  return 0;
}
