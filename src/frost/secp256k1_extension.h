#ifndef ITCOIN_SECP256K1_EXTENSION_H
#define ITCOIN_SECP256K1_EXTENSION_H


#ifdef __cplusplus
extern "C" {
#endif

#include <itcoin_secp256k1.h>
#include <stdbool.h>

void itcoin_secp256k1_ge_set_gej_safe(itcoin_secp256k1_ge *r, const itcoin_secp256k1_gej *a);
void point_mul_scalar(itcoin_secp256k1_gej *result, const itcoin_secp256k1_gej *pt, const itcoin_secp256k1_scalar *sc);
bool point_equal_point(const itcoin_secp256k1_gej *a, const itcoin_secp256k1_gej *b);

#ifdef __cplusplus
}
#endif

#endif // ITCOIN_SECP256K1_EXTENSION_H
