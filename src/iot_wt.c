/* ***************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot_internal.h"
#include "iot_debug.h"
#include "iot_error.h"
#include "iot_util.h"
#include "iot_uuid.h"
#include "security/iot_security_crypto.h"
#include "security/iot_security_helper.h"

static iot_security_buffer_t *_iot_wt_alloc_b64_buffer(size_t plain_len)
{
	iot_security_buffer_t *b64_buf;
	unsigned char *buf;
	size_t len;

	b64_buf = (iot_security_buffer_t *)iot_os_malloc(sizeof(iot_security_buffer_t));
	if (!b64_buf) {
		IOT_ERROR("failed to malloc for b64 buffer");
		return NULL;
	}

	memset(b64_buf, 0, sizeof(iot_security_buffer_t));

	len = IOT_SECURITY_B64_ENCODE_LEN(plain_len);
	buf = (unsigned char *)iot_os_malloc(len);
	if (!buf) {
		IOT_ERROR("failed to malloc for b64");
		iot_os_free(b64_buf);
		return NULL;
	}

	b64_buf->p = buf;
	b64_buf->len = len;

	return b64_buf;
}

#if defined(STDK_IOT_CORE_WEBTOKEN_CBOR)

#include <cbor.h>

/*
 * https://tools.ietf.org/html/rfc8152#section-3.1
 */
enum {
	COSE_HEADER_ALG = 1,
	COSE_HEADER_KID = 4,
};

/*
 * https://tools.ietf.org/html/rfc8152#section-13
 */
enum {
	COSE_KEY_TYPE_OKP = 1,
	COSE_KEY_TYPE_EC2 = 2,
	COSE_KEY_TYPE_SYMMETRIC = 4,
};

enum {
	COSE_ALGORITHM_EdDSA = -8,
};

/*
 * https://tools.ietf.org/html/rfc8392#section-3.1
 */
enum {
	CWT_CLAIMS_ISS = 1,
	CWT_CLAIMS_SUB = 2,
	CWT_CLAIMS_AUD = 3,
	CWT_CLAIMS_EXP = 4,
	CWT_CLAIMS_NBF = 5,
	CWT_CLAIMS_IAT = 6,
	CWT_CLAIMS_CTI = 7,
};

static iot_error_t _iot_cwt_create_protected(iot_security_key_type_t key_type, iot_security_buffer_t *output_buf)
{
	CborEncoder root = {0};
	CborEncoder map = {0};
	int entry_num;
	unsigned char *cborbuf;
	unsigned char *tmp;
	size_t buflen = 32;
	size_t olen;

	if (!output_buf) {
		IOT_ERROR("output buffer is null");
		return IOT_ERROR_INVALID_ARGS;
	}

retry:
	buflen += 32;

	cborbuf = (unsigned char *)iot_os_malloc(buflen);
	if (cborbuf == NULL) {
		IOT_ERROR("failed to malloc for cwt");
		return IOT_ERROR_MEM_ALLOC;
	}

	memset(cborbuf, 0, buflen);

	cbor_encoder_init(&root, cborbuf, buflen, 0);

	entry_num = 1;

	cbor_encoder_create_map(&root, &map, entry_num);

	switch(key_type) {
	case IOT_SECURITY_KEY_TYPE_ED25519:
		cbor_encode_int(&map, COSE_HEADER_ALG);
		cbor_encode_negative_int(&map, -COSE_ALGORITHM_EdDSA);
		break;
	default:
		IOT_ERROR("'%d' is not a supported type", key_type);
		iot_os_free(cborbuf);
		return IOT_ERROR_WEBTOKEN_FAIL;
	}

	cbor_encoder_close_container_checked(&root, &map);

	olen = cbor_encoder_get_buffer_size(&root, cborbuf);
	if (olen < buflen) {
		tmp = (unsigned char *)iot_os_realloc(cborbuf, olen + 1);
		if (tmp) {
			cborbuf = tmp;
			cborbuf[olen] = 0;
		}
	} else {
		IOT_ERROR("allocated size is not enough (%d < %d)",
				(int)buflen, (int)olen);
		iot_os_free(cborbuf);
		if (buflen < IOT_CBOR_MAX_BUF_LEN) {
			goto retry;
		} else {
			return IOT_ERROR_WEBTOKEN_FAIL;
		}
	}

	output_buf->p = cborbuf;
	output_buf->len = olen;

	return IOT_ERROR_NONE;
}

