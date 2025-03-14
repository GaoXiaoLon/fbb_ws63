/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gmssl/zuc.h>


int main(void)
{
	ZUC_CTX zuc_ctx;
	unsigned char key[16] = {
		0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
		0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
	};
	unsigned char iv[16] = {
		0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
		0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
	};
	unsigned char inbuf[1024];
	unsigned char outbuf[1024 + 32];
	ssize_t inlen;
	size_t outlen;

	if (zuc_encrypt_init(&zuc_ctx, key, iv) != 1) {
		fprintf(stderr, "%s %d: error\n", __FILE__, __LINE__);
		return 1;
	}
	while ((inlen = fread(inbuf, 1, sizeof(inbuf), stdin)) > 0) {
		if (zuc_encrypt_update(&zuc_ctx, inbuf, inlen, outbuf, &outlen) != 1) {
			fprintf(stderr, "%s %d: error\n", __FILE__, __LINE__);
			return 1;
		}
		fwrite(outbuf, 1, outlen, stdout);
	}
	if (zuc_encrypt_finish(&zuc_ctx, outbuf, &outlen) != 1) {
		fprintf(stderr, "%s %d: error\n", __FILE__, __LINE__);
		return 1;
	}
	fwrite(outbuf, 1, outlen, stdout);

	return 0;
}
