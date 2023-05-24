#include "secp256k1_extension.h"

#include <itcoin_ecmult_const.h>

void itcoin_secp256k1_ge_set_gej_safe(itcoin_secp256k1_ge *r, const itcoin_secp256k1_gej *a)
{
  itcoin_secp256k1_fe z2, z3;
  itcoin_secp256k1_gej tmp;
  r->infinity = a->infinity;
  itcoin_secp256k1_fe_inv(&tmp.z, &a->z);
  itcoin_secp256k1_fe_sqr(&z2, &tmp.z);
  itcoin_secp256k1_fe_mul(&z3, &tmp.z, &z2);
  itcoin_secp256k1_fe_mul(&tmp.x, &a->x, &z2);
  itcoin_secp256k1_fe_mul(&tmp.y, &a->y, &z3);
  itcoin_secp256k1_fe_set_int(&tmp.z, 1);
  r->x = tmp.x;
  r->y = tmp.y;
}

void point_mul_scalar(itcoin_secp256k1_gej *result, const itcoin_secp256k1_gej *pt, const itcoin_secp256k1_scalar *sc)
{
  itcoin_secp256k1_ge pt_ge;
  itcoin_secp256k1_ge_set_gej_safe(&pt_ge, pt);
  itcoin_secp256k1_ecmult_const(result, &pt_ge, sc, 256);
} // point_mul_scalar()

bool point_equal_point(const itcoin_secp256k1_gej *a, const itcoin_secp256k1_gej *b)
{
  itcoin_secp256k1_ge a_ge;
  itcoin_secp256k1_ge b_ge;

  itcoin_secp256k1_ge_set_gej_safe(&a_ge, a);
  itcoin_secp256k1_ge_set_gej_safe(&b_ge, b);

  return (itcoin_secp256k1_fe_equal(&(a_ge.x), &(b_ge.x)) && itcoin_secp256k1_fe_equal(&(a_ge.y), &(b_ge.y)));
} // point_equal_point()
