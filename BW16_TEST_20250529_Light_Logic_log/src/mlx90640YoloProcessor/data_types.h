#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct Detection {
  float x;
  float y;
  float w;
  float h;
  float conf;
};

struct Anchor {
  float w;
  float h;
};

#ifdef __cplusplus
}
#endif

#endif /* DATA_TYPES_H */