static iot_error_t _iot_cwt_create_unprotected(CborEncoder *root, const char *sn)
{
	CborEncoder map;
	int entry_num = 1;

	cbor_encoder_create_map(root, &map, entry_num);
	cbor_encode_int(&map, COSE_HEADER_KID);
	cbor_encode_byte_string(&map, (const unsigned char *)sn, strlen(sn));
	cbor_encoder_close_container_checked(root, &map);

	return IOT_ERROR_NONE;
}

static iot_error_t _iot_cwt_create_payload(iot_security_buffer_t *output_buf)
{
	iot_error_t err;
	CborEncoder root = {0};
	CborEncoder map = {0};
	int entry_num;
	unsigned char *cborbuf;
	unsigned char *tmp;
	size_t buflen = 128;
	size_t olen;

	long time_in_sec;	/* 1559347200 is '2019-06-01 00:00:00 UTC' */
	struct iot_uuid uuid;	/* 16 bytes */
	const char *aud = "mqtts://greatgate.smartthings.com";

	if (!output_buf) {
		IOT_ERROR("output buffer is null");
		return IOT_ERROR_INVALID_ARGS;
	}

	err = iot_get_time_in_sec_by_long(&time_in_sec);
	if (err) {
		IOT_ERROR("_iot_get_time_in_sec_by_long returned error : %d", err);
		return err;
	}

	err = iot_get_random_uuid(&uuid);
	if (err) {
		IOT_ERROR("iot_get_random_uuid returned error : %d", err);
		return err;
	}

retry:
	buflen += 128;

	cborbuf = (unsigned char *)iot_os_malloc(buflen);
	if (cborbuf == NULL) {
		IOT_ERROR("failed to malloc for cwt");
		return IOT_ERROR_MEM_ALLOC;
	}

	memset(cborbuf, 0, buflen);

	cbor_encoder_init(&root, cborbuf, buflen, 0);

	entry_num = 3;

	cbor_encoder_create_map(&root, &map, entry_num);

	/* iat */
	cbor_encode_int(&map, CWT_CLAIMS_IAT);
	cbor_encode_uint(&map, time_in_sec);
	/* cti */
	cbor_encode_int(&map, CWT_CLAIMS_CTI);
	cbor_encode_byte_string(&map, uuid.id, sizeof(uuid.id));
	/* aud : optional */
	cbor_encode_int(&map, CWT_CLAIMS_AUD);
	cbor_encode_text_string(&map, aud, strlen(aud));

	cbor_encoder_close_container_checked(&root, &map);

	olen = cbor_encoder_get_buffer_size(&root, cborbuf);
	if (olen < buflen) {
		tmp = (unsigned char *)iot_os_realloc(cborbuf, olen + 1);
		if (tmp) {
			cborbuf = tmp;
			cborbuf[olen] = 0;
		}
	} else {
		IOT_ERROR("allocated size is not enough (%d < %d)",
				(int)buflen, (int)olen);
		iot_os_free(cborbuf);
		if (buflen < IOT_CBOR_MAX_BUF_LEN) {
			goto retry;
		} else {
			return IOT_ERROR_WEBTOKEN_FAIL;
		}
	}

	output_buf->p = cborbuf;
	output_buf->len = olen;

	return IOT_ERROR_NONE;
}

