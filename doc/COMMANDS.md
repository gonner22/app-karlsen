# Karlsen commands

## Overview

| Command name | INS | Description |
| --- | --- | --- |
| `GET_VERSION` | 0x03 | Get application version as `MAJOR`, `MINOR`, `PATCH` |
| `GET_APP_NAME` | 0x04 | Get ASCII encoded application name |
| `GET_PUBLIC_KEY` | 0x05 | Get public key given BIP32 path |
| `SIGN_TX` | 0x06 | Sign transaction given transaction info, utxos and outputs |
| `SIGN_MESSAGE` | 0x07 | Sign the personal message |

## GET_VERSION

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x03 | 0x00 | 0x00 | 0x00 | - |

### Response

| Length <br/>(bytes) | SW | RData |
| --- | --- | --- |
| 3 | 0x9000 | `MAJOR (1)` \|\| `MINOR (1)` \|\| `PATCH (1)` |

## GET_APP_NAME

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x04 | 0x00 | 0x00 | 0x00 | - |

### Response

| Length <br/>(bytes) | SW | RData |
| --- | --- | --- |
| 7 | 0x9000 | `Karlsen (7 bytes)` in ASCII |

Raw response looks like: `4b61726c73656e9000`

## GET_PUBLIC_KEY

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x05 | 0x00 (no display) <br> 0x01 (display) | 0x00 | 0x15 | `path len (1 byte)` \|\|<br>`purpose (4 bytes)` \|\|<br> `coin_type (4 bytes)` \|\|<br> `account (4 bytes)` \|\|<br> `type (4 bytes)` \|\|<br>`index (4 bytes)` |

Keys for karlsen normally use the derivation path `m/44'/121337'/<account>'/<type>/<index>`. This command will accept these as inputs:
- `m/44'/121337'`
- `m/44'/121337'/<account>'`
- `m/44'/121337'/<account>'/<type>`
- `m/44'/121337'/<account>'/<type>/<index>`

| CData Part | Description |
| --- | --- |
| `purpose` | Must be `44'` or `0x80000002c` |
| `coin_type` | Must be `121337'` or `0x8001d9f9` |
| `account` | Current wallets all use `0x80000000` (aka. `0'`) for default account but any value from `0x80000000` to `0xFFFFFFFF` is accepted if passed |
| `type` | Current wallets use either `0x00000000` for Receive Address or `0x00000001` for Change Address, but any value from `0x00000000` to `0xFFFFFFFF` is accepted if passed |
| `index` | Any value from `0x00000000` to `0xFFFFFFFF` if passed |

If you want to generate addresses using a root public key,

### Response

| Length <br/>(bytes) | SW | RData |
| --- | --- | --- |
| var | 0x9000 | `len(public_key) (1)` \|\|<br> `public_key (65 bytes)` \|\|<br> `len(chain_code) (1)` \|\|<br> `chain_code (var)` |

`public_key` here will always be `65 bytes`, representing the uncompressed public key in DER encoding.
It is composed of `0x04` followed by `32 bytes` for the `X` coordinate of the public key, then `32 bytes` for the `Y` coordinate.

In Karlsen, you only need the `32 bytes` for the `X` coordinate as the compressed public key to generate addresses whose transactions are signed with Schnorr.

Transactions signed with ECDSA are currently not supported.

## SIGN_TX

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x06 | 0x00-0x03 | 0x80 or 0x00 | var | See below |

#### P1 Breakdown

| P1 Value | Usage | CData |
| --- | --- | --- |
| 0x00 | Sending transaction metadata | `version (2)` \|\| `output_len (1)` \|\| `input_len (1)` \|\| `change_address_type (1)` \|\| `change_address_index (4)` \|\| `account (4)` |
| 0x01 | Sending a tx output | `value (8)` \|\| `script_public_key (34/35)` |
| 0x02 | Sending a tx input | `value (8)` \|\| `tx_id (32)` \|\| `address_type (1)` \|\| `address_index (4)` \|\| `outpoint_index (1)` |
| 0x03 | Requesting for next signature | - |

#### P2 Breakdown
| P2 Value | Usage |
| --- | --- |
| 0x80 | Indicates that there will be more APDU sent by the client |
| 0x00 | Incdicates that this is the last APDU sent by the client |

`P2` value is used only if `P1 in {0x00, 0x01, 0x02}`. If `P1 = 0x03`, `P2` is ignored.

#### Flow
1. Send the first APDU `P1 = 0x00` with the version, output length and input length, change address type and index, and account (for UTXOs and change)
2. For each output (up to 2), send `P1 = 0x01` with the output CData
3. For each UTXO input send `P1 = 0x02` with the input CData. When sending the last UTXO input set `P2 = 0x00` to indicate that it is the last APDU. The signatures will later be sent back to you in the same order these inputs come in.
4. [Display] User will be able to view the transaction info and choose to `Approve` or `Reject`.
5. If approved, the first RAPDU with the signature of the first input index will be sent back to the user.
6. While `has_more` is non-zero, send the `sign_tx` APDU with `P1 = 0x03` to ask for the next signature.
7. When there are no more signatures, `has_more` in the RAPDU will be `0x00` and the context will be reset.

