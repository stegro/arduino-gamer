#ifndef TYPE_H
#define TYPE_H

// ----------------------------------------------
// defines
// ----------------------------------------------
// PRES is 2**14
#define PRES             (1<<14)
#define PSHIFT           14
#define PROUNDBIT        (1 << (PSHIFT-1))

// ----------------------------------------------
// typedef
// ----------------------------------------------
typedef struct {
  long x, y, z;
} Vector3;

typedef struct {
  int x, y, z;
} Vector3i;

// fixed point identity matrix
typedef struct {
  long m[4][4] = {
      {PRES,    0,    0,    0},
      {   0, PRES,    0,    0},
      {   0,    0, PRES,    0},
      {   0,    0,    0,    PRES}
  };
} Matrix4;

#endif // TYPE_H