static iot_error_t _iot_cwt_create_signature(iot_security_context_t *security_context,
					     iot_security_buffer_t *protected_buf,
					     iot_security_buffer_t *payload_buf,
					     iot_security_buffer_t *output_buf)
{
	iot_error_t err;
	iot_security_key_type_t key_type;
	iot_security_buffer_t message_buf = {0};
	CborEncoder root = {0};
	CborEncoder array = {0};
	int array_num = 4;
	unsigned char *cborbuf;
	unsigned char *tmp;
	size_t siglen;
	size_t buflen = 128;
	size_t olen;
	const char *context = "Signature1";

	if (!security_context) {
		IOT_ERROR("security context is null");
		return IOT_ERROR_INVALID_ARGS;
	}

	if (!protected_buf || !payload_buf) {
		IOT_ERROR("input buffer is null");
		return IOT_ERROR_INVALID_ARGS;
	}

	if (!output_buf) {
		IOT_ERROR("output buffer is null");
		return IOT_ERROR_INVALID_ARGS;
	}

	err = iot_security_pk_get_key_type(security_context, &key_type);
	if (err) {
		return err;
	}

	siglen = iot_security_pk_get_signature_len(key_type);
	if (siglen == IOT_SECURITY_SIGNATURE_UNKNOWN_LEN) {
		IOT_ERROR("failed to get signature length");
		return IOT_ERROR_SECURITY_PK_KEY_TYPE;
	}

retry:
	buflen += 128;

	cborbuf = (unsigned char *)iot_os_malloc(buflen);
	if (cborbuf == NULL) {
		IOT_ERROR("failed to malloc for cwt");
		return IOT_ERROR_MEM_ALLOC;
	}

	memset(cborbuf, 0, buflen);

	cbor_encoder_init(&root, cborbuf, buflen, 0);

	cbor_encoder_create_array(&root, &array, array_num);

	cbor_encode_text_string(&array, context, strlen(context));
	cbor_encode_byte_string(&array, protected_buf->p, protected_buf->len);
	cbor_encode_byte_string(&array, NULL, 0);
	cbor_encode_byte_string(&array, payload_buf->p, payload_buf->len);

	cbor_encoder_close_container_checked(&root, &array);

	olen = cbor_encoder_get_buffer_size(&root, cborbuf);
	if (olen < buflen) {
		tmp = (unsigned char *)iot_os_realloc(cborbuf, olen + 1);
		if (tmp) {
			cborbuf = tmp;
			cborbuf[olen] = 0;
		}
	} else {
		IOT_ERROR("allocated size is not enough (%d < %d)", (int)buflen, (int)olen);
		iot_os_free(cborbuf);
		if (buflen < IOT_CBOR_MAX_BUF_LEN) {
			goto retry;
		} else {
			IOT_ERROR("cbor exceeds the maximum size");
			return IOT_ERROR_MEM_ALLOC;
		}
	}

	message_buf.p = cborbuf;
	message_buf.len = olen;

	err = iot_security_pk_sign(security_context, &message_buf, output_buf);
	if (err) {
		IOT_ERROR("iot_security_pk_sign returned error : %d", err);
		iot_os_free(cborbuf);
		return err;
	}

	iot_os_free(cborbuf);

	return IOT_ERROR_NONE;
}

