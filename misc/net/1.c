#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   // time
#include <unistd.h> // getpass

// ============ MD5 / MD5-HMAC ============

// XXX: Only works in little-endian mode
// XXX: Requires calloc()
// TODO: Replace or refactor this?
// ============ Start of AI-generated code ============
// NOTE: This seems to be derived from https://github.com/Zunawe/md5-c/blob/main/md5.c

// Minimal MD5 implementation
// MD5 constants
#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))

#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static void md5(
  const uint8_t *restrict initial_msg, size_t initial_len, uint8_t *restrict digest)
{
  // Message padding
  uint8_t *msg = NULL;
  size_t new_len;
  uint64_t bits_len;
  int i;

  // Initial hash values
  uint32_t h0 = 0x67452301;
  uint32_t h1 = 0xEFCDAB89;
  uint32_t h2 = 0x98BADCFE;
  uint32_t h3 = 0x10325476;

  // Preprocessing: Padding the message
  new_len = ((initial_len + 8) / 64 + 1) * 64; // Message length rounded up to 64-byte multiple
  msg = (uint8_t *)calloc(new_len, 1);         // Allocate memory
  memcpy(msg, initial_msg, initial_len);       // Copy the original message

  msg[initial_len] = 0x80;                     // Append the "1" bit
  bits_len = initial_len * 8;                  // Note: length in *bits*, not bytes
  memcpy(msg + new_len - 8, &bits_len, 8);     // Append original length in bits at the end

  // Constants for MD5 transform
  uint32_t r[] = {
     7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
     5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
     4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
     6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
  };

  uint32_t k[] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
  };

  // Process the message in 512-bit chunks
  for (i = 0; i < new_len; i += 64) {
    uint32_t *w = (uint32_t *)(msg + i);
    uint32_t a = h0, b = h1, c = h2, d = h3;

    // Main loop
    for (int j = 0; j < 64; j++) {
      uint32_t f, g;

      if (j < 16) {
        f = F(b, c, d);
        g = j;
      } else if (j < 32) {
        f = G(b, c, d);
        g = (5 * j + 1) % 16;
      } else if (j < 48) {
        f = H(b, c, d);
        g = (3 * j + 5) % 16;
      } else {
        f = I(b, c, d);
        g = (7 * j) % 16;
      }

      uint32_t temp = d;
      d = c;
      c = b;
      b = b + LEFTROTATE(a + f + k[j] + w[g], r[j]);
      a = temp;
    }

    // Add the chunk's hash to result
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
  }

  // Cleanup
  free(msg);

  // Output the final hash in little-endian
  memcpy(digest, &h0, 4);
  memcpy(digest + 4, &h1, 4);
  memcpy(digest + 8, &h2, 4);
  memcpy(digest + 12, &h3, 4);
}

// HMAC-MD5 implementation
void md5_hmac(
  const uint8_t *restrict key, size_t key_len,
  const uint8_t *restrict msg, size_t msg_len,
  uint8_t out[16])
{
  #define MD5_BLOCK_SIZE 64 // MD5 block size in bytes
  uint8_t key_block[MD5_BLOCK_SIZE] = {0};
  uint8_t o_key_pad[MD5_BLOCK_SIZE], i_key_pad[MD5_BLOCK_SIZE];
  uint8_t inner_hash[16]; // MD5 output size is 16 bytes

  // Step 1: Pad key to block size or hash it if it's longer
  if (key_len > MD5_BLOCK_SIZE) {
    md5(key, key_len, key_block);
  } else {
    memcpy(key_block, key, key_len);
  }

  // Step 2: Create inner and outer padded keys
  for (int i = 0; i < MD5_BLOCK_SIZE; i++) {
    o_key_pad[i] = key_block[i] ^ 0x5C;
    i_key_pad[i] = key_block[i] ^ 0x36;
  }

  // Step 3: Compute inner hash: MD5((K' ^ ipad) || message)
  uint8_t inner_data[MD5_BLOCK_SIZE + msg_len];
  memcpy(inner_data, i_key_pad, MD5_BLOCK_SIZE);
  memcpy(inner_data + MD5_BLOCK_SIZE, msg, msg_len);
  md5(inner_data, MD5_BLOCK_SIZE + msg_len, inner_hash);

  // Step 4: Compute outer hash: MD5((K' ^ opad) || inner_hash)
  uint8_t outer_data[MD5_BLOCK_SIZE + 16];
  memcpy(outer_data, o_key_pad, MD5_BLOCK_SIZE);
  memcpy(outer_data + MD5_BLOCK_SIZE, inner_hash, 16);
  md5(outer_data, MD5_BLOCK_SIZE + 16, out);
}

