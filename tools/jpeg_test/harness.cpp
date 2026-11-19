#include "jpeg_small.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
  // harness in.jpg outW outH out.raw [maxWork]
  FILE* f = fopen(argv[1], "rb"); if (!f) return 2;
  std::vector<uint8_t> d; int c; while ((c = fgetc(f)) != EOF) d.push_back((uint8_t)c); fclose(f);
  int w = atoi(argv[2]), h = atoi(argv[3]);
  size_t maxWork = argc > 5 ? strtoul(argv[5], 0, 10) : 60000;
  std::vector<uint16_t> out(w * h);
  const char* err = nullptr;
  bool ok = jpeg_small::decodeCover(d.data(), d.size(), out.data(), w, h, maxWork, &err);
  if (!ok) { printf("FAIL %s\n", err); return 1; }
  FILE* o = fopen(argv[4], "wb"); fwrite(out.data(), 2, out.size(), o); fclose(o);
  printf("OK\n"); return 0;
}