### Response

| Length <br/>(bytes) | SW | RData |
| --- | --- | --- |
| 100 | 0x9000 | `has_more (1)` \|\| `input_index (1)` \|\| <br/> `len(sig) (1)` \|\| `sig (64)` \|\| <br/> `len(sighash) (1)` \|\| `sighash (32)`|

#### Response Breakdown
| Data | Description |
| --- | --- |
| `has_more`* | Non-zero if there are more signatures to be sent back, `0x00` otherwise |
| `input_index` | The input index in the current transaction that this signature is for |
| `len(sig)` | The length of the signature. Always 64 bytes with Schnorr |
| `sig` | The Schnorr signature |
| `len(sighash)` | The length of the sighash. Always 32 bytes |
| `sighash` | The message (aka. sighash) that was signed. |

\* While `has_more` is non-zero, you can ask for the next signature by sending another APDU back

## SIGN_MESSAGE

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x07 | 0x00 | 0x00 | var | `address_type (1)` \|\| `address_index (4)` \|\|<br>`account (4)` \|\|<br>`message_len (1 bytes)` \|\| `message (var bytes)` |

| CData Part | Description |
| --- | --- |
| `address_type` | Either `00` for Receive Address or `01` for Change Address |
| `address_index` | Any value from `00000000` to `FFFFFFFF` |
| `account` | Any value from `80000000` to `FFFFFFFF` |
| `message_len` | How long the message is. Must be a value from `1` to `128`, inclusive |
| `message` | The message to sign |

### Response

| Length <br/>(bytes) | SW | RData |
| --- | --- | --- |
| var | 0x9000 | See Response Breakdown |

#### Response Breakdown

| Data | Description |
| --- | --- |
| `len(sig)` | The length of the signature. Always 64 bytes with Schnorr |
| `sig` | The Schnorr signature |
| `len(message_hash)` | The length of the message hash. Always 32 bytes |
| `message_hash` | The hash that was signed. |

## Status Words

| SW | SW name | Description |
| --- | --- | --- |
| 0x6985 | `SW_DENY` | Rejected by user |
| 0x6A86 | `SW_WRONG_P1P2` | Either `P1` or `P2` is incorrect |
| 0x6A87 | `SW_WRONG_DATA_LENGTH` | `Lc` or minimum APDU length is incorrect |
| 0x6D00 | `SW_INS_NOT_SUPPORTED` | No command exists with `INS` |
| 0x6E00 | `SW_CLA_NOT_SUPPORTED` | Bad `CLA` used for this application |
| 0xB000 | `SW_WRONG_RESPONSE_LENGTH` | Wrong response length (buffer size problem) |
| 0xB001 | `SW_DISPLAY_BIP32_PATH_FAIL` | BIP32 path conversion to string failed |
| 0xB002 | `SW_DISPLAY_ADDRESS_FAIL` | Address conversion to string failed |
| 0xB003 | `SW_DISPLAY_AMOUNT_FAIL` | Amount conversion to string failed |
| 0xB004 | `SW_WRONG_TX_LENGTH` | Wrong raw transaction length |
| 0xB005 | `SW_TX_PARSING_FAIL` | Failed to parse raw transaction |
| 0xB006 | `SW_TX_HASH_FAIL` | Failed to compute hash digest of raw transaction |
| 0xB007 | `SW_BAD_STATE` | Security issue with bad state |
| 0xB008 | `SW_SIGNATURE_FAIL` | Signature of raw transaction failed |
| 0xB009 | `SW_WRONG_BIP32_PURPOSE` | `Purpose` must be `44'` |
| 0xB00A | `SW_WRONG_BIP32_COIN_TYPE` | `Coin Type` must be `121337'` |
| 0xB00B | `SW_WRONG_BIP32_PATH_LEN` | Path length must be `5` |
| 0xB010 | `SW_MESSAGE_PARSING_FAIL` | Unable to parse message data |
| 0xB011 | `SW_MESSAGE_TOO_LONG` | Message len greater than max |
| 0xB012 | `SW_MESSAGE_TOO_SHORT` | Message len is 0 |
| 0xB013 | `SW_MESSAGE_ADDRESS_TYPE_FAIL` | Address type could not be parsed or is not `0`/`1` |
| 0xB014 | `SW_MESSAGE_ADDRESS_INDEX_FAIL` | Address index could not be parsed |
| 0xB015 | `SW_MESSAGE_LEN_PARSING_FAIL` | Message length could not be parsed |
| 0xB016 | `SW_MESSAGE_UNEXPECTED` | Unexpected error while parsing message |
| 0x9000 | `OK` | Success |
