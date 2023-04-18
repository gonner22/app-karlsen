#include <stdint.h>   // uint*_t
#include <string.h>   // memset, explicit_bzero
#include <stdbool.h>  // bool

#include "crypto.h"

#include "globals.h"

int crypto_derive_private_key(cx_ecfp_private_key_t *private_key,
                              uint8_t chain_code[static 32],
                              const uint32_t *bip32_path,
                              uint8_t bip32_path_len) {
    uint8_t raw_private_key[32] = {0};
    int error = 0;

    BEGIN_TRY {
        TRY {
            // derive the seed with bip32_path
            os_perso_derive_node_bip32(CX_CURVE_256K1,
                                       bip32_path,
                                       bip32_path_len,
                                       raw_private_key,
                                       chain_code);
            // new private_key from raw
            cx_ecfp_init_private_key(CX_CURVE_256K1,
                                     raw_private_key,
                                     sizeof(raw_private_key),
                                     private_key);
        }
        CATCH_OTHER(e) {
            error = e;
        }
        FINALLY {
            explicit_bzero(&raw_private_key, sizeof(raw_private_key));
        }
    }
    END_TRY;

    return error;
}

void crypto_init_public_key(cx_ecfp_private_key_t *private_key,
                            cx_ecfp_public_key_t *public_key,
                            uint8_t raw_public_key[static 64]) {
    // generate corresponding public key
    cx_ecfp_generate_pair(CX_CURVE_256K1, public_key, private_key, 1);

    memmove(raw_public_key, public_key->W + 1, 64);
}

int crypto_sign_message(void) {
    cx_ecfp_private_key_t private_key = {0};
    uint8_t chain_code[32] = {0};
    // uint32_t info = 0;
    int sig_len = 0;

    // FIXME: Forced 44'/111111'/0'/0/0
    G_context.bip32_path[0] = 0x8000002C;
    G_context.bip32_path[1] = 0x8001b207;
    G_context.bip32_path[2] = 0x80000000;
    G_context.bip32_path[3] = 0x00000000;
    G_context.bip32_path[4] = 0x00000000;

    G_context.bip32_path_len = 5;

    // derive private key according to BIP32 path
    int error = crypto_derive_private_key(&private_key,
                                          chain_code,
                                          G_context.bip32_path,
                                          G_context.bip32_path_len);
    if (error != 0) {
        return error;
    }

    BEGIN_TRY {
        TRY {
            // FIXME: implement signing here:
            // from BTC:
            // https://github.com/LedgerHQ/app-bitcoin-new/blob/b2c624769c3b863b38dd133e8facabb3d7b5b76c/src/handler/sign_psbt.c
            // err = cx_ecschnorr_sign_no_throw(&private_key,
            //                              CX_ECSCHNORR_BIP0340 | CX_RND_TRNG,
            //                              CX_SHA256,
            //                              sighash,
            //                              32,
            //                              sig,
            //                              &sig_len);
            // from doc:
            // https://developers.ledger.com/docs/embedded-app/crypto-api/lcx__ecschnorr_8h/#a2aa2454ece11c17373539d7178d26a98
            // static int cx_ecschnorr_sign 	(
            //  const cx_ecfp_private_key_t *  	pvkey,
            // 	int  	mode,
            // 	cx_md_t  	hashID,
            // 	const unsigned char *  	msg,
            // 	unsigned int  	msg_len,
            // 	unsigned char *  	sig,
            // 	size_t  	sig_len,
            // 	unsigned int *  	info
            // )

            // sig_len = cx_ecschnorr_sign(&private_key,
            //                         CX_ECSCHNORR_BIP0340 | CX_RND_TRNG,
            //                         CX_SHA256,
            //                         "somemessagefixme",
            //                         sizeof("somemessagefixme"),
            //                         G_context.tx_info.signature,
            //                         sizeof(G_context.tx_info.signature),
            //                         &info);
            PRINTF("Signature: %.*H\n", sig_len, G_context.tx_info.signature);
        }
        CATCH_OTHER(e) {
            error = e;
        }
        FINALLY {
            explicit_bzero(&private_key, sizeof(private_key));
        }
    }
    END_TRY;

    if (error == 0) {
        G_context.tx_info.signature_len = sig_len;
    }

    return error;
}