static iot_error_t _iot_cwt_create(const iot_security_buffer_t *sn_buf, iot_security_buffer_t *token_buf)
{
	iot_error_t err;
	iot_security_context_t *security_context;
	iot_security_key_type_t key_type;
	CborEncoder root = {0};
	CborEncoder array = {0};
	int array_num = 4;
	iot_security_buffer_t protected_buf = { 0 };
	iot_security_buffer_t payload_buf = { 0 };
	iot_security_buffer_t signature_buf = { 0 };
	iot_security_buffer_t *cbor_b64_buf;
	unsigned char *cborbuf = NULL;
	unsigned char *tmp = NULL;
	size_t olen;
	size_t cbor_len;
	size_t buflen = 128;

	if (!sn_buf || !token_buf) {
		return IOT_ERROR_INVALID_ARGS;
	}

	security_context = iot_security_init();
	if (!security_context) {
		return IOT_ERROR_SECURITY_INIT;
	}

	err = iot_security_pk_init(security_context);
	if (err) {
		return err;
	}

	err = iot_security_pk_get_key_type(security_context, &key_type);
	if (err) {
		goto exit_failed;
	}

retry:
	buflen += 128;

	cborbuf = (unsigned char *)iot_os_malloc(buflen);
	if (cborbuf == NULL) {
		IOT_ERROR("failed to malloc for cbor");
		err = IOT_ERROR_MEM_ALLOC;
		goto exit_failed;
	}

	memset(cborbuf, 0, buflen);

	/*
	 * https://tools.ietf.org/html/rfc8152#section-4.2
	 */
	cbor_encoder_init(&root, cborbuf, buflen, 0);

	cbor_encode_tag(&root, CborCOSE_Sign1Tag);

	cbor_encoder_create_array(&root, &array, array_num);

	/* protected */
	err = _iot_cwt_create_protected(key_type, &protected_buf);
	if (err) {
		goto exit_cborbuf;
	}

	cbor_encode_byte_string(&array, protected_buf.p, protected_buf.len);

	/* unprotected */
	err = _iot_cwt_create_unprotected(&array, (const char *)sn_buf->p);
	if (err) {
		goto exit_protected;
	}

	/* payload */
	err = _iot_cwt_create_payload(&payload_buf);
	if (err) {
		goto exit_unprotected;
	}

	cbor_encode_byte_string(&array, payload_buf.p, payload_buf.len);

	/* signature */
	err = _iot_cwt_create_signature(security_context, &protected_buf, &payload_buf, &signature_buf);
	if (err) {
		goto exit_payload;
	}

	cbor_encode_byte_string(&array, signature_buf.p, signature_buf.len);

	cbor_encoder_close_container_checked(&root, &array);

	olen = cbor_encoder_get_buffer_size(&root, cborbuf);
	if (olen < buflen) {
		tmp = (unsigned char *)iot_os_realloc(cborbuf, olen + 1);
		if (tmp) {
			cborbuf = tmp;
			cborbuf[olen] = 0;
			cbor_len = olen;
		} else {
			IOT_WARN("failed to realloc for cwt");
			cbor_len = buflen;
		}
	} else {
		IOT_ERROR("allocated size is not enough (%d < %d)", (int)buflen, (int)olen);
		iot_os_free(cborbuf);
		if (buflen < IOT_CBOR_MAX_BUF_LEN) {
			iot_os_free(signature_buf.p);
			iot_os_free(payload_buf.p);
			iot_os_free(protected_buf.p);
			goto retry;
		} else {
			goto exit_signature;
		}
	}

	cbor_b64_buf = _iot_wt_alloc_b64_buffer(cbor_len);
	if (!cbor_b64_buf) {
		err = IOT_ERROR_MEM_ALLOC;
		goto exit_signature;
	}

	err = iot_security_base64_encode(cborbuf, cbor_len, cbor_b64_buf->p, cbor_b64_buf->len, &olen);
	if (err) {
		IOT_ERROR("iot_security_base64_encode returned error : %d", err);
		goto exit_cbor_b64_buf_p;
	}

	cbor_b64_buf->len = olen;
	*token_buf = *cbor_b64_buf;

	IOT_DEBUG("CWT(%d) : '%s'", olen, token_buf->p);

	err = IOT_ERROR_NONE;
	goto exit_cbor_b64_buf;

exit_cbor_b64_buf_p:
	iot_os_free(cbor_b64_buf->p);
exit_cbor_b64_buf:
	iot_os_free(cbor_b64_buf);
exit_signature:
	iot_os_free(signature_buf.p);
exit_payload:
	iot_os_free(payload_buf.p);
exit_unprotected:
exit_protected:
	iot_os_free(protected_buf.p);
exit_cborbuf:
	iot_os_free(cborbuf);
exit_failed:
	(void)iot_security_pk_deinit(security_context);
	(void)iot_security_deinit(security_context);

	return err;
}

#else /* !STDK_IOT_CORE_WEBTOKEN_CBOR */

#include <JSON.h>

