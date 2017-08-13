#!/bin/sh
val=$(printf '\20\21\22\23\24\25\26\27' |od -v -An -tx8)
if [ $val = 1716151413121110 ]
then
	#little endian
	cat <<heredoc
static inline uint16_t oi16(uint16_t i) {
	return i;
}
static inline uint32_t oi32(uint32_t i) {
	return i;
}
static inline uint64_t oi64(uint64_t i) {
	return i;
}
heredoc
elif [ $val = 1011121314151617 ]
then
	#big engian
	cat <<heredoc
#define M 255ULL

static inline uint16_t oi16(uint16_t i) {
	return i << 8 | i >> 8;
}
static inline uint32_t oi32(uint32_t i) {
	return i << 24 | (i & (M << 8)) << 8 | (i & (M << 16)) >> 8 | i >> 24;
}
static inline uint64_t oi64(uint64_t i) {
	return i << 56 | (i & (M << 8)) << 40 | (i & (M << 16)) >> 24 | (i & (M << 24)) >> 8
		|(i & (M << 32)) >> 8 | (i & (M << 40)) >> 24 | (i & (M << 48)) >> 40 | i >> 56;
}
#undef M
heredoc
else
	terrible_endianness() {
		cat <<heredoc
static inline uint${1}_t oi${1}(uint${1}_t i) {
	uint8_t *val = &i;
	uint8_t out[$2];
heredoc
		printf "$3" |od -v -An -tx$2 |grep -o '1[0-7]' |awk 'BEGIN{NR--}{printf("\tout[%d] = val[%d];\n", $1 - 10, NR)}'
		cat <<heredoc
	return *(uint${1}_t*)out;
}
heredoc
	}
	terrible_endianness 16 2 '\21\20'
	terrible_endianness 32 4 '\23\22\21\20'
	terrible_endianness 64 8 '\27\26\25\24\23\22\21\20'
fi
