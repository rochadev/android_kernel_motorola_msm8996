/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
/*
 * Contains declarations for routines to calculate the 802.11i PRF
 * functions.
 *
 * Author:  Mayank D. Upadhyay
 * Date:    19-March-2003
 * History:-
 * Date     Modified by Modification Information
 * ------------------------------------------------------
 */

#ifndef __AAG_PRF_H_
#define __AAG_PRF_H_

#include "vos_types.h"
#include <bapRsnAsfPacket.h>
#include <bapRsnSsmEapol.h>
#include "csrApi.h"


/*
 * Define the maximum size needed for the result array so that SHA-1
 * overflow is accounted for.
 */
#define AAG_PRF_MAX_OUTPUT_SIZE 80

#define AAG_RSN_PTK_TK1_OFFSET \
    (AAG_RSN_EAPOL_MIC_KEY_LEN + AAG_RSN_EAPOL_ENC_KEY_LEN)

// Pairwise key related definitions...

#define AAG_RSN_PTK_PRF_LEN_TKIP   512 //bits
#define AAG_RSN_PTK_PRF_LEN_CCMP   384 //bits
#define AAG_RSN_PTK_PRF_LEN_WEP104 384 //bits
#define AAG_RSN_PTK_PRF_LEN_WEP40  384 //bits

// Group key related definitions...

#define AAG_RSN_GMK_SIZE 16

#define AAG_RSN_GTK_PRF_LEN_TKIP   256 //bits
#define AAG_RSN_GTK_PRF_LEN_CCMP   128 //bits
#define AAG_RSN_GTK_PRF_LEN_WEP104 128 //bits
#define AAG_RSN_GTK_PRF_LEN_WEP40  128 //bits

// Key material length that is sent to the MAC layer...

#define AAG_RSN_KEY_MATERIAL_LEN_CCMP   16
#define AAG_RSN_KEY_MATERIAL_LEN_TKIP   32
#define AAG_RSN_KEY_MATERIAL_LEN_WEP104 13
#define AAG_RSN_KEY_MATERIAL_LEN_WEP40  5

/**
 * aagGetKeyMaterialLen
 *
 * Returns the number of bytes of the PTK that have to be provided to
 * the MAC layer for a given cipher type.
 *
 * @param cipherType the cipher-type
 *
 * @return the number of bytes of key material for this cipher type,
 * or 0 for invalid cipher types.
 */
int
aagGetKeyMaterialLen(eCsrEncryptionType cipherType);

/**
 * aagPtkPrf
 *
 * The PRF used for calculating the pairwise temporal key under IEEE
 * 802.11i.
 *
 * @param result a fixed size array where the outputis stored. Should
 * have enough place for the SHA-1 overflow.
 * @param prfLen the number of BITS desired from the PRF result
 * @param pmk the pairwise master-key
 * @param authAddr the MAC address of the authenticator
 * @param suppAddr the MAC address of the supplicant
 * @param aNonce the nonce generated by the authenticator
 * @param sNonce the nonce generated by the supplicant
 *
 * @return ANI_OK if the operation succeeds
 */
int
aagPtkPrf(v_U32_t cryptHandle,
          v_U8_t result[AAG_PRF_MAX_OUTPUT_SIZE],
          v_U32_t prfLen,
          tAniPacket *pmk, 
          tAniMacAddr authAddr,
          tAniMacAddr suppAddr,
          v_U8_t aNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE],
          v_U8_t sNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE]);


/**
 * aagGtkPrf
 *
 * The PRF used for calculating the group temporal key under IEEE
 * 802.11i.
 *
 * @param result a fixed size array where the outputis stored. Should
 * have enough place for the SHA-1 overflow.
 * @param prfLen the number of BITS desired from the PRF result
 * @param gmk the group master-key
 * @param authAddr the MAC address of the authenticator
 * @param gNonce the nonce generated by the authenticator for this purpose
 *
 * @return ANI_OK if the operation succeeds
 */
int
aagGtkPrf(v_U32_t cryptHandle,
          v_U8_t result[AAG_PRF_MAX_OUTPUT_SIZE],
          v_U32_t prfLen,
          v_U8_t gmk[AAG_RSN_GMK_SIZE],
          tAniMacAddr authAddr,
          v_U8_t gNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE]);

/**
 * aagPrf
 *
 * The raw PRF function that is used in IEEE 802.11i.
 *
 * @param result a fixed size array where the outputis stored. Should
 * have enough place for the SHA-1 overflow.
 * @param key the key to use in the PRF
 * @param keyLen the length of the key
 * @param a the parameter A which is usually a unique label
 * @param aLen the length of the parameter A
 * @ param b the parameter B
 * @param bLen the length of parameter B
 * @param prfLen the number to BITS desired from the PRF result
 *
 * @return ANI_OK if the operation succeeds
 */
int
aagPrf(v_U32_t cryptHandle,
       v_U8_t result[AAG_PRF_MAX_OUTPUT_SIZE],
       v_U8_t *key, v_U8_t keyLen,
       v_U8_t *a, v_U8_t aLen,
       v_U8_t *b, v_U8_t bLen,
       v_U32_t prfLen);

#endif //__AAG_PRF_H_
