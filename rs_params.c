#include "rs_params.h"
#include "rs_prf.h"
#include <string.h>

void rs_params_init(rs_params_t *p,
                    const uint8_t seed_ax[RS_SEED_BYTES],
                    const uint8_t seed_ay[RS_SEED_BYTES],
                    const uint8_t seed_orb_x[RS_SEED_BYTES],
                    const uint8_t seed_orb_y[RS_SEED_BYTES],
                    const uint8_t seed_B[RS_SEED_BYTES],
                    const uint8_t seed_C[RS_SEED_BYTES]) {
    // Copy seeds into parameter structure
    memcpy(p->seed_ax, seed_ax, RS_SEED_BYTES);
    memcpy(p->seed_ay, seed_ay, RS_SEED_BYTES);
    memcpy(p->seed_orb_x, seed_orb_x, RS_SEED_BYTES);
    memcpy(p->seed_orb_y, seed_orb_y, RS_SEED_BYTES);
    memcpy(p->seed_B, seed_B, RS_SEED_BYTES);
    memcpy(p->seed_C, seed_C, RS_SEED_BYTES);

    // Derive AES-256 keys from seeds using SHA3-256(label || seed)
    rs_derive_aes_key(p->seed_ax,    "AX_KEY",  p->key_ax);
    rs_derive_aes_key(p->seed_ay,    "AY_KEY",  p->key_ay);
    rs_derive_aes_key(p->seed_orb_x, "AOX_KEY", p->key_orb_x);
    rs_derive_aes_key(p->seed_orb_y, "AOY_KEY", p->key_orb_y);
    rs_derive_aes_key(p->seed_B,     "B_KEY",   p->key_B);
    rs_derive_aes_key(p->seed_C,     "C_KEY",   p->key_C);
}
