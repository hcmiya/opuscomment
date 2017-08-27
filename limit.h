// 120MiB制限はRFCが根拠
#define TAG_LENGTH_LIMIT__RFC (120 * (1 << 20))

// 受け入れ制限は緩く。(このまま保存は出来ない)
#define TAG_LENGTH_LIMIT__INPUT TAG_LENGTH_LIMIT__RFC

// 40MiBはopuscommentが独自に設定した制限。
// 常識的に考えてタグがこれ以上長くなるのは画像含むことを考慮しても想定しづらいので
#define TAG_LENGTH_LIMIT__OUTPUT (40 * (1 << 20))

// スタック上で小さい入出力バッファを使う時のサイズ指定
#define STACK_BUF_LEN 512