// ============ End of AI-generated code ============

// ============ SHA1 ============

// https://github.com/clibs/sha1/blob/master/sha1.h

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
    );

// https://github.com/clibs/sha1/blob/master/sha1.c

#define SHA1HANDSOFF

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#if BYTE_ORDER == LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#elif BYTE_ORDER == BIG_ENDIAN
#define blk0(i) block->l[i]
#else
#error "Endianness not defined!"
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);


/* Hash a single 512-bit block. This is the core of the algorithm. */

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
)
{
    uint32_t a, b, c, d, e;

    typedef union
    {
        unsigned char c[64];
        uint32_t l[16];
    } CHAR64LONG16;

#ifdef SHA1HANDSOFF
    CHAR64LONG16 block[1];      /* use array to appear as a pointer */

    memcpy(block, buffer, 64);
#else
    /* The following had better never be used because it causes the
     * pointer-to-const buffer to be cast into a pointer to non-const.
     * And the result is written through.  I threw a "const" in, hoping
     * this will cause a diagnostic.
     */
    CHAR64LONG16 *block = (const CHAR64LONG16 *) buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
#ifdef SHA1HANDSOFF
    memset(block, '\0', sizeof(block));
#endif
}


/* SHA1Init - Initialize new context */

void SHA1Init(
    SHA1_CTX * context
)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
)
{
    uint32_t i;

    uint32_t j;

    j = context->count[0];
    if ((context->count[0] += len << 3) < j)
        context->count[1]++;
    context->count[1] += (len >> 29);
    j = (j >> 3) & 63;
    if ((j + len) > 63)
    {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
        {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    }
    else
        i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
)
{
    unsigned i;

    unsigned char finalcount[8];

    unsigned char c;

#if 0    /* untested "improvement" by DHR */
    /* Convert context->count to a sequence of bytes
     * in finalcount.  Second element first, but
     * big-endian order within element.
     * But we do it all backwards.
     */
    unsigned char *fcp = &finalcount[8];

    for (i = 0; i < 2; i++)
    {
        uint32_t t = context->count[i];

        int j;

        for (j = 0; j < 4; t >>= 8, j++)
            *--fcp = (unsigned char) t}
#else
    for (i = 0; i < 8; i++)
    {
        finalcount[i] = (unsigned char) ((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);      /* Endian independent */
    }
#endif
    c = 0200;
    SHA1Update(context, &c, 1);
    while ((context->count[0] & 504) != 448)
    {
        c = 0000;
        SHA1Update(context, &c, 1);
    }
    SHA1Update(context, finalcount, 8); /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++)
    {
        digest[i] = (unsigned char)
            ((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
    /* Wipe variables */
    memset(context, '\0', sizeof(*context));
    memset(&finalcount, '\0', sizeof(finalcount));
}

void SHA1(
    uint8_t *hash_out,
    const uint8_t *str,
    uint32_t len)
{
    SHA1_CTX ctx;
    unsigned int ii;

    SHA1Init(&ctx);
    for (ii=0; ii<len; ii+=1)
        SHA1Update(&ctx, (const unsigned char*)str + ii, 1);
    SHA1Final((unsigned char *)hash_out, &ctx);
}

// ============ XXTEA ============

void xxtea_what(uint32_t* v, size_t n, const uint32_t k[4])
{
  uint32_t z = v[n - 1], s = 0;
  for (size_t r = 0; r < 6 + 52 / n; r++) {
    s += 0x9e3779b9;
    for (size_t p = 0; p < n; p++) {
      uint32_t y = v[p == n - 1 ? 0 : p + 1];
      // Incorrect mix function: original is
      //      (((z>>5) ^ (y<<2)) + ((y>>3) ^ (z<<4))) ^ ((s^y) + (k[((p)^(s>>2)) % 4]^z))
      v[p] += (((z>>5) ^ (y<<2)) + (((y>>3) ^ (z<<4)) ^ (s^y)) + (k[((p)^(s>>2)) % 4]^z));
      z = v[p];
    }
  }
}

// ============ Base64 ============

static void base64_encode(char *restrict out, const uint8_t *restrict a, size_t n, bool url_component)
{
  const char *alphabet = "LVoJPiCN2R8G90yg+hmFHuacZ1OWMnrsSTXkYpUq/3dlbfKwv6xztjI7DeBE45QA";
  size_t p = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t x = ((uint32_t)a[i] << 16);
    if (i + 1 < n) x |= ((uint32_t)a[i + 1] <<  8);
    if (i + 2 < n) x |= ((uint32_t)a[i + 2] <<  0);
    for (int j = 0; j < 4; j++) {
      char c = (i * 4 + j * 3 > n * 4) ? '=' : alphabet[(x >> (6 * (3 - j))) & 0x3f];
      if (url_component && (c == '+' || c == '/')) {
        out[p++] = '%';
        out[p++] = "0123456789ABCDEF"[c >> 4];
        out[p++] = "0123456789ABCDEF"[c & 0xf];
      } else {
        out[p++] = c;
      }
    }
  }
  out[p] = '\0';
}

const char *net_tsinghua_login_sign(
  const char token[64],
  const char *username,
  const char *password,
  int ac_id)
{
  // Argument "password"
  // = MD5-HMAC(token, <empty>)
  uint8_t hmac[16];
  md5_hmac((uint8_t *)token, 64, (uint8_t *)token, 0, hmac);

  uint8_t hmac_hex[33];
  for (int i = 0; i < 16; i++) {
    hmac_hex[i * 2 + 0] = "0123456789abcdef"[hmac[i] >> 4];
    hmac_hex[i * 2 + 1] = "0123456789abcdef"[hmac[i] & 0xf];
  }
  hmac_hex[32] = '\0';

  // Argument "info"
  // = Base64(XXTEA'(<JSON>, token))
  static uint8_t info_json[128] = { 0 };
  int info_json_len = snprintf((char *)info_json, sizeof info_json,
    "{\"username\":\"%s\",\"password\":\"%s\",\"ip\":\"\",\"acid\":\"%d\",\"enc_ver\":\"srun_bx1\"}",
    username, password, ac_id);
  assert(info_json_len < sizeof info_json);
  int n_u32 = (info_json_len + 3) / 4;
  ((uint32_t *)info_json)[n_u32] = info_json_len;
  xxtea_what((uint32_t *)info_json, n_u32 + 1, (const uint32_t *)token);

  char base64_info_json[(128 + 2) / 3 * 4 * 3 + 1];
  base64_encode(base64_info_json, info_json, (n_u32 + 1) * 4, false);
  // Do not URL-encode, as this will be fed to the signature algorithm

  // Argument "chksum"
  // = SHA1(<...>)
  static uint8_t checksum_str[1024];
  int checksum_str_len = snprintf((char *)checksum_str, sizeof checksum_str,
    "%s%s%s%s%s%d%s%s200%s1%s{SRBX1}%s",
    token, username,
    token, hmac_hex,
    token, ac_id,
    token,  // ip
    token,  // n
    token,  // type
    token, base64_info_json);
  assert(checksum_str_len < sizeof checksum_str);
  uint8_t checksum[20];
  SHA1(checksum, checksum_str, checksum_str_len);

  uint8_t checksum_hex[41];
  for (int i = 0; i < 20; i++) {
    checksum_hex[i * 2 + 0] = "0123456789abcdef"[checksum[i] >> 4];
    checksum_hex[i * 2 + 1] = "0123456789abcdef"[checksum[i] & 0xf];
  }
  checksum_hex[40] = '\0';

  // URL-encode into the final URL
  base64_encode(base64_info_json, info_json, (n_u32 + 1) * 4, true);

  static char out_query[1024];
  int out_query_len = snprintf(out_query, sizeof out_query,
    "https://auth4.tsinghua.edu.cn/cgi-bin/srun_portal?"
    "callback=f&action=login&ip=&double_stack=1&mac="
    "&username=%s&password=%%7BMD5%%7D%s&ac_id=%d"
    "&info=%%7BSRBX1%%7D%s&chksum=%s&n=200&type=1",
    username, hmac_hex, ac_id, base64_info_json, checksum_hex
  );
  assert(out_query_len < sizeof out_query);
  return out_query;
}

int main()
{
#if 0
  const char *token = "316f5ecb6c05123c8e25d2a9745e9e16fa0f46760c5801c55a3759cc81f315fa";
  const char *user = "user";
  const char *pwd = "qwqwq";
  int ac_id = 1;
#else
  char token[128], user[128], *pwd;
  int ac_id;
  printf("token: "); scanf("%s", token);
  printf("user: "); scanf("%s", user);
  pwd = getpass("pwd: ");
  printf("ac_id: "); scanf("%d", &ac_id);
#endif

  puts(net_tsinghua_login_sign(token, user, pwd, ac_id));

  return 0;
}