static char * _iot_jwt_header_rs256(const iot_security_buffer_t *sn_buf)
{
	JSON_H *object;
	char *object_str;

	if (!sn_buf) {
		IOT_ERROR("sn buf is null");
		return NULL;
	}

	object = JSON_CREATE_OBJECT();
	if (!object) {
		IOT_ERROR("JSON_CREATE_OBJECT returned NULL");
		return NULL;
	}

	JSON_ADD_ITEM_TO_OBJECT(object, "alg", JSON_CREATE_STRING("RS256"));
	JSON_ADD_ITEM_TO_OBJECT(object, "kty", JSON_CREATE_STRING("RSA"));
	JSON_ADD_ITEM_TO_OBJECT(object, "crv", JSON_CREATE_STRING(""));
	JSON_ADD_ITEM_TO_OBJECT(object, "typ", JSON_CREATE_STRING("JWT"));
	JSON_ADD_ITEM_TO_OBJECT(object, "ver", JSON_CREATE_STRING("0.0.1"));
	JSON_ADD_ITEM_TO_OBJECT(object, "kid", JSON_CREATE_STRING((char *)sn_buf->p));

	object_str = JSON_PRINT(object);
	if (!object_str) {
		IOT_ERROR("JSON_PRINT returned NULL");
		JSON_DELETE(object);
		return NULL;
	}

	JSON_DELETE(object);

	return object_str;
}

static char * _iot_jwt_header_ed25519(const iot_security_buffer_t *sn_buf)
{
	JSON_H *object;
	char *object_str;

	if (!sn_buf) {
		IOT_ERROR("sn buf is null");
		return NULL;
	}

	object = JSON_CREATE_OBJECT();
	if (!object) {
		IOT_ERROR("JSON_CREATE_OBJECT returned NULL");
		return NULL;
	}

	JSON_ADD_ITEM_TO_OBJECT(object, "alg", JSON_CREATE_STRING("EdDSA"));
	JSON_ADD_ITEM_TO_OBJECT(object, "kty", JSON_CREATE_STRING("OKP"));
	JSON_ADD_ITEM_TO_OBJECT(object, "crv", JSON_CREATE_STRING("Ed25519"));
	JSON_ADD_ITEM_TO_OBJECT(object, "typ", JSON_CREATE_STRING("JWT"));
	JSON_ADD_ITEM_TO_OBJECT(object, "ver", JSON_CREATE_STRING("0.0.1"));
	JSON_ADD_ITEM_TO_OBJECT(object, "kid", JSON_CREATE_STRING((char *)sn_buf->p));

	object_str = JSON_PRINT(object);
	if (!object_str) {
		IOT_ERROR("JSON_PRINT returned NULL");
		JSON_DELETE(object);
		return NULL;
	}

	JSON_DELETE(object);

	return object_str;
}


static char * _iot_jwt_create_header(const iot_security_buffer_t *sn_buf, iot_security_key_type_t key_type)
{
	char *object_str;

	switch(key_type) {
	case IOT_SECURITY_KEY_TYPE_RSA2048:
		object_str = _iot_jwt_header_rs256(sn_buf);
		break;
	case IOT_SECURITY_KEY_TYPE_ED25519:
		object_str = _iot_jwt_header_ed25519(sn_buf);
		break;
	default:
		IOT_ERROR("pubkey type (%d) is not supported", key_type);
		object_str = NULL;
		break;
	}

	return object_str;
}

static iot_error_t _iot_jwt_create_b64h(const iot_security_buffer_t *sn_buf,
					iot_security_key_type_t key_type,
					iot_security_buffer_t *b64h_buf)
{
	iot_error_t err = IOT_ERROR_NONE;
	iot_security_buffer_t *b64_buf;
	char *hdr;
	size_t hdr_len;
	size_t out_len;

	if (!sn_buf || !b64h_buf) {
		IOT_ERROR("params is NULL");
		return IOT_ERROR_INVALID_ARGS;
	}

	hdr = _iot_jwt_create_header(sn_buf, key_type);
	if (!hdr) {
		IOT_ERROR("_iot_jwt_create_header returned NULL");
		err = IOT_ERROR_WEBTOKEN_FAIL;
		goto exit;
	}

	hdr_len = strlen(hdr);

	b64_buf = _iot_wt_alloc_b64_buffer(hdr_len);
	if (!b64_buf) {
		goto exit_hdr;
	}

	err = iot_security_base64_encode((unsigned char *)hdr, hdr_len, b64_buf->p, b64_buf->len, &out_len);
	if (err) {
		IOT_ERROR("iot_security_base64_encode = %d", err);
		goto exit_b64_buf_p;
	}

	b64h_buf->p = b64_buf->p;
	b64h_buf->len = out_len;
	goto exit_b64_buf;

exit_b64_buf_p:
	iot_os_free(b64_buf->p);
exit_b64_buf:
	iot_os_free(b64_buf);
exit_hdr:
	free(hdr);
exit:
	return err;
}

