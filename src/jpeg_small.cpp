// Small JPEG decoder for aircraft photos. See jpeg_small.h.
//
// How it stays small: the photo box is 88x59, and a planespotters thumbnail
// is about 200x136, so the picture is decoded at 1/2 (or 1/4, 1/8) size.
// A JPEG block's top-left NxN coefficients are exactly what an NxN reduced
// inverse DCT needs, so only those are stored, plus one "is non-zero" bit per
// coefficient (progressive refinement scans need that to stay in step with
// the bitstream). The picture is then scaled down to cover the box with a
// box filter, one row at a time.
//
// Supports: baseline/extended (SOF0/1) and progressive (SOF2) Huffman JPEGs,
// 8-bit, grayscale or YCbCr, any sampling factors up to 4, restart markers.
// No float maths at run time (the ESP32-C6 has no FPU) except building one
// small cosine table per picture.
#include "jpeg_small.h"

#include <math.h>
#include <new>
#include <stdlib.h>
#include <string.h>

namespace jpeg_small {
namespace {

const uint8_t ZZ[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                        12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                        35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

struct Huff {
  bool defined = false;
  uint8_t vals[256];
  int32_t mincode[17], maxcode[18], valptr[17];
};

struct Comp {
  uint8_t id = 0, h = 1, v = 1, tq = 0;
  int bw = 0, bh = 0;          // blocks in the MCU-padded grid
  int usedBw = 0, usedBh = 0;  // blocks that hold picture (non-interleaved scans)
  int16_t* coef = nullptr;     // bw*bh*N*N low-frequency coefficients
  uint64_t* nz = nullptr;      // bw*bh masks: bit n = natural coefficient n is non-zero
  int dcPred = 0;
  uint8_t td = 0, ta = 0;
  uint8_t* row = nullptr;      // one MCU row of reduced pixels: (bw*N) x (v*N)
};

struct Decoder {
  const uint8_t* data;
  size_t len;
  const char* err = nullptr;

  // memory accounting
  size_t maxWork, used = 0;
  void* alloc(size_t n) {
    if (used + n > maxWork) {
      err = "too big for memory";
      return nullptr;
    }
    void* p = calloc(1, n);
    if (!p) {
      err = "out of memory";
      return nullptr;
    }
    used += n;
    return p;
  }

  // tables and frame
  uint16_t qt[4][64];
  bool qtDefined[4] = {false, false, false, false};
  Huff dc[4], ac[4];
  int restartInterval = 0;
  bool frame = false, progressive = false;
  int W = 0, H = 0, nf = 0, hmax = 1, vmax = 1, mcusX = 0, mcusY = 0;
  Comp comp[3];
  int N = 8, NN = 64, scaleDiv = 1;  // reduced block size, N*N, and 8/N
  int scans = 0;

  // bit reader
  const uint8_t* p = nullptr;
  const uint8_t* end = nullptr;
  uint32_t bitBuf = 0;
  int bitCnt = 0;
  bool atMarker = false;
  int zeroBytes = 0;

  void bitsReset() {
    bitBuf = 0;
    bitCnt = 0;
    atMarker = false;
  }
  void fill() {
    while (bitCnt <= 24) {
      uint32_t byte = 0;
      if (atMarker || p >= end) {
        if (++zeroBytes > 4096) err = "ran out of data";
      } else if (*p == 0xFF) {
        uint8_t next = (p + 1 < end) ? p[1] : 0xD9;
        if (next == 0x00) {
          byte = 0xFF;
          p += 2;
        } else {
          atMarker = true;  // end of the scan's data; feed zeros from here
        }
      } else {
        byte = *p++;
      }
      bitBuf |= byte << (24 - bitCnt);
      bitCnt += 8;
    }
  }
  inline int bits(int n) {  // n = 1..16
    if (n < 1 || n > 16) {
      err = "bad bit count";
      return 0;
    }
    if (bitCnt < n) fill();
    int v = (int)(bitBuf >> (32 - n));
    bitBuf <<= n;
    bitCnt -= n;
    return v;
  }
  inline int bit() { return bits(1); }
  static inline int extend(int v, int s) { return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v; }
  int decode(const Huff& h) {
    int code = bit();
    int l = 1;
    while (l <= 16 && code > h.maxcode[l]) {
      code = (code << 1) | bit();
      ++l;
    }
    if (l > 16) {
      err = "bad Huffman code";
      return 0;
    }
    int idx = h.valptr[l] + code - h.mincode[l];
    return (idx >= 0 && idx < 256) ? h.vals[idx] : 0;
  }

  // ------------------------------------------------------------- headers
  static int be16(const uint8_t* q) { return (q[0] << 8) | q[1]; }

  bool readDQT(const uint8_t* s, int n) {
    while (n > 0) {
      int pq = s[0] >> 4, tq = s[0] & 15;
      int need = 1 + (pq ? 128 : 64);
      if (tq > 3 || pq > 1 || n < need) return fail("bad DQT");
      for (int k = 0; k < 64; ++k) qt[tq][ZZ[k]] = pq ? be16(s + 1 + 2 * k) : s[1 + k];
      qtDefined[tq] = true;
      s += need;
      n -= need;
    }
    return true;
  }

  bool readDHT(const uint8_t* s, int n) {
    while (n > 0) {
      if (n < 17) return fail("bad DHT");
      int tc = s[0] >> 4, th = s[0] & 15;
      if (tc > 1 || th > 3) return fail("bad DHT");
      Huff& h = tc ? ac[th] : dc[th];
      int total = 0;
      for (int l = 1; l <= 16; ++l) total += s[l];
      if (total > 256 || n < 17 + total) return fail("bad DHT");
      memcpy(h.vals, s + 17, total);
      int code = 0, k = 0;
      for (int l = 1; l <= 16; ++l) {
        h.valptr[l] = k;
        h.mincode[l] = code;
        code += s[l];
        k += s[l];
        h.maxcode[l] = s[l] ? code - 1 : -1;
        if (code > (1 << l)) return fail("bad DHT");
        code <<= 1;
      }
      h.maxcode[17] = 0x7FFFFFFF;
      h.defined = true;
      s += 17 + total;
      n -= 17 + total;
    }
    return true;
  }

  bool readSOF(const uint8_t* s, int n, int outW, int outH) {
    if (frame) return fail("two frames");
    if (n < 6 || s[0] != 8) return fail("not 8-bit");
    H = be16(s + 1);
    W = be16(s + 3);
    nf = s[5];
    if (W <= 0 || H <= 0) return fail("no size");
    if (nf != 1 && nf != 3) return fail("not gray or YCbCr");
    if (n < 6 + 3 * nf) return fail("bad SOF");
    hmax = vmax = 1;
    for (int i = 0; i < nf; ++i) {
      Comp& c = comp[i];
      c.id = s[6 + 3 * i];
      c.h = s[7 + 3 * i] >> 4;
      c.v = s[7 + 3 * i] & 15;
      c.tq = s[8 + 3 * i];
      if (c.h < 1 || c.h > 4 || c.v < 1 || c.v > 4 || c.tq > 3) return fail("bad SOF");
      if (nf == 1) c.h = c.v = 1;  // a one-component picture is never interleaved
      if (c.h > hmax) hmax = c.h;
      if (c.v > vmax) vmax = c.v;
    }
    mcusX = (W + 8 * hmax - 1) / (8 * hmax);
    mcusY = (H + 8 * vmax - 1) / (8 * vmax);

    // Reduce as far as possible while still covering the output box.
    scaleDiv = 1;
    for (int d = 8; d >= 2; d /= 2) {
      if ((W + d - 1) / d >= outW && (H + d - 1) / d >= outH) {
        scaleDiv = d;
        break;
      }
    }
    N = 8 / scaleDiv;
    NN = N * N;

    for (int i = 0; i < nf; ++i) {
      Comp& c = comp[i];
      c.bw = mcusX * c.h;
      c.bh = mcusY * c.v;
      int compW = (W * c.h + hmax - 1) / hmax, compH = (H * c.v + vmax - 1) / vmax;
      c.usedBw = (compW + 7) / 8;
      c.usedBh = (compH + 7) / 8;
      size_t blocks = (size_t)c.bw * c.bh;
      c.coef = (int16_t*)alloc(blocks * NN * sizeof(int16_t));
      if (!c.coef) return false;
      c.nz = (uint64_t*)alloc(blocks * sizeof(uint64_t));
      if (!c.nz) return false;
      c.row = (uint8_t*)alloc((size_t)c.bw * N * c.v * N);
      if (!c.row) return false;
    }
    frame = true;
    return true;
  }

  bool fail(const char* why) {
    if (!err) err = why;
    return false;
  }

  // ------------------------------------------------------------- coefficients
  inline int16_t* slot(Comp& c, int blk, int nat) {
    int u = nat & 7, v = nat >> 3;
    return (u < N && v < N) ? &c.coef[(size_t)blk * NN + v * N + u] : nullptr;
  }
  inline void setCoef(Comp& c, int blk, int nat, int val) {
    if (int16_t* q = slot(c, blk, nat)) *q = (int16_t)val;
    if (val) c.nz[blk] |= (uint64_t)1 << nat;
  }

  int dcDiff(Comp& c) {
    int t = decode(dc[c.td]);
    if (t > 16) {
      err = "bad DC code";
      return 0;
    }
    return t ? extend(bits(t), t) : 0;
  }
  static int clampDc(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }

  // Sequential (baseline) block: DC + all AC.
  void blockSequential(Comp& c, int blk) {
    c.dcPred = clampDc(c.dcPred + dcDiff(c));
    setCoef(c, blk, 0, c.dcPred);
    for (int k = 1; k < 64;) {
      int rs = decode(ac[c.ta]);
      int s = rs & 15, r = rs >> 4;
      if (!s) {
        if (r != 15) break;
        k += 16;
        continue;
      }
      k += r;
      if (k > 63) break;
      setCoef(c, blk, ZZ[k], extend(bits(s), s));
      ++k;
    }
  }

  // Progressive scans.
  int ss = 0, se = 0, ah = 0, al = 0, eobrun = 0;

  void blockDcFirst(Comp& c, int blk) {
    c.dcPred = clampDc(c.dcPred + dcDiff(c));
    if (int16_t* q = slot(c, blk, 0)) *q = (int16_t)(c.dcPred * (1 << al));
  }
  void blockDcRefine(Comp& c, int blk) {
    if (bit()) {
      if (int16_t* q = slot(c, blk, 0)) *q |= (int16_t)(1 << al);
    }
  }
  void blockAcFirst(Comp& c, int blk) {
    if (eobrun) {
      --eobrun;
      return;
    }
    for (int k = ss; k <= se;) {
      int rs = decode(ac[c.ta]);
      int s = rs & 15, r = rs >> 4;
      if (!s) {
        if (r < 15) {
          eobrun = (1 << r) - 1;
          if (r) eobrun += bits(r);
          break;
        }
        k += 16;
        continue;
      }
      k += r;
      if (k > 63) break;
      setCoef(c, blk, ZZ[k], extend(bits(s), s) * (1 << al));
      ++k;
    }
  }
  // Refines one already non-zero coefficient (reads one correction bit).
  inline void refineNonZero(Comp& c, int blk, int nat, int p1) {
    if (bit()) {
      if (int16_t* q = slot(c, blk, nat)) {
        if ((*q & p1) == 0) *q += (*q >= 0) ? p1 : -p1;
      }
    }
  }
  void blockAcRefine(Comp& c, int blk) {
    const int p1 = 1 << al;
    int k = ss;
    if (!eobrun) {
      for (; k <= se; ++k) {
        int rs = decode(ac[c.ta]);
        if (err) return;
        int s = rs & 15, r = rs >> 4;
        int val = 0;
        if (s) {
          val = bit() ? p1 : -p1;  // s is always 1 here
        } else if (r != 15) {
          eobrun = 1 << r;
          if (r) eobrun += bits(r);
          break;
        }
        // skip r zero coefficients (refining the non-zero ones on the way)
        while (k <= se) {
          int nat = ZZ[k];
          if (c.nz[blk] >> nat & 1) {
            refineNonZero(c, blk, nat, p1);
          } else {
            if (--r < 0) break;
          }
          ++k;
        }
        if (val && k <= se) setCoef(c, blk, ZZ[k], val);
      }
    }
    if (eobrun) {
      for (; k <= se; ++k) {
        int nat = ZZ[k];
        if (c.nz[blk] >> nat & 1) refineNonZero(c, blk, nat, p1);
      }
      --eobrun;
    }
  }

  bool readSOS(const uint8_t* s, int n, const uint8_t* scanData) {
    if (!frame) return fail("scan before frame");
    if (n < 1) return fail("bad SOS");
    int ns = s[0];
    if (ns < 1 || ns > nf || n < 1 + 2 * ns + 3) return fail("bad SOS");
    Comp* sc[3];
    for (int i = 0; i < ns; ++i) {
      int id = s[1 + 2 * i];
      sc[i] = nullptr;
      for (int j = 0; j < nf; ++j)
        if (comp[j].id == id) sc[i] = &comp[j];
      if (!sc[i]) return fail("bad SOS");
      sc[i]->td = s[2 + 2 * i] >> 4;
      sc[i]->ta = s[2 + 2 * i] & 15;
      if (sc[i]->td > 3 || sc[i]->ta > 3) return fail("bad SOS");
    }
    ss = s[1 + 2 * ns];
    se = s[2 + 2 * ns];
    ah = s[3 + 2 * ns] >> 4;
    al = s[3 + 2 * ns] & 15;

    enum { SEQ, DC_FIRST, DC_REFINE, AC_FIRST, AC_REFINE } kind;
    if (!progressive) {
      kind = SEQ;
    } else if (ss == 0) {
      if (se != 0) return fail("bad progressive scan");
      kind = ah ? DC_REFINE : DC_FIRST;
    } else {
      if (se < ss || se > 63 || ns != 1) return fail("bad progressive scan");
      kind = ah ? AC_REFINE : AC_FIRST;
    }
    if (ah > 13 || al > 13) return fail("bad progressive scan");
    for (int i = 0; i < ns; ++i) {
      bool needDc = kind == SEQ || kind == DC_FIRST;
      bool needAc = kind == SEQ || kind == AC_FIRST || kind == AC_REFINE;
      if (needDc && !dc[sc[i]->td].defined) return fail("missing Huffman table");
      if (needAc && !ac[sc[i]->ta].defined) return fail("missing Huffman table");
      sc[i]->dcPred = 0;
    }

    p = scanData;
    bitsReset();
    eobrun = 0;
    int mcu = 0;
    auto restart = [&]() -> bool {
      if (!restartInterval || mcu == 0 || mcu % restartInterval) return true;
      // skip to the RSTn marker
      while (p + 1 < end && !(p[0] == 0xFF && p[1] >= 0xD0 && p[1] <= 0xD7)) {
        if (p[0] == 0xFF && p[1] != 0x00 && p[1] != 0xFF) return fail("missing restart marker");
        ++p;
      }
      if (p + 1 >= end) return fail("missing restart marker");
      p += 2;
      bitsReset();
      eobrun = 0;
      for (int i = 0; i < ns; ++i) sc[i]->dcPred = 0;
      return true;
    };
    auto block = [&](Comp& c, int blk) {
      switch (kind) {
        case SEQ: blockSequential(c, blk); break;
        case DC_FIRST: blockDcFirst(c, blk); break;
        case DC_REFINE: blockDcRefine(c, blk); break;
        case AC_FIRST: blockAcFirst(c, blk); break;
        case AC_REFINE: blockAcRefine(c, blk); break;
      }
    };

    if (ns == 1) {  // non-interleaved: one block per MCU, only the blocks that hold picture
      Comp& c = *sc[0];
      for (int by = 0; by < c.usedBh && !err; ++by) {
        for (int bx = 0; bx < c.usedBw && !err; ++bx, ++mcu) {
          if (!restart()) return false;
          block(c, by * c.bw + bx);
        }
      }
    } else {
      for (int my = 0; my < mcusY && !err; ++my) {
        for (int mx = 0; mx < mcusX && !err; ++mx, ++mcu) {
          if (!restart()) return false;
          for (int i = 0; i < ns; ++i) {
            Comp& c = *sc[i];
            for (int v = 0; v < c.v; ++v)
              for (int h = 0; h < c.h; ++h) block(c, (my * c.v + v) * c.bw + mx * c.h + h);
          }
        }
      }
    }
    if (err) return false;
    ++scans;
    // leave p at the next marker
    while (p + 1 < end && !(p[0] == 0xFF && p[1] != 0x00 && !(p[1] >= 0xD0 && p[1] <= 0xD7) && p[1] != 0xFF)) ++p;
    return true;
  }

  // ------------------------------------------------------------- output
  int32_t cosT[8][8];  // cosT[x][u] = C(u) * cos((2x+1) u pi / 2N) * 1024

  void buildCos() {
    for (int x = 0; x < N; ++x)
      for (int u = 0; u < N; ++u) {
        double cu = u ? 1.0 : 0.70710678118654752;
        cosT[x][u] = (int32_t)lround(cu * cos((2 * x + 1) * u * 3.14159265358979323846 / (2 * N)) * 1024.0);
      }
  }

  // Reduced inverse DCT of one block into N x N pixels at out (row stride `stride`).
  void idct(const int16_t* in, const uint16_t* q, uint8_t* out, int stride) {
    int32_t f[64], tmp[64];
    for (int v = 0; v < N; ++v)
      for (int u = 0; u < N; ++u) {
        int32_t val = (int32_t)in[v * N + u] * q[v * 8 + u];
        if (val > 2047) val = 2047;
        if (val < -2048) val = -2048;
        f[v * N + u] = val;
      }
    // rows: tmp[v][x] = sum_u f[v][u] * cos[x][u]
    for (int v = 0; v < N; ++v)
      for (int x = 0; x < N; ++x) {
        int32_t s = 0;
        for (int u = 0; u < N; ++u) s += f[v * N + u] * cosT[x][u];
        tmp[v * N + x] = (s + 64) >> 7;
      }
    // columns: pixel[y][x] = 1/4 * sum_v cos[y][v] * tmp[v][x]
    for (int y = 0; y < N; ++y)
      for (int x = 0; x < N; ++x) {
        int32_t s = 0;
        for (int v = 0; v < N; ++v) s += cosT[y][v] * tmp[v * N + x];
        int px = ((s + (1 << 14)) >> 15) + 128;
        out[y * stride + x] = (uint8_t)(px < 0 ? 0 : px > 255 ? 255 : px);
      }
  }

  uint32_t* acc = nullptr;
  int16_t* colMap = nullptr;
  uint8_t* written = nullptr;

  bool output(uint16_t* out, int outW, int outH) {
    for (int i = 0; i < nf; ++i)
      if (!qtDefined[comp[i].tq]) return fail("missing quantization table");
    buildCos();
    const int Wd = (W + scaleDiv - 1) / scaleDiv, Hd = (H + scaleDiv - 1) / scaleDiv;

    // cover: scale = max(outW/Wd, outH/Hd) = num/den, centered crop
    int64_t num, den;
    if ((int64_t)outW * Hd >= (int64_t)outH * Wd) {
      num = outW;
      den = Wd;
    } else {
      num = outH;
      den = Hd;
    }
    auto mapX = [&](int x) -> int {
      int64_t t = (2 * (int64_t)x + 1) * num - ((int64_t)Wd * num - (int64_t)outW * den);
      return t < 0 ? -1 : (int)(t / (2 * den));
    };
    auto mapY = [&](int y) -> int {
      int64_t t = (2 * (int64_t)y + 1) * num - ((int64_t)Hd * num - (int64_t)outH * den);
      return t < 0 ? -1 : (int)(t / (2 * den));
    };

    acc = (uint32_t*)alloc((size_t)outW * 4 * sizeof(uint32_t));
    colMap = (int16_t*)alloc((size_t)Wd * sizeof(int16_t));
    written = (uint8_t*)alloc(outH);
    if (!acc || !colMap || !written) return false;
    for (int x = 0; x < Wd; ++x) {
      int t = mapX(x);
      colMap[x] = (int16_t)(t >= outW ? -1 : t);
    }

    int curRow = -1;
    auto flush = [&]() {
      if (curRow < 0 || curRow >= outH) return;
      uint16_t* o = out + (size_t)curRow * outW;
      int firstX = -1;
      for (int x = 0; x < outW; ++x) {
        uint32_t* a = acc + 4 * x;
        if (a[3]) {
          uint32_t r = (a[0] + a[3] / 2) / a[3], g = (a[1] + a[3] / 2) / a[3], b = (a[2] + a[3] / 2) / a[3];
          o[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
          if (firstX < 0) firstX = x;
        } else if (x) {
          o[x] = o[x - 1];  // only when enlarging a tiny picture
        }
      }
      if (firstX < 0) return;  // nothing landed in this row
      for (int x = 0; x < firstX; ++x) o[x] = o[firstX];
      written[curRow] = 1;
      memset(acc, 0, (size_t)outW * 4 * sizeof(uint32_t));
    };

    const int mcuRowPx = vmax * N;
    for (int my = 0; my < mcusY; ++my) {
      // reduced pixels of this MCU row, per component
      for (int i = 0; i < nf; ++i) {
        Comp& c = comp[i];
        const int stride = c.bw * N;
        for (int v = 0; v < c.v; ++v) {
          int by = my * c.v + v;
          for (int bx = 0; bx < c.bw; ++bx)
            idct(c.coef + ((size_t)by * c.bw + bx) * NN, qt[c.tq], c.row + (size_t)v * N * stride + bx * N, stride);
        }
      }
      for (int py = 0; py < mcuRowPx; ++py) {
        int y = my * mcuRowPx + py;
        if (y >= Hd) break;
        int ty = mapY(y);
        if (ty < 0) continue;
        if (ty >= outH) break;
        if (ty != curRow) {
          flush();
          curRow = ty;
        }
        const Comp& c0 = comp[0];
        const uint8_t* yRow = c0.row + (size_t)(py * c0.v / vmax) * c0.bw * N;
        const uint8_t *cbRow = nullptr, *crRow = nullptr;
        if (nf == 3) {
          cbRow = comp[1].row + (size_t)(py * comp[1].v / vmax) * comp[1].bw * N;
          crRow = comp[2].row + (size_t)(py * comp[2].v / vmax) * comp[2].bw * N;
        }
        for (int x = 0; x < Wd; ++x) {
          int tx = colMap[x];
          if (tx < 0) continue;
          int Y = yRow[x * c0.h / hmax];
          int r = Y, g = Y, b = Y;
          if (nf == 3) {
            int cb = cbRow[x * comp[1].h / hmax] - 128, cr = crRow[x * comp[2].h / hmax] - 128;
            r = Y + ((91881 * cr + 32768) >> 16);
            g = Y - ((22554 * cb + 46802 * cr + 32768) >> 16);
            b = Y + ((116130 * cb + 32768) >> 16);
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
          }
          uint32_t* a = acc + 4 * tx;
          a[0] += r;
          a[1] += g;
          a[2] += b;
          a[3] += 1;
        }
      }
    }
    flush();

    // rows that got no source pixels (only when enlarging a tiny picture)
    int first = 0;
    while (first < outH && !written[first]) ++first;
    if (first == outH) return fail("no pixels");
    for (int y = 0; y < first; ++y) memcpy(out + (size_t)y * outW, out + (size_t)first * outW, outW * 2);
    for (int y = first + 1; y < outH; ++y)
      if (!written[y]) memcpy(out + (size_t)y * outW, out + (size_t)(y - 1) * outW, outW * 2);
    return true;
  }

  // ------------------------------------------------------------- main loop
  bool run(uint16_t* out, int outW, int outH) {
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) return fail("not a JPEG");
    end = data + len;
    const uint8_t* q = data + 2;
    for (;;) {
      if (err) return false;
      while (q < end && *q != 0xFF) ++q;  // tolerate junk between segments
      while (q < end && *q == 0xFF) ++q;  // fill bytes
      if (q >= end) break;               // truncated: use what we have
      uint8_t m = *q++;
      if (m == 0xD9) break;  // EOI
      if (m == 0x01 || (m >= 0xD0 && m <= 0xD8)) continue;
      if (q + 2 > end) break;
      int segLen = be16(q);
      if (segLen < 2 || q + segLen > end) {
        if (m == 0xDA) return fail("truncated");
        break;
      }
      const uint8_t* s = q + 2;
      int n = segLen - 2;
      switch (m) {
        case 0xC0:
        case 0xC1:
        case 0xC2:
          progressive = m == 0xC2;
          if (!readSOF(s, n, outW, outH)) return false;
          break;
        case 0xC3: case 0xC5: case 0xC6: case 0xC7:
        case 0xC9: case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
          return fail("unsupported JPEG type");
        case 0xC4:
          if (!readDHT(s, n)) return false;
          break;
        case 0xDB:
          if (!readDQT(s, n)) return false;
          break;
        case 0xDD:
          if (n < 2) return fail("bad DRI");
          restartInterval = be16(s);
          break;
        case 0xDA:
          if (!readSOS(s, n, q + segLen)) return false;
          q = p;
          continue;
        default:
          break;  // APPn, COM, ...
      }
      q += segLen;
    }
    if (!frame || !scans) return fail("no picture data");
    return output(out, outW, outH);
  }

  void freeAll() {
    free(acc);
    free(colMap);
    free(written);
    for (auto& c : comp) {
      free(c.coef);
      free(c.nz);
      free(c.row);
    }
  }
};

}  // namespace

bool decodeCover(const uint8_t* data, size_t len, uint16_t* out, int outW, int outH, size_t maxWork,
                 const char** err) {
  if (!data || !out || outW <= 0 || outH <= 0) {
    if (err) *err = "bad arguments";
    return false;
  }
  Decoder* d = (Decoder*)calloc(1, sizeof(Decoder));
  if (!d) {
    if (err) *err = "out of memory";
    return false;
  }
  new (d) Decoder();
  d->data = data;
  d->len = len;
  d->maxWork = maxWork;
  bool ok = d->run(out, outW, outH);
  if (err) *err = ok ? nullptr : (d->err ? d->err : "failed");
  d->freeAll();
  d->~Decoder();
  free(d);
  return ok;
}

}  // namespace jpeg_small
