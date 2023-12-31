/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023, Analog Devices, Inc. All rights reserved.
 */

#include <errno.h>
#include <libadiotp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>

struct adi_otp {
	TEEC_Context ctx;
	TEEC_Session session;
};

static int adi_otp_check_version(struct adi_otp *otp);

struct adi_otp *adi_otp_open(void) {
	TEEC_UUID uuid = PTA_ADI_OTP_UUID;
	uint32_t origin;
	struct adi_otp *ret;
	TEEC_Result res;

	ret = calloc(sizeof(*ret), 1);
	if (!ret)
		return NULL;

	res = TEEC_InitializeContext(NULL, &ret->ctx);
	if (TEEC_SUCCESS != res)
		goto cleanup;

	res = TEEC_OpenSession(&ret->ctx, &ret->session, &uuid, TEEC_LOGIN_PUBLIC,
		NULL, NULL, &origin);
	if (TEEC_SUCCESS != res)
		goto finalize;

	res = adi_otp_check_version(ret);
	if (res)
		goto finalize;

	return ret;

finalize:
	TEEC_FinalizeContext(&ret->ctx);

cleanup:
	free(ret);

	return NULL;
}

int adi_otp_get_version(struct adi_otp *otp, int *major, int *minor) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_OUTPUT,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE
	);

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_VERSION, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP read failed, ret = %d\n", res);
		return (int) res;
	}

	*major = op.params[0].value.a;
	*minor = op.params[0].value.b;
	return TEEC_SUCCESS;
}

int adi_otp_check_version(struct adi_otp *otp) {
	int ret;
	int major, minor;

	ret = adi_otp_get_version(otp, &major, &minor);
	if (ret)
		return ret;

	if (major != ADI_OTP_MAJOR) {
		fprintf(stderr, "OTP major version mismatch: pTA version = %d, library version = %d\n", major, ADI_OTP_MAJOR);
		return -EINVAL;
	}

	if (minor < ADI_OTP_MINOR) {
		fprintf(stderr, "OTP minor version too old: pTA version = %d, library version = %d\n", minor, ADI_OTP_MINOR);
		return -EINVAL;
	}

	return 0;
}

void adi_otp_close(struct adi_otp *otp) {
	if (!otp)
		return;

	TEEC_CloseSession(&otp->session);
	TEEC_FinalizeContext(&otp->ctx);
}

int adi_otp_read(struct adi_otp *otp, uint32_t id, void *buf, uint32_t *len) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_MEMREF_TEMP_OUTPUT,
		TEEC_NONE,
		TEEC_NONE
	);

	op.params[0].value.a = id;
	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = *len;

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_READ, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP read failed, ret = %d\n", res);
		return (int) res;
	}

	// Save actual size read
	*len = op.params[1].tmpref.size;
	return 0;
}

int adi_otp_write(struct adi_otp *otp, uint32_t id, void *buf, uint32_t len) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_MEMREF_TEMP_INPUT,
		TEEC_NONE,
		TEEC_NONE
	);

	op.params[0].value.a = id;
	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = len;

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_WRITE, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP write failed, ret = %d\n", res);
		return (int) res;
	}

	return 0;
}

int adi_otp_invalidate(struct adi_otp *otp, uint32_t id) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE
	);

	op.params[0].value.a = id;

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_INVALIDATE, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP invalidate failed, ret = %d\n", res);
		return (int) res;
	}

	return 0;
}

int adi_otp_is_valid(struct adi_otp *otp, uint32_t id, uint32_t *valid) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_VALUE_OUTPUT,
		TEEC_NONE,
		TEEC_NONE
	);

	op.params[0].value.a = id;

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_IS_VALID, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP is_valid failed, ret = %d\n", res);
		return (int) res;
	}

	*valid = op.params[1].value.a;

	return 0;
}

int adi_otp_is_written(struct adi_otp *otp, uint32_t id, uint32_t *written) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_VALUE_OUTPUT,
		TEEC_NONE,
		TEEC_NONE
	);

	op.params[0].value.a = id;

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_IS_WRITTEN, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP is_written failed, ret = %d\n", res);
		return (int) res;
	}

	*written = op.params[1].value.a;

	return 0;
}

int adi_otp_is_locked(struct adi_otp *otp, uint32_t *locked) {
	return adi_otp_is_written(otp, ADI_OTP_ID_lock, locked);
}

int adi_otp_lock(struct adi_otp *otp) {
	uint32_t origin;
	TEEC_Result res;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE
	);

	res = TEEC_InvokeCommand(&otp->session, ADI_OTP_CMD_LOCK, &op, &origin);
	if (TEEC_SUCCESS != res) {
		fprintf(stderr, "OTP lock failed, ret = %d\n", res);
		return (int) res;
	}

	return 0;
}