static char * _iot_jwt_create_payload(void)
{
	iot_error_t err;
	JSON_H *object;
	char *object_str;
	char time_in_sec[16]; /* 1559347200 is '2019-06-01 00:00:00 UTC' */
	char uuid_str[40];    /* 4066c24f-cd48-4e92-a538-362e74337c7f */
	struct iot_uuid uuid;

	err = iot_get_time_in_sec(time_in_sec, sizeof(time_in_sec));
	if (err) {
		IOT_ERROR("_iot_get_time_in_sec returned error : %d", err);
		return NULL;
	}

	err = iot_get_random_uuid(&uuid);
	if (err) {
		IOT_ERROR("iot_get_random_uuid returned error : %d", err);
		return NULL;
	}

	err = iot_util_convert_uuid_str(&uuid, uuid_str, sizeof(uuid_str));
	if (err) {
		IOT_ERROR("iot_util_convert_uuid_str returned error : %d", err);
		return NULL;
	}


	object = JSON_CREATE_OBJECT();
	if (!object) {
		IOT_ERROR("JSON_CREATE_OBJECT returned NULL");
		return NULL;
	}

	JSON_ADD_ITEM_TO_OBJECT(object, "iat", JSON_CREATE_STRING(time_in_sec));
	JSON_ADD_ITEM_TO_OBJECT(object, "jti", JSON_CREATE_STRING(uuid_str));

	object_str = JSON_PRINT(object);
	if (!object_str) {
		IOT_ERROR("JSON_PRINT returned NULL");
		JSON_DELETE(object);
		return NULL;
	}

	JSON_DELETE(object);

	return object_str;
}

static iot_error_t _iot_jwt_create_b64p(iot_security_buffer_t *b64p_buf)
{
	iot_error_t err = IOT_ERROR_NONE;
	iot_security_buffer_t *b64_buf;
	char *payload;
	size_t payload_len;
	size_t out_len;

	payload = _iot_jwt_create_payload();
	if (!payload) {
		IOT_ERROR("_iot_jwt_create_payload returned NULL");
		err = IOT_ERROR_WEBTOKEN_FAIL;
		goto exit;
	}

	payload_len = strlen(payload);

	b64_buf = _iot_wt_alloc_b64_buffer(payload_len);
	if (!b64_buf) {
		goto exit_payload;
	}

	err = iot_security_base64_encode((unsigned char *)payload, payload_len, b64_buf->p, b64_buf->len, &out_len);
	if (err) {
		IOT_ERROR("iot_security_base64_encode returned error : %d", err);
		goto exit_b64_buf_p;
	}

	b64p_buf->p = b64_buf->p;
	b64p_buf->len = out_len;
	goto exit_b64_buf;

exit_b64_buf_p:
	iot_os_free(b64_buf->p);
exit_b64_buf:
	iot_os_free(b64_buf);
exit_payload:
	free(payload);
exit:
	return err;
}

static iot_error_t _iot_jwt_create_b64s(iot_security_context_t *security_context,
                                        unsigned char *b64hp, size_t hp_len,
                                        iot_security_buffer_t *b64s_buf)
{
	iot_error_t err;
	iot_security_buffer_t b64hp_buf = { 0 };
	iot_security_buffer_t sig_buf = { 0 };
	iot_security_buffer_t *sig_b64_buf;
	size_t out_len;

	b64hp_buf.p = b64hp;
	b64hp_buf.len = hp_len;
	err = iot_security_pk_sign(security_context, &b64hp_buf, &sig_buf);
	if (err) {
		IOT_ERROR("iot_security_pk_sign returned error : %d", err);
		goto exit;
	}

	sig_b64_buf = _iot_wt_alloc_b64_buffer(sig_buf.len);
	if (!sig_b64_buf) {
		goto exit_sig;
	}

	err = iot_security_base64_encode(sig_buf.p, sig_buf.len, sig_b64_buf->p, sig_b64_buf->len, &out_len);
	if (err) {
		IOT_ERROR("iot_security_base64_encode returned error : %d", err);
		goto exit_b64_buf_p;
	}

	b64s_buf->p = sig_b64_buf->p;
	b64s_buf->len = out_len;
	goto exit_b64_buf;

exit_b64_buf_p:
	iot_os_free(sig_b64_buf->p);
exit_b64_buf:
	iot_os_free(sig_b64_buf);
exit_sig:
	iot_os_free(sig_buf.p);
exit:
	return err;
}

