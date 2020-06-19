#include "util.h"

#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#pragma GCC diagnostic ignored "-Wunused-macros"

#ifdef MINIMOD_LOG_ENABLE
#define LOG(FMT, ...) printf("[util] " FMT "\n", ##__VA_ARGS__)
#else
#define LOG(...)
#endif
#define LOGE(FMT, ...) fprintf(stderr, "[util] " FMT "\n", ##__VA_ARGS__)

#define ASSERT(in_condition)                      \
	do                                            \
	{                                             \
		if (__builtin_expect(!(in_condition), 0)) \
		{                                         \
			LOGE(                                 \
			  "[assertion] %s:%i: '%s'",          \
			  __FILE__,                           \
			  __LINE__,                           \
			  #in_condition);                     \
			__asm__ volatile("int $0x03");        \
			__builtin_unreachable();              \
		}                                         \
	} while (__LINE__ == -1)

#pragma GCC diagnostic pop


static int8_t const kTable[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};
struct quad
{
	uint8_t q[4];
};

static struct quad
quad_from_bytes(uint8_t a, uint8_t b, uint8_t c)
{
	return (struct quad){
		{
		  (uint8_t)((a >> 2) & 0x3f),
		  (uint8_t)((((a & 0x03) << 4) | ((b >> 4) & 0x0f)) & 0x3f),
		  (uint8_t)((((b & 0x0f) << 2) | ((c >> 6) & 0x03)) & 0x3f),
		  (uint8_t)(c & 0x3f),
		},
	};
}


size_t
enc_base64(
  void const *in_src,
  size_t in_srcbytes,
  void *out_dst,
  size_t in_dstbytes)
{
	size_t const nquads = (in_srcbytes / 3) + ((in_srcbytes % 3) ? 1 : 0);
	size_t const req_bytes = 4 * nquads;
	// early out
	if (!out_dst || in_dstbytes < req_bytes)
	{
		return req_bytes;
	}

	uint8_t const *in = (uint8_t const *)in_src;
	char *out = (char *)out_dst;
	// TODO limit by in_dstbytes too
	size_t remaining = in_srcbytes;

	while (remaining >= 3)
	{
		struct quad const quad = quad_from_bytes(in[0], in[1], in[2]);
		out[0] = kTable[quad.q[0]];
		out[1] = kTable[quad.q[1]];
		out[2] = kTable[quad.q[2]];
		out[3] = kTable[quad.q[3]];

		in += 3;
		out += 4;

		remaining -= 3;
	}

	if (remaining == 2)
	{
		struct quad const quad = quad_from_bytes(in[0], in[1], 0);
		out[0] = kTable[quad.q[0]];
		out[1] = kTable[quad.q[1]];
		out[2] = kTable[quad.q[2]];
		out[3] = '=';

		remaining -= 2;
	}
	else if (remaining == 1)
	{
		struct quad const quad = quad_from_bytes(in[0], 0, 0);
		out[0] = kTable[quad.q[0]];
		out[1] = kTable[quad.q[1]];
		out[2] = '=';
		out[3] = '=';

		remaining -= 1;
	}

	ASSERT(remaining == 0);

	return req_bytes;
}
