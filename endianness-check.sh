#!/bin/sh
val=$(printf '\20\21\22\23\24\25\26\27' |od -v -An -tx8)
if [ $val = 1716151413121110 ]
then
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
	echo '#error 謎のエンディアンネス'
fi