static iot_error_t _iot_jwt_create(const iot_security_buffer_t *sn_buf, iot_security_buffer_t *token_buf)
{
	iot_error_t err;
	iot_security_context_t *security_context;
	iot_security_key_type_t key_type;
	iot_security_buffer_t b64h_buf = { 0 };
	iot_security_buffer_t b64p_buf = { 0 };
	iot_security_buffer_t b64s_buf = { 0 };
	unsigned char *tmp;
	size_t sig_len;
	size_t token_len;
	size_t written = 0;

	if (!sn_buf || !token_buf) {
		return IOT_ERROR_INVALID_ARGS;
	}

	security_context = iot_security_init();
	if (!security_context) {
		return IOT_ERROR_SECURITY_INIT;
	}

	err = iot_security_pk_init(security_context);
	if (err) {
		return err;
	}

	err = iot_security_pk_get_key_type(security_context, &key_type);
	if (err) {
		goto exit_payload;
	}

	/* b64h = b64(header) */

	err = _iot_jwt_create_b64h(sn_buf, key_type, &b64h_buf);
	if (err) {
		IOT_ERROR("_iot_jwt_create_b64h = %d", err);
		goto exit;
	}

	/* b64p = b64(payload) */

	err = _iot_jwt_create_b64p(&b64p_buf);
	if (err) {
		IOT_ERROR("_iot_jwt_create_b64p returned error : %d", err);
		goto exit_header;
	}

	/* b64h.b64 */

	sig_len = iot_security_pk_get_signature_len(key_type);

	token_len = b64h_buf.len + b64p_buf.len + IOT_SECURITY_B64_ENCODE_LEN(sig_len) + 3;

	tmp = (unsigned char *)iot_os_malloc(token_len);
	if (tmp == NULL) {
		IOT_ERROR("malloc returned NULL");
		err = IOT_ERROR_MEM_ALLOC;
		goto exit_payload;
	}

	memcpy(tmp, b64h_buf.p, b64h_buf.len);
	written += b64h_buf.len;

	tmp[written++] = '.';

	memcpy(tmp + written, b64p_buf.p, b64p_buf.len);
	written += b64p_buf.len;

	/* b64s = b64(sign(sha256(b64h.b64p))) */

	err = _iot_jwt_create_b64s(security_context, tmp, written, &b64s_buf);
	if (err) {
		IOT_ERROR("_iot_jwt_create_b64s returned error : %d", err);
		iot_os_free(tmp);
		goto exit_payload;
	}

	/* token = b64h.b64p.b64s */

	tmp[written++] = '.';

	memcpy(tmp + written, b64s_buf.p, b64s_buf.len);
	written += b64s_buf.len;

	tmp[written] = '\0';

	IOT_DEBUG("token: %s (%d)", tmp, written);

	token_buf->p = tmp;
	token_buf->len = written;
	err = IOT_ERROR_NONE;
	goto exit_signature;

exit_signature:
	iot_os_free(b64s_buf.p);
exit_payload:
	iot_os_free(b64p_buf.p);
exit_header:
	iot_os_free(b64h_buf.p);
exit:
	(void)iot_security_pk_deinit(security_context);
	(void)iot_security_deinit(security_context);

	return err;
}

#endif /* STDK_IOT_CORE_WEBTOKEN_CBOR */

iot_error_t iot_wt_create(const iot_security_buffer_t *sn_buf, iot_security_buffer_t *token_buf)
{
#if defined(STDK_IOT_CORE_WEBTOKEN_CBOR)
	return _iot_cwt_create(sn_buf, token_buf);
#else
	return _iot_jwt_create(sn_buf, token_buf);
#endif
}
